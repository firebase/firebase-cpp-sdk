/*
 * Copyright 2016 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "auth/src/include/firebase/auth.h"

#include <assert.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "app/src/app_state.h"
#include "app/src/assert.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/include/firebase/version.h"
#include "app/src/log.h"
#include "app/src/no_destructor.h"
#include "app/src/semaphore.h"
#include "app/src/service_interfaces.h"
#include "app/src/util.h"
#include "auth/src/common.h"
#include "auth/src/data.h"

// Register the module initializer.
FIREBASE_APP_REGISTER_CALLBACKS(
    auth, { return ::firebase::kInitResultSuccess; }, false);

namespace firebase {
namespace auth {

DEFINE_FIREBASE_VERSION_STRING(FirebaseAuth);

namespace {

std::atomic<int> g_live_auth_count{0};

// Protects AuthStateListener::auths_ and IdTokenListener::auths_ across
// multiple App instances and listener destructors. Lock ordering is strictly
// AuthData::listeners_mutex -> ListenerAuthsMutex() (leaf lock).
std::mutex& ListenerAuthsMutex() {
  static ::firebase::internal::NoDestructor<std::mutex> mutex;
  return *mutex;
}

template <typename T>
bool PushBackIfMissing(const T& entry, std::vector<T>* v) {
  auto it = std::find(v->begin(), v->end(), entry);
  if (it != v->end()) return false;
  v->push_back(entry);
  return true;
}

void PruneAndRemoveWeak(const AuthData* target,
                        std::vector<std::weak_ptr<AuthData>>* auths,
                        std::vector<std::shared_ptr<AuthData>>* keep_alive) {
  auths->erase(
      std::remove_if(auths->begin(), auths->end(),
                     [target, keep_alive](const std::weak_ptr<AuthData>& w) {
                       auto locked = w.lock();
                       if (!locked) return true;
                       const bool match = (locked.get() == target);
                       keep_alive->push_back(std::move(locked));
                       return match;
                     }),
      auths->end());
}

bool AddWeakIfMissing(const std::shared_ptr<AuthData>& auth_data,
                      std::vector<std::weak_ptr<AuthData>>* auths,
                      std::vector<std::shared_ptr<AuthData>>* keep_alive) {
  auths->erase(std::remove_if(auths->begin(), auths->end(),
                              [](const std::weak_ptr<AuthData>& w) {
                                return w.expired();
                              }),
               auths->end());
  for (const auto& w : *auths) {
    auto locked = w.lock();
    if (locked) {
      const bool match = (locked.get() == auth_data.get());
      keep_alive->push_back(std::move(locked));
      if (match) return false;
    }
  }
  auths->push_back(auth_data);
  return true;
}

template <typename T>
bool ReplaceEntryWithBack(const T& entry, std::vector<T>* v) {
  auto it = std::find(v->begin(), v->end(), entry);
  if (it == v->end()) return false;

  if (&(*it) != &(v->back())) {
    *it = std::move(v->back());
  }
  v->pop_back();
  return true;
}

template <typename T>
inline bool VectorContains(const T& entry, const std::vector<T>& v) {
  return std::find(v.begin(), v.end(), entry) != v.end();
}

}  // namespace

AuthData::AuthData(::firebase::internal::AppState& state,
                   void* platform_auth_impl)
    : app_state(state),
      future_impl(kNumAuthFunctions),
      auth_impl(platform_auth_impl) {
  g_live_auth_count.fetch_add(1, std::memory_order_relaxed);
  static const char* kApiIdentifier = "Auth";
  future_api_id = CreateApiIdentifier(kApiIdentifier, this);
  InitPlatformAuth(this);
}

AuthData::~AuthData() {
  {
    MutexLock destructing_lock(destructing_mutex);
    destructing = true;
  }
  MarkPersistentCacheLoaded();

  ClearListeners();

  // Destroy platform-specific resources (worker threads, JNI/ObjC refs, etc.).
  // Pending futures and in-flight OnCompletion callbacks are backed by a
  // shared FutureImplCore and cancelled safely when `future_impl` destructs.
  DestroyPlatformAuth(this);

  ClearUserInfos(this);

  if (g_live_auth_count.fetch_sub(1, std::memory_order_acq_rel) == 1) {
    CleanupCredentialFutureImpl();
  }

  auth_impl = nullptr;
  user_impl = nullptr;
  listener_impl = nullptr;
  id_token_listener_impl = nullptr;
  phone_auth_provider_data = nullptr;
}

Auth AuthData::MakeAuthHandle() {
  auto state = app_state.weak_from_this().lock();
  if (!state) return Auth();
  return Auth(std::shared_ptr<AuthData>(std::move(state), this));
}

User AuthData::MakeCurrentUserHandle() {
  auto state = app_state.weak_from_this().lock();
  if (!state) return User();
  MutexLock lock(future_impl.mutex());
  if (user_impl == nullptr) return User();
  return User(std::weak_ptr<AuthData>(
      std::shared_ptr<AuthData>(std::move(state), this)));
}

User AuthData::MakeStrongCurrentUserHandle() {
  auto state = app_state.weak_from_this().lock();
  if (!state) return User();
  MutexLock lock(future_impl.mutex());
  if (user_impl == nullptr) return User();
  return User(std::shared_ptr<AuthData>(std::move(state), this));
}

void AuthData::AddInternalAuthStateListener(AuthStateListener* listener) {
  if (!listener) return;
  MutexLock lock(listeners_mutex);
  PushBackIfMissing(listener, &listeners);
}

void AuthData::RemoveInternalAuthStateListener(AuthStateListener* listener) {
  if (!listener) return;
  MutexLock lock(listeners_mutex);
  ReplaceEntryWithBack(listener, &listeners);
}

void AuthData::AddInternalIdTokenListener(IdTokenListener* listener) {
  if (!listener) return;
  MutexLock lock(listeners_mutex);
  PushBackIfMissing(listener, &id_token_listeners);
}

void AuthData::RemoveInternalIdTokenListener(IdTokenListener* listener) {
  if (!listener) return;
  MutexLock lock(listeners_mutex);
  ReplaceEntryWithBack(listener, &id_token_listeners);
}

void AuthData::ClearListeners() {
  MutexLock lock(listeners_mutex);
  listeners.clear();
  id_token_listeners.clear();
}

Auth::Auth(std::shared_ptr<AuthData> auth_data)
    : auth_data_(std::move(auth_data)) {}

// static
Auth Auth::GetAuth(const App& app, InitResult* init_result_out) {
  auto state = ::firebase::internal::AppState::FromApp(app);
  if (!state) {
    if (init_result_out != nullptr) {
      *init_result_out = kInitResultFailedMissingDependency;
    }
    return Auth();
  }

  bool created_new = false;
  auto data = state->GetOrCreateService<AuthData>(
      &::firebase::internal::kAuthServiceTag, "",
      [&app, &created_new](::firebase::internal::AppState& app_state)
          -> std::shared_ptr<AuthData> {
        void* auth_impl = CreatePlatformAuth(app);
        if (!auth_impl) return nullptr;
        created_new = true;
        LogDebug("Creating Auth for App %s", app_state.name().c_str());
        auto created = std::make_shared<AuthData>(app_state, auth_impl);
        // Start async persistent cache loading BEFORE publishing AuthData so
        // pre-existing AppState subscribers notified on attach never wait on a
        // load that has not yet been scheduled.
        StartPlatformAuth(created);
        return created;
      });

  if (!data) {
    if (init_result_out != nullptr) {
      *init_result_out = kInitResultFailedMissingDependency;
    }
    return Auth();
  }

  Auth auth(std::move(data));
  if (!created_new) {
    // Log heartbeat data when instance getters are called on an existing
    // instance.
    LogHeartbeat(auth);
  }
  if (init_result_out != nullptr) *init_result_out = kInitResultSuccess;
  return auth;
}

App Auth::app() const {
  return auth_data_ ? auth_data_->app_state.TryMakeAppHandle() : App();
}

void Auth::AddAuthStateListener(AuthStateListener* listener) {
  if (!auth_data_ || !listener) return;
  bool should_notify = false;
  // Keep locked AuthData refs alive until after releasing mutexes so another
  // AppState's destructor can never run while holding listeners_mutex.
  std::vector<std::shared_ptr<AuthData>> keep_alive;
  {
    MutexLock lock(auth_data_->listeners_mutex);
    const bool listener_added =
        PushBackIfMissing(listener, &auth_data_->listeners);
    bool auth_added = false;
    {
      std::lock_guard<std::mutex> auths_lock(ListenerAuthsMutex());
      auth_added = AddWeakIfMissing(auth_data_, &listener->auths_, &keep_alive);
    }
    FIREBASE_ASSERT_RETURN_VOID(listener_added == auth_added);
    (void)auth_added;

    should_notify =
        listener_added && !auth_data_->persistent_cache_load_pending;
  }
  keep_alive.clear();
  if (should_notify) {
    listener->OnAuthStateChanged(*this);
  }
}

void Auth::AddIdTokenListener(IdTokenListener* listener) {
  if (!auth_data_ || !listener) return;
  bool listener_added = false;
  bool should_notify = false;
  std::vector<std::shared_ptr<AuthData>> keep_alive;
  {
    MutexLock lock(auth_data_->listeners_mutex);
    listener_added =
        PushBackIfMissing(listener, &auth_data_->id_token_listeners);
    bool auth_added = false;
    {
      std::lock_guard<std::mutex> auths_lock(ListenerAuthsMutex());
      auth_added = AddWeakIfMissing(auth_data_, &listener->auths_, &keep_alive);
    }
    FIREBASE_ASSERT_RETURN_VOID(listener_added == auth_added);
    (void)auth_added;

    should_notify =
        listener_added && !auth_data_->persistent_cache_load_pending;
  }
  keep_alive.clear();
  if (should_notify) {
    listener->OnIdTokenChanged(*this);
  }
  if (listener_added) {
    EnableTokenAutoRefresh(auth_data_.get());
  }
}

void Auth::RemoveAuthStateListener(AuthStateListener* listener) {
  if (!auth_data_ || !listener) return;
  std::vector<std::shared_ptr<AuthData>> keep_alive;
  {
    MutexLock lock(auth_data_->listeners_mutex);
    ReplaceEntryWithBack(listener, &auth_data_->listeners);
    std::lock_guard<std::mutex> auths_lock(ListenerAuthsMutex());
    PruneAndRemoveWeak(auth_data_.get(), &listener->auths_, &keep_alive);
  }
}

void Auth::RemoveIdTokenListener(IdTokenListener* listener) {
  if (!auth_data_ || !listener) return;
  bool removed = false;
  std::vector<std::shared_ptr<AuthData>> keep_alive;
  {
    MutexLock lock(auth_data_->listeners_mutex);
    removed = ReplaceEntryWithBack(listener, &auth_data_->id_token_listeners);
    std::lock_guard<std::mutex> auths_lock(ListenerAuthsMutex());
    PruneAndRemoveWeak(auth_data_.get(), &listener->auths_, &keep_alive);
  }
  keep_alive.clear();
  if (removed) {
    DisableTokenAutoRefresh(auth_data_.get());
  }
}

AuthStateListener::~AuthStateListener() {
  std::vector<std::weak_ptr<AuthData>> auths_copy;
  {
    std::lock_guard<std::mutex> auths_lock(ListenerAuthsMutex());
    auths_copy.swap(auths_);
  }
  for (const auto& weak_auth : auths_copy) {
    if (auto data = weak_auth.lock()) {
      data->MakeAuthHandle().RemoveAuthStateListener(this);
    }
  }
}

IdTokenListener::~IdTokenListener() {
  std::vector<std::weak_ptr<AuthData>> auths_copy;
  {
    std::lock_guard<std::mutex> auths_lock(ListenerAuthsMutex());
    auths_copy.swap(auths_);
  }
  for (const auto& weak_auth : auths_copy) {
    if (auto data = weak_auth.lock()) {
      data->MakeAuthHandle().RemoveIdTokenListener(this);
    }
  }
}

void NotifyAuthStateListeners(AuthData* auth_data) {
  Auth auth_handle = auth_data->MakeAuthHandle();
  if (!auth_handle.is_valid()) return;

  std::vector<AuthStateListener*> listeners;
  {
    MutexLock lock(auth_data->listeners_mutex);
    auth_data->persistent_cache_load_pending = false;
    listeners = auth_data->listeners;
  }
  // Wake any current_user() waiters BEFORE invoking cross-product or user
  // callbacks so callbacks querying current_user() cannot self-deadlock.
  auth_data->MarkPersistentCacheLoaded();

  // Notify cross-product internal subscribers first (outside listeners_mutex)
  // to preserve internal-before-user ordering across platforms.
  auth_data->app_state.NotifyAuthStateChanged();

  LogDebug("Auth state changed. Notifying %zu listeners.", listeners.size());

  for (auto* listener : listeners) {
    {
      MutexLock lock(auth_data->listeners_mutex);
      if (!VectorContains(listener, auth_data->listeners)) continue;
    }
    listener->OnAuthStateChanged(auth_handle);
  }
}

void NotifyIdTokenListeners(AuthData* auth_data) {
  Auth auth_handle = auth_data->MakeAuthHandle();
  if (!auth_handle.is_valid()) return;

  std::vector<IdTokenListener*> listeners;
  {
    MutexLock lock(auth_data->listeners_mutex);
    auth_data->persistent_cache_load_pending = false;
    listeners = auth_data->id_token_listeners;
  }
  auth_data->MarkPersistentCacheLoaded();
  LogDebug("ID token changed. Notifying %zu listeners.", listeners.size());

  for (auto* listener : listeners) {
    {
      MutexLock lock(auth_data->listeners_mutex);
      if (!VectorContains(listener, auth_data->id_token_listeners)) continue;
    }
    listener->OnIdTokenChanged(auth_handle);
  }
}

void NotifyAllListeners(AuthData* auth_data) {
  Auth auth_handle = auth_data->MakeAuthHandle();
  if (!auth_handle.is_valid()) return;

  std::vector<AuthStateListener*> auth_listeners;
  std::vector<IdTokenListener*> token_listeners;
  {
    MutexLock lock(auth_data->listeners_mutex);
    auth_data->persistent_cache_load_pending = false;
    auth_listeners = auth_data->listeners;
    token_listeners = auth_data->id_token_listeners;
  }
  // Wake any current_user() waiters BEFORE invoking cross-product or user
  // callbacks so callbacks querying current_user() cannot self-deadlock.
  auth_data->MarkPersistentCacheLoaded();

  auth_data->app_state.NotifyAuthStateChanged();

  LogDebug("Auth state changed. Notifying %zu listeners.",
           auth_listeners.size());
  for (auto* listener : auth_listeners) {
    {
      MutexLock lock(auth_data->listeners_mutex);
      if (!VectorContains(listener, auth_data->listeners)) continue;
    }
    listener->OnAuthStateChanged(auth_handle);
  }

  LogDebug("ID token changed. Notifying %zu listeners.",
           token_listeners.size());
  for (auto* listener : token_listeners) {
    {
      MutexLock lock(auth_data->listeners_mutex);
      if (!VectorContains(listener, auth_data->id_token_listeners)) continue;
    }
    listener->OnIdTokenChanged(auth_handle);
  }
}

AUTH_RESULT_FN(Auth, FetchProvidersForEmail, Auth::FetchProvidersResult)
AUTH_RESULT_FN(Auth, SendPasswordResetEmail, void)
AUTH_RESULT_FN(Auth, SignInWithCustomToken, AuthResult)
AUTH_RESULT_FN(Auth, SignInAndRetrieveDataWithCredential, AuthResult)
AUTH_RESULT_FN(Auth, SignInAnonymously, AuthResult)
AUTH_RESULT_FN(Auth, SignInWithEmailAndPassword, AuthResult)
AUTH_RESULT_FN(Auth, CreateUserWithEmailAndPassword, AuthResult)

}  // namespace auth
}  // namespace firebase
