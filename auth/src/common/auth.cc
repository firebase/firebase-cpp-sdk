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
#include <stdio.h>

#include <algorithm>
#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "app/src/assert.h"
#include "app/src/cleanup_notifier.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/include/firebase/internal/platform.h"
#include "app/src/include/firebase/version.h"
#include "app/src/log.h"
#include "app/src/semaphore.h"
#include "app/src/util.h"
#include "auth/src/common.h"
#include "auth/src/common/promise.h"
#include "auth/src/data.h"
#include "auth/src/include/firebase/auth/credential.h"

FIREBASE_APP_REGISTER_CALLBACKS(
    auth, { return ::firebase::kInitResultSuccess; }, {}, false);

namespace firebase {
namespace auth {

DEFINE_FIREBASE_VERSION_STRING(FirebaseAuth);

namespace {
std::map<App*, Auth*>* g_auths = nullptr;
Mutex* g_auths_mutex = new Mutex();
Mutex* g_auth_delete_mutex = new Mutex();

Auth* FindAuthLocked(App* app) {
  if (!g_auths || !app) return nullptr;
  auto it = g_auths->find(app);
  return it != g_auths->end() ? it->second : nullptr;
}

App* FindAppForAuthLocked(Auth* auth) {
  if (!g_auths || !auth) return nullptr;
  for (const auto& kv : *g_auths) {
    if (kv.second == auth) return kv.first;
  }
  return nullptr;
}

template <typename R, typename Fn, typename... Args>
Future<R> RunAuthPromiseFn(AuthData* auth_data, int fn_id, Fn fn,
                           Args&&... args) {
  if (!auth_data) return Future<R>();
  Promise<R> promise(&auth_data->future_impl, fn_id);
  fn(auth_data, promise, std::forward<Args>(args)...);
  return promise.future();
}
}  // namespace

Auth* Auth::GetAuth(App* app, InitResult* init_result_out) {
  if (!app) {
    LogError("Auth::GetAuth() called with null firebase::App.");
    if (init_result_out) *init_result_out = kInitResultFailedMissingDependency;
    return nullptr;
  }

  Auth* auth = nullptr;
  {
    MutexLock lock(*g_auths_mutex);
    if (!g_auths) g_auths = new std::map<App*, Auth*>();

    Auth* existing_auth = FindAuthLocked(app);
    if (existing_auth) {
      if (init_result_out) *init_result_out = kInitResultSuccess;
      auth = existing_auth;
    } else {
      void* auth_impl = CreatePlatformAuth(app);
      if (!auth_impl) {
        if (g_auths->empty()) {
          delete g_auths;
          g_auths = nullptr;
        }
        if (init_result_out) {
          *init_result_out = kInitResultFailedMissingDependency;
        }
        return nullptr;
      }

      auth = new Auth(app, auth_impl);
      LogDebug("Creating Auth %p for App %p", auth, app);

      CleanupNotifier* notifier = CleanupNotifier::FindByOwner(app);
      assert(notifier);
      if (notifier) {
        notifier->RegisterObject(auth, [](void* object) {
          Auth* auth_to_clean = reinterpret_cast<Auth*>(object);
          MutexLock delete_lock(*g_auth_delete_mutex);
          App* app_to_log = nullptr;
          {
            MutexLock lock(*g_auths_mutex);
            app_to_log = FindAppForAuthLocked(auth_to_clean);
            if (!app_to_log) return;
          }
          LogWarning(
              "Auth object %p should be deleted before the App %p it depends "
              "upon.",
              auth_to_clean, app_to_log);
          auth_to_clean->DeleteInternal();
        });
      }
      if (init_result_out) *init_result_out = kInitResultSuccess;
    }
    LogHeartbeat(auth);
  }
  return auth;
}

#if !defined(SWIG)
Auth* Auth::GetAuth(InitResult* init_result_out) {
  App* default_app = App::GetInstance();
  if (!default_app) {
    LogError("Auth::GetAuth() called before default App was created.");
    if (init_result_out) *init_result_out = kInitResultFailedMissingDependency;
    return nullptr;
  }
  return GetAuth(default_app, init_result_out);
}
#endif  // !defined(SWIG)

Auth* Auth::FindAuth(App* app) {
  MutexLock lock(*g_auths_mutex);
  return FindAuthLocked(app);
}

Auth::Auth(App* app, void* auth_impl) : auth_data_(new AuthData) {
  FIREBASE_ASSERT(app != nullptr && auth_impl != nullptr);
  auth_data_->app = app;
  auth_data_->auth = this;
  auth_data_->auth_impl = auth_impl;
  auth_data_->user_impl = nullptr;
  auth_data_->future_api_id = CreateApiIdentifier("Auth", this);
  (*g_auths)[app] = this;
  InitPlatformAuth(auth_data_);
}

void Auth::DeleteInternal() {
  MutexLock delete_lock(*g_auth_delete_mutex);
  App* app = nullptr;
  {
    MutexLock lock(*g_auths_mutex);
    if (!auth_data_) return;
    {
      MutexLock destructing_lock(auth_data_->destructing_mutex);
      if (auth_data_->destructing) return;
      auth_data_->destructing = true;
    }
    app = auth_data_->app;
    if (g_auths) {
      for (auto it = g_auths->begin(); it != g_auths->end(); ++it) {
        if (it->second == this) {
          LogDebug("Deleting Auth %p for App %p", this, it->first);
          g_auths->erase(it);
          break;
        }
      }
    }
  }

  if (CleanupNotifier* notifier = CleanupNotifier::FindByOwner(app)) {
    notifier->UnregisterObject(this);
  }

  while (!auth_data_->future_impl.IsSafeToDelete()) {
    ::firebase::internal::Sleep(100);
  }

  auth_data_->ClearListeners();
  DestroyPlatformAuth(auth_data_);

  AuthData* data_to_delete = nullptr;
  {
    MutexLock lock(*g_auths_mutex);
    data_to_delete = auth_data_;
    auth_data_ = nullptr;
  }
  delete data_to_delete;

  {
    MutexLock lock(*g_auths_mutex);
    if (g_auths && g_auths->empty()) {
      delete g_auths;
      g_auths = nullptr;
      CleanupCredentialFutureImpl();
    }
  }
}

Auth::~Auth() { DeleteInternal(); }

App& Auth::app() {
  FIREBASE_ASSERT(auth_data_ != nullptr);
  return *auth_data_->app;
}

template <typename T>
static bool PushBackIfMissing(const T& entry, std::vector<T>* v) {
  if (std::find(v->begin(), v->end(), entry) != v->end()) return false;
  v->push_back(entry);
  return true;
}

template <typename T>
static bool AddListener(T listener, std::vector<T>* listener_vector, Auth* auth,
                        std::vector<Auth*>* auth_vector) {
  const bool listener_added = PushBackIfMissing(listener, listener_vector);
  const bool auth_added = PushBackIfMissing(auth, auth_vector);
  FIREBASE_ASSERT_RETURN(false, listener_added == auth_added);
  (void)auth_added;
  return listener_added;
}

void Auth::AddAuthStateListener(AuthStateListener* listener) {
  if (!auth_data_ || !listener) return;
  MutexLock lock(auth_data_->listeners_mutex);
  if (AddListener(listener, &auth_data_->listeners, this, &listener->auths_) &&
      !auth_data_->persistent_cache_load_pending) {
    listener->OnAuthStateChanged(this);
  }
}

void Auth::AddIdTokenListener(IdTokenListener* listener) {
  if (!auth_data_ || !listener) return;
  MutexLock lock(auth_data_->listeners_mutex);
  if (AddListener(listener, &auth_data_->id_token_listeners, this,
                  &listener->auths_)) {
    if (!auth_data_->persistent_cache_load_pending) {
      listener->OnIdTokenChanged(this);
    }
    EnableTokenAutoRefresh(auth_data_);
  }
}

template <typename T>
static bool EraseEntryPreservingOrder(const T& entry, std::vector<T>* v) {
  auto it = std::find(v->begin(), v->end(), entry);
  if (it == v->end()) return false;
  v->erase(it);
  return true;
}

template <typename T>
static bool RemoveListener(T listener, std::vector<T>* listener_vector,
                           Auth* auth, std::vector<Auth*>* auth_vector,
                           Mutex* mutex) {
  MutexLock lock(*mutex);
  const bool removed = EraseEntryPreservingOrder(listener, listener_vector);
  EraseEntryPreservingOrder(auth, auth_vector);
  return removed;
}

void Auth::RemoveAuthStateListener(AuthStateListener* listener) {
  if (!auth_data_ || !listener) return;
  RemoveListener(listener, &auth_data_->listeners, this, &listener->auths_,
                 &auth_data_->listeners_mutex);
}

void Auth::RemoveIdTokenListener(IdTokenListener* listener) {
  if (!auth_data_ || !listener) return;
  if (RemoveListener(listener, &auth_data_->id_token_listeners, this,
                     &listener->auths_, &auth_data_->listeners_mutex)) {
    DisableTokenAutoRefresh(auth_data_);
  }
}

AuthStateListener::~AuthStateListener() {
  while (!auths_.empty()) (*auths_.begin())->RemoveAuthStateListener(this);
}

IdTokenListener::~IdTokenListener() {
  while (!auths_.empty()) (*auths_.begin())->RemoveIdTokenListener(this);
}

#ifndef SWIG
FederatedOAuthProvider::FederatedOAuthProvider(
    const FederatedOAuthProviderData& provider_data, AuthHandler* handler)
    : provider_data_(provider_data), handler_(handler) {}
#endif  // !SWIG

template <typename T>
static inline bool VectorContains(const T& entry, const std::vector<T>& v) {
  return std::find(v.begin(), v.end(), entry) != v.end();
}

#define AUTH_NOTIFY_LISTENERS(notify_method_name, notification_name,       \
                              listeners_vector, notification_method)       \
  void notify_method_name(AuthData* auth_data) {                           \
    if (!auth_data) return;                                                \
    MutexLock lock(auth_data->listeners_mutex);                            \
    auth_data->persistent_cache_load_pending = false;                      \
    auto listeners = auth_data->listeners_vector;                          \
    LogDebug(notification_name " changed. Notifying %zu listeners.",       \
             listeners.size());                                            \
    for (auto* listener : listeners) {                                     \
      if (VectorContains(listener, auth_data->listeners_vector)) {         \
        listener->notification_method(auth_data->auth);                    \
      }                                                                    \
    }                                                                      \
  }

AUTH_NOTIFY_LISTENERS(NotifyAuthStateListeners, "Auth state", listeners,
                      OnAuthStateChanged)
AUTH_NOTIFY_LISTENERS(NotifyIdTokenListeners, "ID token", id_token_listeners,
                      OnIdTokenChanged)

AUTH_RESULT_FN(Auth, FetchProvidersForEmail, Auth::FetchProvidersResult)
AUTH_RESULT_FN(Auth, SendPasswordResetEmail, void)
AUTH_RESULT_FN(Auth, SignInWithCustomToken, AuthResult)
AUTH_RESULT_FN(Auth, SignInWithCredential, User)
AUTH_RESULT_FN(Auth, SignInWithProvider, AuthResult)
AUTH_RESULT_FN(Auth, SignInAndRetrieveDataWithCredential, AuthResult)
AUTH_RESULT_FN(Auth, SignInAnonymously, AuthResult)
AUTH_RESULT_FN(Auth, SignInWithEmailAndPassword, AuthResult)
AUTH_RESULT_FN(Auth, CreateUserWithEmailAndPassword, AuthResult)

Future<AuthResult> Auth::SignInWithCustomToken(const char* custom_token) {
  return RunAuthPromiseFn<AuthResult>(auth_data_, kAuthFn_SignInWithCustomToken,
                                      ::firebase::auth::SignInWithCustomToken,
                                      custom_token);
}

Future<User> Auth::SignInWithCredential(const Credential& credential) {
  return RunAuthPromiseFn<User>(auth_data_, kAuthFn_SignInWithCredential,
                                ::firebase::auth::SignInWithCredential,
                                credential);
}

Future<AuthResult> Auth::SignInWithProvider(FederatedAuthProvider* provider) {
  return auth_data_ ? ::firebase::auth::SignInWithProvider(auth_data_, provider)
                    : Future<AuthResult>();
}

Future<AuthResult> Auth::SignInAnonymously() {
  return RunAuthPromiseFn<AuthResult>(auth_data_, kAuthFn_SignInAnonymously,
                                      ::firebase::auth::SignInAnonymously);
}

Future<AuthResult> Auth::SignInWithEmailAndPassword(const char* const email,
                                                    const char* const password) {
  return RunAuthPromiseFn<AuthResult>(
      auth_data_, kAuthFn_SignInWithEmailAndPassword,
      ::firebase::auth::SignInWithEmailAndPassword, email, password);
}

Future<AuthResult> Auth::CreateUserWithEmailAndPassword(
    const char* const email, const char* const password) {
  return RunAuthPromiseFn<AuthResult>(
      auth_data_, kAuthFn_CreateUserWithEmailAndPassword,
      ::firebase::auth::CreateUserWithEmailAndPassword, email, password);
}

Future<AuthResult> Auth::SignInAndRetrieveDataWithCredential(
    const Credential& credential) {
  return RunAuthPromiseFn<AuthResult>(
      auth_data_, kAuthFn_SignInAndRetrieveDataWithCredential,
      ::firebase::auth::SignInAndRetrieveDataWithCredential, credential);
}

Future<Auth::FetchProvidersResult> Auth::FetchProvidersForEmail(
    const char* email) {
  return RunAuthPromiseFn<Auth::FetchProvidersResult>(
      auth_data_, kAuthFn_FetchProvidersForEmail,
      ::firebase::auth::FetchProvidersForEmail, email);
}

Future<void> Auth::SendPasswordResetEmail(const char* email) {
  return RunAuthPromiseFn<void>(auth_data_, kAuthFn_SendPasswordResetEmail,
                                ::firebase::auth::SendPasswordResetEmail,
                                email);
}

User Auth::current_user() {
  return auth_data_ ? auth_data_->current_user : User();
}

void Auth::SignOut() {
  if (auth_data_) ::firebase::auth::SignOut(auth_data_);
}

}  // namespace auth
}  // namespace firebase
