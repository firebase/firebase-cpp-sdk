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
  if (App* default_app = App::GetInstance()) {
    RegisterTerminateOnDefaultAppDestroy(*default_app);
  }
}
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
  Listener* prev = state.listener;
  state.listener = listener;
  if (state.listener) {
    bool replaced = (prev != nullptr && prev != state.listener);
    if (!state.prev_token_received.empty() &&
        (state.has_pending_token || replaced)) {
      std::string copy = state.prev_token_received;
      state.has_pending_token = false;
      state.listener->OnTokenReceived(copy.c_str());
    }
    if (state.listener && !state.prev_registration_received.empty() &&
        (state.has_pending_registration || replaced)) {
      std::string copy = state.prev_registration_received;
      state.has_pending_registration = false;
      state.listener->OnRegistrationReceived(copy.c_str());
    }
    if (state.listener && !state.prev_unregistration_received.empty() &&
        (state.has_pending_unregistration || replaced)) {
      std::string copy = state.prev_unregistration_received;
      state.has_pending_unregistration = false;
      state.listener->OnUnregistrationReceived(copy.c_str());
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
  return prev;
}
void UnregisterListener(Listener* listener) {
  ListenerState& state = GetListenerState();
  MutexLock lock(state.lock);
  if (state.listener == listener && listener != nullptr) {
    state.listener = nullptr;
    if (!state.prev_token_received.empty()) state.has_pending_token = true;
    if (!state.prev_registration_received.empty()) {
      state.has_pending_registration = true;
    }
    if (!state.prev_unregistration_received.empty()) {
      state.has_pending_unregistration = true;
    }
    NotifyListenerSet(nullptr);
  }
}
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
  if (!token) return;
  ListenerState& state = GetListenerState();
  MutexLock lock(state.lock);
  if (!state.has_pending_token && !state.prev_token_received.empty() &&
      state.prev_token_received == token) {
    return;
  }
  state.prev_token_received = token;
  if (state.listener) {
    state.has_pending_token = false;
    state.listener->OnTokenReceived(token);
  } else {
    state.has_pending_token = true;
  }
}
void NotifyListenerOnRegistrationReceived(const char* installationId) {
  if (!installationId || installationId[0] == '\0') return;
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
  if (!installationId || installationId[0] == '\0') return;
  ListenerState& state = GetListenerState();
  MutexLock lock(state.lock);
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
  void OnMessage(const Message& message) {
    Message copy(message);
    MutexLock lock(mutex_);
    messages_.push(std::move(copy));
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
    if (messages_.empty()) return false;
    *message = std::move(messages_.front());
    messages_.pop();
    return true;
  }
  bool PollString(std::string PollableListenerImpl::* field, std::string* out) {
    if (!out) return false;
    MutexLock lock(mutex_);
    if ((this->*field).empty()) return false;
    *out = std::move(this->*field);
    (this->*field).clear();
    return true;
  }
  std::string token_;
  std::string registration_id_;
  std::string unregistration_id_;
 private:
  Mutex mutex_;
  std::queue<Message> messages_;
};
PollableListener::PollableListener() : impl_(new PollableListenerImpl()) {}
PollableListener::PollableListener(PollableListener&& other) noexcept
    : impl_(nullptr) {
  ListenerState& state = GetListenerState();
  MutexLock lock(state.lock);
  impl_ = other.impl_;
  other.impl_ = nullptr;
  if (state.listener == &other) state.listener = this;
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
  std::string out;
  bool ok =
      impl_ ? impl_->PollString(&PollableListenerImpl::token_, &out) : false;
  if (got_token) *got_token = ok;
  return out;
}
std::string PollableListener::PollRegistration(bool* got_registration) {
  std::string out;
  bool ok = impl_ ? impl_->PollString(&PollableListenerImpl::registration_id_,
                                      &out)
                  : false;
  if (got_registration) *got_registration = ok;
  return out;
}
std::string PollableListener::PollUnregistration(bool* got_unregistration) {
  std::string out;
  bool ok = impl_ ? impl_->PollString(&PollableListenerImpl::unregistration_id_,
                                      &out)
                  : false;
  if (got_unregistration) *got_unregistration = ok;
  return out;
}
FutureData* FutureData::s_future_data_;
FutureData* FutureData::Create() {
  delete s_future_data_;
  s_future_data_ = new FutureData();
  return s_future_data_;
}
void FutureData::Destroy() {
  delete s_future_data_;
  s_future_data_ = nullptr;
}
FutureData* FutureData::Get() { return s_future_data_; }
}  // namespace messaging
}  // namespace firebase
