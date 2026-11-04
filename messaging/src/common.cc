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
#include <utility>

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

namespace {

struct ListenerState {
  Mutex lock;
  Listener* listener = nullptr;
  std::string prev_token_received;
  bool has_pending_token = false;
  std::string prev_registration_received;
  bool has_pending_registration = false;
  std::string prev_unregistration_received;
  bool has_pending_unregistration = false;
  const App* cleanup_app = nullptr;
};

ListenerState& GetListenerState() {
  static ListenerState* s_state = new ListenerState();
  return *s_state;
}

}  // namespace

namespace internal {

const char kMessagingModuleName[] = "messaging";

// Registers a cleanup task for this module if auto-initialization is disabled.
void RegisterTerminateOnDefaultAppDestroy(const App& app) {
  if (!AppCallback::GetEnabledByName(kMessagingModuleName)) {
    CleanupNotifier* cleanup_notifier =
        CleanupNotifier::FindByOwner(const_cast<App*>(&app));
    assert(cleanup_notifier);
    if (cleanup_notifier) {
      {
        MutexLock lock(GetListenerState().lock);
        GetListenerState().cleanup_app = &app;
      }
      cleanup_notifier->RegisterObject(
          const_cast<char*>(kMessagingModuleName), [](void*) {
            {
              MutexLock lock(GetListenerState().lock);
              GetListenerState().cleanup_app = nullptr;
            }
            LogError(
                "messaging::Terminate() should be called before default app is "
                "destroyed.");
            if (firebase::messaging::internal::IsInitialized()) {
              firebase::messaging::Terminate();
            }
          });
    }
  }
}

void RegisterTerminateOnDefaultAppDestroy() {
  App* default_app = App::GetInstance();
  if (default_app) {
    RegisterTerminateOnDefaultAppDestroy(*default_app);
  }
}

// Remove the cleanup task for this module if auto-initialization is disabled.
void UnregisterTerminateOnDefaultAppDestroy() {
  App* owner = nullptr;
  {
    MutexLock lock(GetListenerState().lock);
    if (GetListenerState().cleanup_app != nullptr) {
      owner = const_cast<App*>(GetListenerState().cleanup_app);
      GetListenerState().cleanup_app = nullptr;
    } else if (!AppCallback::GetEnabledByName(kMessagingModuleName) &&
               firebase::messaging::internal::IsInitialized()) {
      owner = App::GetInstance();
    }
  }
  if (owner) {
    CleanupNotifier* cleanup_notifier = CleanupNotifier::FindByOwner(owner);
    assert(cleanup_notifier);
    if (cleanup_notifier) {
      cleanup_notifier->UnregisterObject(
          const_cast<char*>(kMessagingModuleName));
    }
  }
}

}  // namespace internal

Listener* SetListener(Listener* listener) {
  ListenerState& state = GetListenerState();
  MutexLock lock(state.lock);
  Listener* previous_listener = state.listener;
  state.listener = listener;
  if (state.listener) {
    // If we have a pending token (or are replacing a previous listener when a
    // token was already received), send it before notifying other systems
    // about the listener.
    if (!state.prev_token_received.empty() &&
        (state.has_pending_token ||
         (previous_listener != nullptr &&
          previous_listener != state.listener))) {
      std::string token_copy = state.prev_token_received;
      state.has_pending_token = false;
      state.listener->OnTokenReceived(token_copy.c_str());
    }
    // If we have a pending registration installation ID, send it as well.
    if (state.listener && !state.prev_registration_received.empty() &&
        (state.has_pending_registration ||
         (previous_listener != nullptr &&
          previous_listener != state.listener))) {
      std::string reg_copy = state.prev_registration_received;
      state.has_pending_registration = false;
      state.listener->OnRegistrationReceived(reg_copy.c_str());
    }
    if (state.listener && !state.prev_unregistration_received.empty() &&
        (state.has_pending_unregistration ||
         (previous_listener != nullptr &&
          previous_listener != state.listener))) {
      std::string unreg_copy = state.prev_unregistration_received;
      state.has_pending_unregistration = false;
      state.listener->OnUnregistrationReceived(unreg_copy.c_str());
    }
  } else {
    state.prev_token_received.clear();
    state.has_pending_token = false;
    state.prev_registration_received.clear();
    state.has_pending_registration = false;
    state.prev_unregistration_received.clear();
    state.has_pending_unregistration = false;
  }
  NotifyListenerSet(state.listener);
  return previous_listener;
}

void UnregisterListener(Listener* listener) {
  ListenerState& state = GetListenerState();
  MutexLock lock(state.lock);
  if (state.listener == listener && listener != nullptr) {
    state.listener = nullptr;
    if (!state.prev_token_received.empty()) {
      state.has_pending_token = true;
    }
    if (!state.prev_registration_received.empty()) {
      state.has_pending_registration = true;
    }
    if (!state.prev_unregistration_received.empty()) {
      state.has_pending_unregistration = true;
    }
    NotifyListenerSet(nullptr);
  }
}

// Determine whether a listener is present.
bool HasListener() {
  MutexLock lock(GetListenerState().lock);
  return GetListenerState().listener != nullptr;
}

Listener* SetListenerIfNotNull(Listener* listener) {
  if (!listener) {
    MutexLock lock(GetListenerState().lock);
    return GetListenerState().listener;
  }
  return SetListener(listener);
}

void NotifyListenerOnMessage(const Message& message) {
  ListenerState& state = GetListenerState();
  MutexLock lock(state.lock);
  if (state.listener) state.listener->OnMessage(message);
}

void NotifyListenerOnTokenReceived(const char* token) {
  if (!token) {
    return;
  }
  ListenerState& state = GetListenerState();
  MutexLock lock(state.lock);
  // If we have a previous token that we've notified the current listener about,
  // check to ensure no repeat.
  if (!state.has_pending_token && !state.prev_token_received.empty() &&
      state.prev_token_received == token) {
    return;
  }

  state.prev_token_received = token;

  if (state.listener) {
    state.has_pending_token = false;
    state.listener->OnTokenReceived(token);
  } else {
    // Set so that if a listener is set in the future, it will be sent
    // the token, since the underlying platform won't send it again.
    state.has_pending_token = true;
  }
}

void NotifyListenerOnRegistrationReceived(const char* installationId) {
  if (!installationId || installationId[0] == '\0') {
    return;
  }
  ListenerState& state = GetListenerState();
  MutexLock lock(state.lock);
  state.prev_unregistration_received.clear();
  state.has_pending_unregistration = false;

  state.prev_registration_received = installationId;

  if (state.listener) {
    state.has_pending_registration = false;
    state.listener->OnRegistrationReceived(installationId);
  } else {
    state.has_pending_registration = true;
  }
}

void NotifyListenerOnUnregistrationReceived(const char* installationId) {
  if (!installationId || installationId[0] == '\0') {
    return;
  }
  ListenerState& state = GetListenerState();
  MutexLock lock(state.lock);
  // Clear cached registration/token state since the installation unregistered.
  state.prev_registration_received.clear();
  state.has_pending_registration = false;
  state.prev_token_received.clear();
  state.has_pending_token = false;

  state.prev_unregistration_received = installationId;

  if (state.listener) {
    state.has_pending_unregistration = false;
    state.listener->OnUnregistrationReceived(installationId);
  } else {
    state.has_pending_unregistration = true;
  }
}

class PollableListenerImpl {
 public:
  PollableListenerImpl()
      : token_(), registration_id_(), unregistration_id_(), messages_() {}

  void OnMessage(const Message& message) {
    // Since locking can potentially take a while, especially given that we
    // might need to allocate, we instead do the more expensive copy outside
    // the mutex lock and then do the much faster move inside the critical
    // section. We have to do the copy first because the message passed in is
    // const and move constructors require non-const values.
    Message new_message(message);
    {
      MutexLock lock(mutex_);
      messages_.push(std::move(new_message));
    }
  }

  void OnTokenReceived(const char* token) {
    if (!token) return;
    MutexLock lock(mutex_);
    token_ = token;
  }

  void OnRegistrationReceived(const char* installationId) {
    if (!installationId) return;
    MutexLock lock(mutex_);
    registration_id_ = installationId;
    unregistration_id_.clear();
  }

  void OnUnregistrationReceived(const char* installationId) {
    if (!installationId) return;
    MutexLock lock(mutex_);
    unregistration_id_ = installationId;
    registration_id_.clear();
    token_.clear();
  }

  bool PollMessage(Message* message) {
    if (!message) return false;
    MutexLock lock(mutex_);
    if (messages_.empty()) {
      return false;
    }
    *message = std::move(messages_.front());
    messages_.pop();
    return true;
  }

  bool PollRegistrationToken(std::string* token) {
    if (!token) return false;
    MutexLock lock(mutex_);
    if (token_.empty()) {
      return false;
    }
    *token = std::move(token_);
    token_.clear();
    return true;
  }

  bool PollRegistration(std::string* installation_id) {
    if (!installation_id) return false;
    MutexLock lock(mutex_);
    if (registration_id_.empty()) {
      return false;
    }
    *installation_id = std::move(registration_id_);
    registration_id_.clear();
    return true;
  }

  bool PollUnregistration(std::string* installation_id) {
    if (!installation_id) return false;
    MutexLock lock(mutex_);
    if (unregistration_id_.empty()) {
      return false;
    }
    *installation_id = std::move(unregistration_id_);
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

PollableListener::PollableListener(PollableListener&& other) noexcept
    : impl_(nullptr) {
  ListenerState& state = GetListenerState();
  MutexLock lock(state.lock);
  impl_ = other.impl_;
  other.impl_ = nullptr;
  if (state.listener == &other) {
    state.listener = this;
  }
}

PollableListener& PollableListener::operator=(
    PollableListener&& other) noexcept {
  if (this != &other) {
    PollableListenerImpl* old_impl = nullptr;
    {
      ListenerState& state = GetListenerState();
      MutexLock lock(state.lock);
      old_impl = impl_;
      impl_ = other.impl_;
      other.impl_ = nullptr;
      if (state.listener == &other) {
        state.listener = this;
      } else if (state.listener == this) {
        UnregisterListener(this);
      }
    }
    delete old_impl;
  }
  return *this;
}

PollableListener::~PollableListener() {
  PollableListenerImpl* old_impl = nullptr;
  {
    ListenerState& state = GetListenerState();
    MutexLock lock(state.lock);
    UnregisterListener(this);
    old_impl = impl_;
    impl_ = nullptr;
  }
  delete old_impl;
}

void PollableListener::OnMessage(const Message& message) {
  if (impl_) impl_->OnMessage(message);
}

void PollableListener::OnTokenReceived(const char* token) {
  if (impl_) impl_->OnTokenReceived(token);
}

void PollableListener::OnRegistrationReceived(const char* installationId) {
  if (impl_) impl_->OnRegistrationReceived(installationId);
}

void PollableListener::OnUnregistrationReceived(const char* installationId) {
  if (impl_) impl_->OnUnregistrationReceived(installationId);
}

bool PollableListener::PollMessage(Message* out_message) {
  return impl_ ? impl_->PollMessage(out_message) : false;
}

std::string PollableListener::PollRegistrationToken(bool* got_token) {
  std::string out_token;
  const bool result = impl_ ? impl_->PollRegistrationToken(&out_token) : false;
  if (got_token) *got_token = result;
  return out_token;
}

std::string PollableListener::PollRegistration(bool* got_registration) {
  std::string out_registration;
  const bool result =
      impl_ ? impl_->PollRegistration(&out_registration) : false;
  if (got_registration) *got_registration = result;
  return out_registration;
}

std::string PollableListener::PollUnregistration(bool* got_unregistration) {
  std::string out_unregistration;
  const bool result =
      impl_ ? impl_->PollUnregistration(&out_unregistration) : false;
  if (got_unregistration) *got_unregistration = result;
  return out_unregistration;
}

FutureData* FutureData::s_future_data_;

// Create FutureData singleton.
FutureData* FutureData::Create() {
  delete s_future_data_;
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
