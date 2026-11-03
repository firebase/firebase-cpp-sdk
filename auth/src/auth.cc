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
#include "auth/src/data.h"

// Register the module initializer.
FIREBASE_APP_REGISTER_CALLBACKS(
    auth, { return ::firebase::kInitResultSuccess; },
    {
        // Nothing to tear down.
    },
    false);

namespace firebase {
namespace auth {

DEFINE_FIREBASE_VERSION_STRING(FirebaseAuth);

namespace {
// The global map of Apps to Auths. Each App can have at most one Auth,
// and an Auth requires an App. So, the user acquires the Auth via an App
// reference.
std::map<App*, Auth*>* g_auths = nullptr;
Mutex* g_auths_mutex = new Mutex();

// Assumes *g_auths_mutex is held by caller.
Auth* FindAuthLocked(App* app) {
  if (g_auths == nullptr || app == nullptr) {
    return nullptr;
  }
  auto it = g_auths->find(app);
  return it != g_auths->end() ? it->second : nullptr;
}
}  // namespace

// static
Auth* Auth::GetAuth(App* app, InitResult* init_result_out) {
  if (app == nullptr) {
    LogError("Auth::GetAuth() called with null firebase::App.");
    if (init_result_out != nullptr) {
      *init_result_out = kInitResultFailedMissingDependency;
    }
    return nullptr;
  }

  Auth* auth = nullptr;
  bool is_new_auth = false;
  {
    MutexLock lock(*g_auths_mutex);
    if (g_auths == nullptr) {
      g_auths = new std::map<App*, Auth*>();
    }

    // Return the Auth if it already exists.
    Auth* existing_auth = FindAuthLocked(app);
    if (existing_auth != nullptr) {
      if (init_result_out != nullptr) *init_result_out = kInitResultSuccess;
      auth = existing_auth;
    } else {
      // Create the platform dependent version of Auth.
      void* auth_impl = CreatePlatformAuth(app);
      if (!auth_impl) {
        if (init_result_out != nullptr) {
          *init_result_out = kInitResultFailedMissingDependency;
        }
        return nullptr;
      }

      // Create a new Auth and initialize.
      // Note: Auth constructor registers into (*g_auths)[app] before calling
      // InitPlatformAuth so re-entrant FindAuth(app) calls during
      // initialization succeed.
      auth = new Auth(app, auth_impl);
      is_new_auth = true;
      LogDebug("Creating Auth %p for App %p", auth, app);

      if (init_result_out != nullptr) *init_result_out = kInitResultSuccess;
    }
  }

  if (is_new_auth) {
    // Register CleanupNotifier outside *g_auths_mutex to avoid lock-order
    // inversions with CleanupNotifier::CleanupAll().
    CleanupNotifier* notifier = CleanupNotifier::FindByOwner(app);
    assert(notifier);
    if (notifier != nullptr) {
      notifier->RegisterObject(auth, [](void* object) {
        Auth* auth_to_clean = reinterpret_cast<Auth*>(object);
        LogWarning(
            "Auth object %p should be deleted before the App %p it depends "
            "upon.",
            auth_to_clean,
            auth_to_clean->auth_data_ ? auth_to_clean->auth_data_->app
                                      : nullptr);
        auth_to_clean->DeleteInternal();
      });
    }
  }

  // Log heartbeat data when instance getters are called.
  // See go/firebase-platform-logging-design for more information.
  LogHeartbeat(auth);
  return auth;
}

#if !defined(SWIG)
// static
Auth* Auth::GetAuth(InitResult* init_result_out) {
  App* default_app = App::GetInstance();
  if (default_app == nullptr) {
    LogError("Auth::GetAuth() called before default App was created.");
    if (init_result_out != nullptr) {
      *init_result_out = kInitResultFailedMissingDependency;
    }
    return nullptr;
  }
  return GetAuth(default_app, init_result_out);
}
#endif  // !defined(SWIG)

// static
Auth* Auth::FindAuth(App* app) {
  MutexLock lock(*g_auths_mutex);
  return FindAuthLocked(app);
}

// Auth uses the pimpl mechanism to hide internal data types from the interface.
Auth::Auth(App* app, void* auth_impl) : auth_data_(new AuthData) {
  FIREBASE_ASSERT(app != nullptr && auth_impl != nullptr);
  auth_data_->app = app;
  auth_data_->auth = this;
  auth_data_->auth_impl = auth_impl;
  auth_data_->user_impl = nullptr;

  static const char* kApiIdentifier = "Auth";
  auth_data_->future_api_id = CreateApiIdentifier(kApiIdentifier, this);

  // Register in g_auths before InitPlatformAuth so any synchronous listener or
  // function-registry callback triggered during InitPlatformAuth can resolve
  // FindAuth(app).
  (*g_auths)[app] = this;

  InitPlatformAuth(auth_data_);
}

void Auth::DeleteInternal() {
  bool is_last_auth = false;
  {
    MutexLock lock(*g_auths_mutex);
    if (!auth_data_) return;

    {
      MutexLock destructing_lock(auth_data_->destructing_mutex);
      if (auth_data_->destructing) return;
      auth_data_->destructing = true;
    }

    if (g_auths != nullptr) {
      for (auto it = g_auths->begin(); it != g_auths->end(); ++it) {
        if (it->second == this) {
          LogDebug("Deleting Auth %p for App %p", this, it->first);
          g_auths->erase(it);
          break;
        }
      }
      if (g_auths->empty()) {
        delete g_auths;
        g_auths = nullptr;
        is_last_auth = true;
      }
    }
  }

  // Wait for any in-flight futures without holding g_auths_mutex so callbacks
  // calling FindAuth() cannot deadlock.
  while (!auth_data_->future_impl.IsSafeToDelete()) {
    internal::Sleep(100);
  }

  CleanupNotifier* notifier = CleanupNotifier::FindByOwner(auth_data_->app);
  if (notifier != nullptr) {
    notifier->UnregisterObject(this);
  }

  auth_data_->ClearListeners();

  // Destroy the platform-specific object before deleting auth_data_.
  DestroyPlatformAuth(auth_data_);

  // Delete the pimpl data (which also destroys phone_auth_provider).
  AuthData* data_to_delete = nullptr;
  {
    MutexLock lock(*g_auths_mutex);
    data_to_delete = auth_data_;
    auth_data_ = nullptr;
  }
  delete data_to_delete;

  // If this is the last Auth instance to be cleaned up, also clean up data for
  // Credentials.
  if (is_last_auth) {
    CleanupCredentialFutureImpl();
  }
}

Auth::~Auth() { DeleteInternal(); }

// Always non-nullptr since set in constructor.
App& Auth::app() {
  FIREBASE_ASSERT(auth_data_ != nullptr);
  return *auth_data_->app;
}

template <typename T>
static bool PushBackIfMissing(const T& entry, std::vector<T>* v) {
  auto it = std::find(v->begin(), v->end(), entry);
  if (it != v->end()) return false;

  v->push_back(entry);
  return true;
}

// Store a unique listener of type T in a listener_vector and unique auth
// object in auth_vector.  Both vectors must be in sync, i.e addition must
// either succeed or fail otherwise this method asserts.
// Return whether the listener is added.
template <typename T>
static bool AddListener(T listener, std::vector<T>* listener_vector, Auth* auth,
                        std::vector<Auth*>* auth_vector) {
  // Add to array of listeners if not already there.
  const bool listener_added = PushBackIfMissing(listener, listener_vector);
  // Add auth to array of Auths if not already there.
  const bool auth_added = PushBackIfMissing(auth, auth_vector);

  // The listener and Auth should either point at each other or not point
  // at each other.
  FIREBASE_ASSERT_RETURN(false, listener_added == auth_added);
  (void)auth_added;

  return listener_added;
}

void Auth::AddAuthStateListener(AuthStateListener* listener) {
  if (!auth_data_ || !listener) return;
  // Hold listeners_mutex until the method ends to protect against race
  // conditions.
  MutexLock lock(auth_data_->listeners_mutex);
  bool added =
      AddListener(listener, &auth_data_->listeners, this, &listener->auths_);

  // If the listener is registered successfully and persistent cache has been
  // loaded, trigger OnAuthStateChanged() immediately.  Otherwise, wait until
  // the cache is loaded, through AuthStateListener event.
  if (added && !auth_data_->persistent_cache_load_pending) {
    listener->OnAuthStateChanged(this);
  }
}

void Auth::AddIdTokenListener(IdTokenListener* listener) {
  if (!auth_data_ || !listener) return;
  // Hold listeners_mutex until the method ends to protect against race
  // conditions.
  MutexLock lock(auth_data_->listeners_mutex);
  bool added = AddListener(listener, &auth_data_->id_token_listeners, this,
                           &listener->auths_);
  // AddListener is valid even if the listener is already registered.
  // This makes sure that we only increase the reference count if a listener
  // was actually added.
  if (added) {
    // If the listener is registered successfully and persistent cache has been
    // loaded, trigger OnIdTokenChanged() immediately.  Otherwise, wait until
    // the cache is loaded, through IdTokenListener event.
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

// Remove a listener of type T from listener_vector and unique auth object in
// auth_vector.  Both vectors must be in sync.
// Returns true if the listener was present and removed.
template <typename T>
static bool RemoveListener(T listener, std::vector<T>* listener_vector,
                           Auth* auth, std::vector<Auth*>* auth_vector,
                           Mutex* mutex) {
  MutexLock lock(*mutex);
  // Remove `listener` from our vector of listeners while preserving FIFO order.
  const bool removed_listener =
      EraseEntryPreservingOrder(listener, listener_vector);
  // Remove this Auth from the listener's vector of Auths.
  // This ensures the listener doesn't try to unregister itself again when it is
  // destroyed.
  EraseEntryPreservingOrder(auth, auth_vector);
  return removed_listener;
}

void Auth::RemoveAuthStateListener(AuthStateListener* listener) {
  if (!auth_data_ || !listener) return;
  RemoveListener(listener, &auth_data_->listeners, this, &listener->auths_,
                 &auth_data_->listeners_mutex);
}

void Auth::RemoveIdTokenListener(IdTokenListener* listener) {
  if (!auth_data_ || !listener) return;
  const bool removed =
      RemoveListener(listener, &auth_data_->id_token_listeners, this,
                     &listener->auths_, &auth_data_->listeners_mutex);
  // Only decrease the reference count if a listener was actually removed.
  if (removed) {
    DisableTokenAutoRefresh(auth_data_);
  }
}

AuthStateListener::~AuthStateListener() {
  // Removing the listener edits the auths list, hence the while loop.
  while (!auths_.empty()) {
    (*auths_.begin())->RemoveAuthStateListener(this);
  }
}

IdTokenListener::~IdTokenListener() {
  // Removing the listener edits the auths list, hence the while loop.
  while (!auths_.empty()) {
    (*auths_.begin())->RemoveIdTokenListener(this);
  }
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

// Generate a method that notifies a vector of listeners using listeners_method.
#define AUTH_NOTIFY_LISTENERS(notify_method_name, notification_name,           \
                              listeners_vector, notification_method)           \
  void notify_method_name(AuthData* auth_data) {                               \
    if (auth_data == nullptr) return;                                          \
    MutexLock lock(auth_data->listeners_mutex);                                \
                                                                               \
    /* Auth should have loaded persistent cache if exists when the listener */ \
    /* event is triggered for the first time. */                               \
    auth_data->persistent_cache_load_pending = false;                          \
                                                                               \
    /* Make a copy of the listener list in case it gets modified during */     \
    /* notification_method(). Note that modification is possible because */    \
    /* the same thread is allowed to reacquire `listeners_mutex` */            \
    /* multiple times. */                                                      \
    auto listeners = auth_data->listeners_vector;                              \
    LogDebug(notification_name " changed. Notifying %zu listeners.",           \
             listeners.size());                                                \
                                                                               \
    for (auto it = listeners.begin(); it != listeners.end(); ++it) {           \
      /* Skip any listeners that have been removed in notification_method() */ \
      /* on earlier iterations of this loop. */                                \
      auto* listener = *it;                                                    \
      if (!VectorContains(listener, auth_data->listeners_vector)) {            \
        continue;                                                              \
      }                                                                        \
                                                                               \
      /* Notify the listener. */                                               \
      listener->notification_method(auth_data->auth);                          \
    }                                                                          \
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

}  // namespace auth
}  // namespace firebase
