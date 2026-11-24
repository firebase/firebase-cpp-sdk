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

template <typename T>
bool PushBackIfMissing(const T& entry, std::vector<T>* v) {
  auto it = std::find(v->begin(), v->end(), entry);
  if (it != v->end()) return false;
  v->push_back(entry);
  return true;
}

void PruneAndRemoveWeak(const AuthData* target,
                        std::vector<std::weak_ptr<AuthData>>* auths) {
  auths->erase(
      std::remove_if(auths->begin(), auths->end(),
                     [target](const std::weak_ptr<AuthData>& w) {
                       auto locked = w.lock();
                       return !locked || locked.get() == target;
                     }),
      auths->end());
}

bool AddWeakIfMissing(const std::shared_ptr<AuthData>& auth_data,
                      std::vector<std::weak_ptr<AuthData>>* auths) {
  // Prune any expired entries first.
  auths->erase(std::remove_if(auths->begin(), auths->end(),
                              [](const std::weak_ptr<AuthData>& w) {
                                return w.expired();
                              }),
               auths->end());
  for (const auto& w : *auths) {
    if (w.lock().get() == auth_data.get()) {
      return false;
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

  // Wait for any pending futures in flight before tearing down platform state.
  while (!future_impl.IsSafeToDelete()) {
    internal::Sleep(100);
  }

  ClearListeners();

  // Destroy platform-specific resources (worker threads, JNI/ObjC refs, etc.).
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
  auto handle = app_state.FindServiceHandle<AuthData>(
      &::firebase::internal::kAuthServiceTag);
  return Auth(std::move(handle));
}

User AuthData::MakeCurrentUserHandle() {
  auto handle = app_state.FindServiceHandle<AuthData>(
      &::firebase::internal::kAuthServiceTag);
  if (!handle) return User();
  MutexLock lock(future_impl.mutex());
  if (user_impl == nullptr) return User();
  return User(std::weak_ptr<AuthData>(handle));
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
        return std::make_shared<AuthData>(app_state, auth_impl);
      });

  if (!data) {
    if (init_result_out != nullptr) {
      *init_result_out = kInitResultFailedMissingDependency;
    }
    return Auth();
  }

  if (created_new) {
    StartPlatformAuth(data);
  }

  Auth auth(std::move(data));
  if (!created_new) {
    // Log heartbeat data when instance getters are called on an existing
    // instance. See go/firebase-platform-logging-design for more information.
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
  MutexLock lock(auth_data_->listeners_mutex);
  const bool listener_added =
      PushBackIfMissing(listener, &auth_data_->listeners);
  const bool auth_added = AddWeakIfMissing(auth_data_, &listener->auths_);
  FIREBASE_ASSERT_RETURN_VOID(listener_added == auth_added);
  (void)auth_added;

  // If the listener is registered successfully and persistent cache has been
  // loaded, trigger OnAuthStateChanged() immediately. Otherwise, wait until
  // the cache is loaded.
  if (listener_added && !auth_data_->persistent_cache_load_pending) {
    listener->OnAuthStateChanged(*this);
  }
}

void Auth::AddIdTokenListener(IdTokenListener* listener) {
  if (!auth_data_ || !listener) return;
  MutexLock lock(auth_data_->listeners_mutex);
  const bool listener_added =
      PushBackIfMissing(listener, &auth_data_->id_token_listeners);
  const bool auth_added = AddWeakIfMissing(auth_data_, &listener->auths_);
  FIREBASE_ASSERT_RETURN_VOID(listener_added == auth_added);
  (void)auth_added;

  if (listener_added) {
    if (!auth_data_->persistent_cache_load_pending) {
      listener->OnIdTokenChanged(*this);
    }
    EnableTokenAutoRefresh(auth_data_.get());
  }
}

void Auth::RemoveAuthStateListener(AuthStateListener* listener) {
  if (!auth_data_ || !listener) return;
  MutexLock lock(auth_data_->listeners_mutex);
  ReplaceEntryWithBack(listener, &auth_data_->listeners);
  PruneAndRemoveWeak(auth_data_.get(), &listener->auths_);
}

void Auth::RemoveIdTokenListener(IdTokenListener* listener) {
  if (!auth_data_ || !listener) return;
  bool removed = false;
  {
    MutexLock lock(auth_data_->listeners_mutex);
    removed = ReplaceEntryWithBack(listener, &auth_data_->id_token_listeners);
    PruneAndRemoveWeak(auth_data_.get(), &listener->auths_);
  }
  if (removed) {
    DisableTokenAutoRefresh(auth_data_.get());
  }
}

AuthStateListener::~AuthStateListener() {
  std::vector<std::weak_ptr<AuthData>> auths_copy;
  auths_copy.swap(auths_);
  for (const auto& weak_auth : auths_copy) {
    if (auto data = weak_auth.lock()) {
      data->MakeAuthHandle().RemoveAuthStateListener(this);
    }
  }
}

IdTokenListener::~IdTokenListener() {
  std::vector<std::weak_ptr<AuthData>> auths_copy;
  auths_copy.swap(auths_);
  for (const auto& weak_auth : auths_copy) {
    if (auto data = weak_auth.lock()) {
      data->MakeAuthHandle().RemoveIdTokenListener(this);
    }
  }
}

// Generate a method that notifies a vector of listeners using listeners_method.
// Obtains the Auth value handle BEFORE acquiring listeners_mutex so lock order
// stays strictly AppState::services_mutex_ -> AuthData::listeners_mutex.
#define AUTH_NOTIFY_LISTENERS(notify_method_name, notification_name,           \
                              listeners_vector, notification_method)           \
  void notify_method_name(AuthData* auth_data) {                               \
    Auth auth_handle = auth_data->MakeAuthHandle();                            \
    if (!auth_handle.is_valid()) return;                                       \
                                                                               \
    MutexLock lock(auth_data->listeners_mutex);                                \
                                                                               \
    /* Auth should have loaded persistent cache if exists when the listener */ \
    /* event is triggered for the first time. */                               \
    auth_data->persistent_cache_load_pending = false;                          \
                                                                               \
    /* Make a copy of the listener list in case it gets modified during */     \
    /* notification_method(). */                                               \
    auto listeners = auth_data->listeners_vector;                              \
    LogDebug(notification_name " changed. Notifying %zu listeners.",           \
             listeners.size());                                                \
                                                                               \
    for (auto* listener : listeners) {                                         \
      /* Skip any listeners that have been removed in notification_method() */ \
      /* on earlier iterations of this loop. */                                \
      if (!VectorContains(listener, auth_data->listeners_vector)) {            \
        continue;                                                              \
      }                                                                        \
                                                                               \
      /* Notify the listener. */                                               \
      listener->notification_method(auth_handle);                              \
    }                                                                          \
  }

AUTH_NOTIFY_LISTENERS(NotifyAuthStateListeners, "Auth state", listeners,
                      OnAuthStateChanged);

AUTH_NOTIFY_LISTENERS(NotifyIdTokenListeners, "ID token", id_token_listeners,
                      OnIdTokenChanged);

void NotifyAllListeners(AuthData* auth_data) {
  Auth auth_handle = auth_data->MakeAuthHandle();
  if (!auth_handle.is_valid()) return;

  MutexLock lock(auth_data->listeners_mutex);
  auth_data->persistent_cache_load_pending = false;

  auto auth_listeners = auth_data->listeners;
  LogDebug("Auth state changed. Notifying %zu listeners.",
           auth_listeners.size());
  for (auto* listener : auth_listeners) {
    if (!VectorContains(listener, auth_data->listeners)) continue;
    listener->OnAuthStateChanged(auth_handle);
  }

  auto token_listeners = auth_data->id_token_listeners;
  LogDebug("ID token changed. Notifying %zu listeners.",
           token_listeners.size());
  for (auto* listener : token_listeners) {
    if (!VectorContains(listener, auth_data->id_token_listeners)) continue;
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
