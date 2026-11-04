#ifndef FIREBASE_REMOTE_CONFIG_SRC_CONFIG_UPDATE_LISTENER_REGISTRATION_INTERNAL_H_
#define FIREBASE_REMOTE_CONFIG_SRC_CONFIG_UPDATE_LISTENER_REGISTRATION_INTERNAL_H_
#include <functional>
#include <mutex>
namespace firebase::remote_config::internal {
class RemoteConfigInternal;
class ConfigUpdateListenerRegistrationInternal {
 public:
  ConfigUpdateListenerRegistrationInternal(
      RemoteConfigInternal* remote_config,
      std::function<void()> listener_removal_function);
  ConfigUpdateListenerRegistrationInternal(
      const ConfigUpdateListenerRegistrationInternal&) = delete;
  ConfigUpdateListenerRegistrationInternal& operator=(
      const ConfigUpdateListenerRegistrationInternal&) = delete;
  ConfigUpdateListenerRegistrationInternal(
      ConfigUpdateListenerRegistrationInternal&&) = delete;
  ConfigUpdateListenerRegistrationInternal& operator=(
      ConfigUpdateListenerRegistrationInternal&&) = delete;
  ~ConfigUpdateListenerRegistrationInternal();
  void Remove();
  [[nodiscard]] RemoteConfigInternal* remote_config() const {
    return remote_config_;
  }
 private:
  RemoteConfigInternal* remote_config_ = nullptr;
  std::mutex mutex_;
  std::function<void()> listener_removal_function_;
};
}  // namespace firebase::remote_config::internal
#endif
