#include "remote_config/src/config_update_listener_registration_internal.h"
#include <functional>
#include <mutex>
#include <utility>
namespace firebase::remote_config::internal {
ConfigUpdateListenerRegistrationInternal::
    ConfigUpdateListenerRegistrationInternal(
        RemoteConfigInternal* remote_config,
        std::function<void()> listener_removal_function)
    : remote_config_(remote_config),
      listener_removal_function_(std::move(listener_removal_function)) {}
ConfigUpdateListenerRegistrationInternal::
    ~ConfigUpdateListenerRegistrationInternal() {
  Remove();
}
void ConfigUpdateListenerRegistrationInternal::Remove() {
  std::function<void()> fn;
  {
    std::scoped_lock lock(mutex_);
    fn = std::move(listener_removal_function_);
    listener_removal_function_ = nullptr;
  }
  if (fn) {
    fn();
  }
}
}  // namespace firebase::remote_config::internal
