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

#include "app/src/assert.h"
#include "app/src/cleanup_notifier.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/include/firebase/internal/platform.h"
#include "app/src/include/firebase/version.h"
#include "app/src/mutex.h"
#include "app/src/semaphore.h"
#include "app/src/util.h"
#include "auth/src/common.h"
#include "auth/src/data.h"

// Workaround MSVC's incompatible libc headers.
#if FIREBASE_PLATFORM_WINDOWS
#define snprintf _snprintf
#endif  // FIREBASE_PLATFORM_WINDOWS

// Register the module initializer.
FIREBASE_APP_REGISTER_CALLBACKS(
    auth,
    {
      FIREBASE_UTIL_RETURN_FAILURE_IF_GOOGLE_PLAY_UNAVAILABLE(*app);
      return ::firebase::kInitResultSuccess;
    },
    {
        // Nothing to tear down.
    });

namespace firebase {
namespace auth {

DEFINE_FIREBASE_VERSION_STRING(FirebaseAuth);

// The global map of Apps to Auths. Each App can have at most one Auth,
// and an Auth requires an App. So, the user acquires the Auth via an App
// reference.
// TODO(jsanmiya): Ensure all the Auths are destroyed on shutdown.
std::map<App*, Auth*> g_auths;
Mutex g_auths_mutex;  // NOLINT

// static
Auth* Auth::GetAuth(App* app, InitResult* init_result_out) {
  MutexLock lock(g_auths_mutex);
  // Return the Auth if it already exists.
  Auth* existing_auth = FindAuth(app);
  if (existing_auth) {
    if (init_result_out != nullptr) *init_result_out = kInitResultSuccess;
    return existing_auth;
  }

  FIREBASE_UTIL_RETURN_NULL_IF_GOOGLE_PLAY_UNAVAILABLE(*app, init_result_out);

  // Create the platform dependent version of Auth.
  void* auth_impl = CreatePlatformAuth(app);
  if (!auth_impl) return nullptr;

  // Create a new Auth and initialize.
  Auth* auth = new Auth(app, auth_impl);
  LogDebug("Creating Auth %p for App %p", auth, app);

  // Stick it in the global map so we remember it, and can delete it on
  // shutdown.
  g_auths[app] = auth;

  if (init_result_out) *init_result_out = kInitResultSuccess;
  return auth;
}

// static
Auth* Auth::FindAuth(App* app) {
  MutexLock lock(g_auths_mutex);
  // Return the Auth if it already exists.
  std::map<App*, Auth*>::iterator it = g_auths.find(app);
  if (it != g_auths.end()) {
    return it->second;
  }
  return nullptr;
}

// Auth uses the pimpl mechanism to hide internal data types from the interface.
Auth::Auth(App* app, void* auth_impl) : auth_data_(new AuthData) {
  FIREBASE_ASSERT(app != nullptr && auth_impl != nullptr);
  auth_data_->app = app;
  auth_data_->auth = this;
  auth_data_->auth_impl = auth_impl;
  InitPlatformAuth(auth_data_);

  std::string* future_id = &auth_data_->future_api_id;
  static const char* kApiIdentifier = "Auth";
  future_id->reserve(strlen(kApiIdentifier) +
                     16 /* hex characters in the pointer */ +
                     1 /* null terminator */);
  snprintf(&((*future_id)[0]), future_id->capacity(), "%s0x%016llx",
           kApiIdentifier,
           static_cast<unsigned long long>(  // NOLINT
               reinterpret_cast<intptr_t>(this)));

  // Cleanup this object if app is destroyed.
  CleanupNotifier* notifier = CleanupNotifier::FindByOwner(app);
  assert(notifier);
  notifier->RegisterObject(this, [](void* object) {
    Auth* auth = reinterpret_cast<Auth*>(object);
    LogWarning(
        "Auth object 0x%08x should be deleted before the App 0x%08x it "
        "depends upon.",
        static_cast<int>(reinterpret_cast<intptr_t>(auth)),
        static_cast<int>(reinterpret_cast<intptr_t>(auth->auth_data_->app)));
    auth->DeleteInternal();
  });
}

void Auth::DeleteInternal() {
  MutexLock lock(g_auths_mutex);

  if (!auth_data_) return;

  {
    MutexLock destructing_lock(auth_data_->desctruting_mutex);
    auth_data_->destructing = true;
  }

  CleanupNotifier* notifier = CleanupNotifier::FindByOwner(auth_data_->app);
  assert(notifier);
  notifier->UnregisterObject(this);

  int num_auths_remaining = 0;
  {
    // Remove `this` from the g_auths map.
    // The mapping is 1:1, so we should only ever delete one.
    for (auto it = g_auths.begin(); it != g_auths.end(); ++it) {
      if (it->second == this) {
        LogDebug("Deleting Auth %p for App %p", this, it->first);
        g_auths.erase(it);
        break;
      }
    }

    num_auths_remaining = g_auths.size();
  }

  auth_data_->ClearListeners();

  // If this is the last Auth instance to be cleaned up, also clean up data for
  // Credentials.
  if (num_auths_remaining == 0) {
    CleanupCredentialFutureImpl();
  }

  // Destroy the platform-specific object.
  DestroyPlatformAuth(auth_data_);

  // Delete the pimpl data.
  delete auth_data_;
  auth_data_ = nullptr;
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
  if (!auth_data_) return;
  // Would have to lock mutex till the method ends to protect on race
  // conditions.
  MutexLock lock(auth_data_->listeners_mutex);
  bool added =
      AddListener(listener, &auth_data_->listeners, this, &listener->auths_);

  // If the listener is registered successfully and persistent cache has been
  // loaded, trigger OnAuthStateChanged() immediately.  Otherwise, wait until
  // the cache is loaded, through AuthStateListener event.
  // NOTE: This should be called synchronously or current_user() for desktop
  // implementation may not work.
  if (added && !auth_data_->persistent_cache_load_pending) {
    listener->OnAuthStateChanged(this);
  }
}

void Auth::AddIdTokenListener(IdTokenListener* listener) {
  if (!auth_data_) return;
  // Would have to lock mutex till the method ends to protect on race
  // conditions.
  MutexLock lock(auth_data_->listeners_mutex);
  bool added = AddListener(listener, &auth_data_->id_token_listeners, this,
                           &listener->auths_);
  // AddListener is valid even if the listener is already registered.
  // This makes sure that we only increase the reference count if a listener
  // was actually added.
  if (added) {
    // If the listener is registered successfully and persistent cache has been
    // loaded, trigger OnAuthStateChanged() immediately.  Otherwise, wait until
    // the cache is loaded, through AuthStateListener event
    if (!auth_data_->persistent_cache_load_pending) {
      listener->OnIdTokenChanged(this);
    }
    EnableTokenAutoRefresh(auth_data_);
  }
}

template <typename T>
static bool ReplaceEntryWithBack(const T& entry, std::vector<T>* v) {
  auto it = std::find(v->begin(), v->end(), entry);
  if (it == v->end()) return false;

  // If it's not already the back element, move/copy the back element onto it.
  if (&(*it) != &(v->back())) {
#if defined(FIREBASE_USE_MOVE_OPERATORS)
    *it = std::move(v->back());
#else
    *it = v->back();
#endif  // defined(FIREBASE_USE_MOVE_OPERATORS)
  }
  v->pop_back();
  return true;
}

// Remove a listener of type T from listener_vector and unique auth object in
// auth_vector.  Both vectors must be in sync, i.e addition must either
// succeed or fail otherwise this method asserts.
template <typename T>
static void RemoveListener(T listener, std::vector<T>* listener_vector,
                           Auth* auth, std::vector<Auth*>* auth_vector,
                           Mutex* mutex) {
  MutexLock lock(*mutex);
  // Remove `listener` from our vector of listeners.
  ReplaceEntryWithBack(listener, listener_vector);
  // Remove this Auth from the listener's vector of Auths.
  // This ensures the listener doesn't try to unregister itself again when it is
  // destroyed.
  ReplaceEntryWithBack(auth, auth_vector);
}

void Auth::RemoveAuthStateListener(AuthStateListener* listener) {
  if (!auth_data_) return;
  RemoveListener(listener, &auth_data_->listeners, this, &listener->auths_,
                 &auth_data_->listeners_mutex);
}

void Auth::RemoveIdTokenListener(IdTokenListener* listener) {
  if (!auth_data_) return;
  int listener_count = auth_data_->id_token_listeners.size();
  RemoveListener(listener, &auth_data_->id_token_listeners, this,
                 &listener->auths_, &auth_data_->listeners_mutex);
  // RemoveListener is valid even if the listener is not registered.
  // This makes sure that we only decrease the reference count if a listener
  // was actually removed.
  if (auth_data_->id_token_listeners.size() < listener_count) {
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

template <typename T>
static inline bool VectorContains(const T& entry, const std::vector<T>& v) {
  return std::find(v.begin(), v.end(), entry) != v.end();
}

// Generate a method that notifies a vector of listeners using listeners_method.
#define AUTH_NOTIFY_LISTENERS(notify_method_name, notification_name,           \
                              listeners_vector, notification_method)           \
  void notify_method_name(AuthData* auth_data) {                               \
    MutexLock lock(auth_data->listeners_mutex);                                \
                                                                               \
    /* Auth should have loaded persistent cache if exists when the listener */ \
    /* event is triggered for the first time. */                               \
    auth_data->persistent_cache_load_pending = false;                          \
                                                                               \
    /* Make a copy of the listener list in case it gets modified during */     \
    /* notification_method(). Note that modification is possible because */    \
    /* the same thread is allowed to reaquire the `listeners_mutex` */         \
    /* multiple times. */                                                      \
    auto listeners = auth_data->listeners_vector;                              \
    LogDebug(notification_name " changed. Notifying %d listeners.",            \
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
                      OnAuthStateChanged);

AUTH_NOTIFY_LISTENERS(NotifyIdTokenListeners, "ID token", id_token_listeners,
                      OnIdTokenChanged);

AUTH_RESULT_FN(Auth, FetchProvidersForEmail, Auth::FetchProvidersResult)
AUTH_RESULT_FN(Auth, SignInWithCustomToken, User*)
AUTH_RESULT_FN(Auth, SignInWithCredential, User*)
AUTH_RESULT_FN(Auth, SignInAndRetrieveDataWithCredential, SignInResult)
AUTH_RESULT_FN(Auth, SignInAnonymously, User*)
AUTH_RESULT_FN(Auth, SignInWithEmailAndPassword, User*)
AUTH_RESULT_FN(Auth, CreateUserWithEmailAndPassword, User*)
AUTH_RESULT_FN(Auth, SendPasswordResetEmail, void)

}  // namespace auth
}  // namespace firebase
