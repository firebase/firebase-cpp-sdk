// Copyright 2016 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "messaging/src/common.h"

#include <assert.h>

#include <queue>
#include <string>

#include "app/src/cleanup_notifier.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/util.h"
#include "messaging/src/include/firebase/messaging.h"

// Register the module initializer.
FIREBASE_APP_REGISTER_CALLBACKS(
    messaging,
    {
      if (app == ::firebase::App::GetInstance()) {
        return firebase::messaging::Initialize(*app, nullptr);
      }
      return kInitResultSuccess;
    },
    {
      if (app == ::firebase::App::GetInstance()) {
        firebase::messaging::Terminate();
      }
    },
    false);

namespace firebase {
namespace messaging {

static Mutex g_listener_lock;  // NOLINT
static Listener* g_listener = nullptr;

// Keep track of the most recent token received.
static std::string* g_prev_token_received = nullptr;
// Keep track if that token was receieved without a listener set.
static bool g_has_pending_token = false;

// Keep track of the most recent registration installation ID received.
static std::string* g_prev_registration_received = nullptr;
// Keep track if that registration was received without a listener set.
static bool g_has_pending_registration = false;

namespace internal {

const char kMessagingModuleName[] = "messaging";

// Registers a cleanup task for this module if auto-initialization is disabled.
void RegisterTerminateOnDefaultAppDestroy() {
  if (!AppCallback::GetEnabledByName(kMessagingModuleName)) {
    CleanupNotifier* cleanup_notifier =
        CleanupNotifier::FindByOwner(App::GetInstance());
    assert(cleanup_notifier);
    cleanup_notifier->RegisterObject(
        const_cast<char*>(kMessagingModuleName), [](void*) {
          LogError(
              "messaging::Terminate() should be called before default app is "
              "destroyed.");
          if (firebase::messaging::internal::IsInitialized()) {
            firebase::messaging::Terminate();
          }
        });
  }
}

// Remove the cleanup task for this module if auto-initialization is disabled.
void UnregisterTerminateOnDefaultAppDestroy() {
  if (!AppCallback::GetEnabledByName(kMessagingModuleName) &&
      firebase::messaging::internal::IsInitialized()) {
    CleanupNotifier* cleanup_notifier =
        CleanupNotifier::FindByOwner(App::GetInstance());
    assert(cleanup_notifier);
    cleanup_notifier->UnregisterObject(const_cast<char*>(kMessagingModuleName));
  }
}

}  // namespace internal

Listener* SetListener(Listener* listener) {
  MutexLock lock(g_listener_lock);
  Listener* previous_listener = g_listener;
  if (listener && !g_prev_token_received) {
    g_prev_token_received = new std::string;
  }
  if (listener && !g_prev_registration_received) {
    g_prev_registration_received = new std::string;
  }
  g_listener = listener;
  // If we have a pending token, send it before notifying other systems about
  // the listener.
  if (g_listener && g_has_pending_token && g_prev_token_received) {
    g_listener->OnTokenReceived(g_prev_token_received->c_str());
    g_has_pending_token = false;
  }
  // If we have a pending registration installation ID, send it as well.
  if (g_listener && g_has_pending_registration &&
      g_prev_registration_received) {
    g_listener->OnRegistrationReceived(g_prev_registration_received->c_str());
    g_has_pending_registration = false;
  }
  NotifyListenerSet(listener);
  if (!listener) {
    if (g_prev_token_received) {
      std::string* ptr = g_prev_token_received;
      g_prev_token_received = nullptr;
      delete ptr;
      g_has_pending_token = false;
    }
    if (g_prev_registration_received) {
      std::string* ptr = g_prev_registration_received;
      g_prev_registration_received = nullptr;
      delete ptr;
      g_has_pending_registration = false;
    }
  }
  return previous_listener;
}

// Determine whether a listener is present.
bool HasListener() {
  MutexLock lock(g_listener_lock);
  return g_listener != nullptr;
}

Listener* SetListenerIfNotNull(Listener* listener) {
  return SetListener(listener ? listener : g_listener);
}

void NotifyListenerOnMessage(const Message& message) {
  MutexLock lock(g_listener_lock);
  if (g_listener) g_listener->OnMessage(message);
}

void NotifyListenerOnTokenReceived(const char* token) {
  MutexLock lock(g_listener_lock);
  // If we have a previous token that we've notified any listener about, check
  // to ensure no repeat.
  if (g_prev_token_received && *g_prev_token_received == token) {
    return;
  }

  if (!g_prev_token_received) g_prev_token_received = new std::string;
  *g_prev_token_received = token;

  if (g_listener) {
    g_listener->OnTokenReceived(token);
  } else {
    // Set so that if a listener is set in the future, it will be sent
    // the token, since the underlying platform won't send it again.
    g_has_pending_token = true;
  }
}

void NotifyListenerOnRegistrationReceived(const char* installationId) {
  if (!installationId || installationId[0] == '\0') {
    return;
  }
  MutexLock lock(g_listener_lock);
  if (g_prev_registration_received &&
      *g_prev_registration_received == installationId) {
    return;
  }

  if (!g_prev_registration_received)
    g_prev_registration_received = new std::string;
  *g_prev_registration_received = installationId;

  if (g_listener) {
    g_listener->OnRegistrationReceived(installationId);
  } else {
    g_has_pending_registration = true;
  }
}

void NotifyListenerOnUnregistrationReceived(const char* installationId) {
  if (!installationId || installationId[0] == '\0') {
    return;
  }
  MutexLock lock(g_listener_lock);
  // If the unregistration is for a pending registration that we haven't sent to
  // a listener yet, clear the pending state.
  if (g_prev_registration_received &&
      *g_prev_registration_received == installationId) {
    delete g_prev_registration_received;
    g_prev_registration_received = nullptr;
    g_has_pending_registration = false;
  }
  if (g_listener) {
    g_listener->OnUnregistrationReceived(installationId);
  }
}

class PollableListenerImpl {
 public:
  PollableListenerImpl()
      : token_(), registration_id_(), unregistration_id_(), messages_() {}

  void OnMessage(const Message& message) {
    // Since locking can potentially take a while, especially given that we
    // might need to do allocate, we instead do the more expensive copy outside
    // the mutex lock and then do the much faster move inside the critical
    // section. We have to do the copy first because the message passed in is
    // const and move constructors require non-const values.
#ifdef FIREBASE_USE_MOVE_OPERATORS
    Message new_message(message);
    {
      MutexLock lock(mutex_);
      messages_.push(std::move(new_message));
    }
#else
    {
      MutexLock lock(mutex_);
      messages_.push(message);
    }
#endif
  }

  void OnTokenReceived(const char* token) {
    MutexLock lock(mutex_);
    token_ = token;
  }

  void OnRegistrationReceived(const char* installationId) {
    MutexLock lock(mutex_);
    registration_id_ = installationId;
  }

  void OnUnregistrationReceived(const char* installationId) {
    MutexLock lock(mutex_);
    unregistration_id_ = installationId;
  }

  bool PollMessage(Message* message) {
    MutexLock lock(mutex_);
    if (messages_.empty()) {
      return false;
    }
    *message = messages_.front();
    messages_.pop();
    return true;
  }

  bool PollRegistrationToken(std::string* token) {
    MutexLock lock(mutex_);
    if (token_.empty()) {
      return false;
    }
    *token = token_;
    token_.clear();
    return true;
  }

  bool PollRegistration(std::string* installation_id) {
    MutexLock lock(mutex_);
    if (registration_id_.empty()) {
      return false;
    }
    *installation_id = registration_id_;
    registration_id_.clear();
    return true;
  }

  bool PollUnregistration(std::string* installation_id) {
    MutexLock lock(mutex_);
    if (unregistration_id_.empty()) {
      return false;
    }
    *installation_id = unregistration_id_;
    unregistration_id_.clear();
    return true;
  }

 private:
  // Mutex to prevent race conditions
  Mutex mutex_;

  // The newest registration token to be received.
  std::string token_;

  // The newest registration installation ID to be received.
  std::string registration_id_;

  // The newest unregistration installation ID to be received.
  std::string unregistration_id_;

  // A queue of all enqueued messages.
  std::queue<Message> messages_;
};

PollableListener::PollableListener() : impl_(new PollableListenerImpl()) {}

PollableListener::~PollableListener() { delete impl_; }

void PollableListener::OnMessage(const Message& message) {
  impl_->OnMessage(message);
}

void PollableListener::OnTokenReceived(const char* token) {
  impl_->OnTokenReceived(token);
}

void PollableListener::OnRegistrationReceived(const char* installationId) {
  impl_->OnRegistrationReceived(installationId);
}

void PollableListener::OnUnregistrationReceived(const char* installationId) {
  impl_->OnUnregistrationReceived(installationId);
}

bool PollableListener::PollMessage(Message* out_message) {
  return impl_->PollMessage(out_message);
}

std::string PollableListener::PollRegistrationToken(bool* got_token) {
  std::string out_token;
  *got_token = impl_->PollRegistrationToken(&out_token);
  return out_token;
}

std::string PollableListener::PollRegistration(bool* got_registration) {
  std::string out_registration;
  *got_registration = impl_->PollRegistration(&out_registration);
  return out_registration;
}

std::string PollableListener::PollUnregistration(bool* got_unregistration) {
  std::string out_unregistration;
  *got_unregistration = impl_->PollUnregistration(&out_unregistration);
  return out_unregistration;
}

FutureData* FutureData::s_future_data_;

// Create FutureData singleton.
FutureData* FutureData::Create() {
  s_future_data_ = new FutureData();
  return s_future_data_;
}

// Destroy the FutureData singleton.
void FutureData::Destroy() {
  delete s_future_data_;
  s_future_data_ = nullptr;
}

// Get the Future data singleton.
FutureData* FutureData::Get() { return s_future_data_; }

}  // namespace messaging
}  // namespace firebase
