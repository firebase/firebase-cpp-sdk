#ifndef FIREBASE_REMOTE_CONFIG_SRC_ANDROID_REMOTE_CONFIG_ANDROID_H_
#define FIREBASE_REMOTE_CONFIG_SRC_ANDROID_REMOTE_CONFIG_ANDROID_H_
#include <atomic>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>
#include "app/src/cleanup_notifier.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/reference_counted_future_impl.h"
#include "firebase/app.h"
#include "firebase/future.h"
#include "remote_config/src/include/firebase/remote_config.h"
namespace firebase::remote_config::internal {
class RemoteConfigInternal {
 public:
  explicit RemoteConfigInternal(const firebase::App& app);
  ~RemoteConfigInternal();
  RemoteConfigInternal(const RemoteConfigInternal&) = delete;
  RemoteConfigInternal& operator=(const RemoteConfigInternal&) = delete;
  RemoteConfigInternal(RemoteConfigInternal&&) = delete;
  RemoteConfigInternal& operator=(RemoteConfigInternal&&) = delete;
  Future<ConfigInfo> EnsureInitialized();
  Future<ConfigInfo> EnsureInitializedLastResult();
  Future<bool> Activate();
  Future<bool> ActivateLastResult();
  Future<bool> FetchAndActivate();
  Future<bool> FetchAndActivateLastResult();
  Future<void> Fetch(uint64_t cache_expiration_in_seconds);
  Future<void> FetchLastResult();
#ifndef SWIG
  Future<void> SetDefaults(const ConfigKeyValueVariant* defaults,
                           size_t number_of_defaults);
#endif
  Future<void> SetDefaults(const ConfigKeyValue* defaults,
                           size_t number_of_defaults);
  Future<void> SetDefaults(int defaults_resource_id);
  Future<void> SetDefaultsLastResult();
  Future<void> SetConfigSettings(ConfigSettings settings);
  Future<void> SetConfigSettingsLastResult();
  Future<void> SetCustomSignals(
      const std::map<std::string, Variant>& custom_signals);
  Future<void> SetCustomSignalsLastResult();
  [[nodiscard]] ConfigSettings GetConfigSettings();
  [[nodiscard]] bool GetBoolean(const char* key, ValueInfo* info);
  [[nodiscard]] std::string GetString(const char* key, ValueInfo* info);
  [[nodiscard]] int64_t GetLong(const char* key, ValueInfo* info);
  [[nodiscard]] double GetDouble(const char* key, ValueInfo* info);
  [[nodiscard]] std::vector<unsigned char> GetData(const char* key,
                                                   ValueInfo* info);
  [[nodiscard]] std::vector<std::string> GetKeys();
  [[nodiscard]] std::vector<std::string> GetKeysByPrefix(const char* prefix);
  [[nodiscard]] std::map<std::string, Variant> GetAll();
  [[nodiscard]] ConfigInfo GetInfo() const;
  [[nodiscard]] ConfigUpdateListenerRegistration AddOnConfigUpdateListener(
      std::function<void(ConfigUpdate&&, RemoteConfigError)>
          config_update_listener);
  [[nodiscard]] bool Initialized() const;
  void Cleanup();
  CleanupNotifier& cleanup_notifier() { return cleanup_; }
  void set_throttled_end_time(int64_t end_time) {
    throttled_end_time_.store(end_time);
  }
  [[nodiscard]] int64_t throttled_end_time() const {
    return throttled_end_time_.load();
  }
  void SaveTmpKeysToDefault(std::vector<std::string> tmp_default_keys) {
    MutexLock lock(default_key_mutex_);
    default_keys_ = std::move(tmp_default_keys);
  }
 private:
  const firebase::App& app_;
  ReferenceCountedFutureImpl future_impl_;
  jobject internal_obj_ = nullptr;
  bool initialized_ref_ = false;
  Mutex default_key_mutex_;
  std::vector<std::string> default_keys_;
  std::atomic<int64_t> throttled_end_time_{0};
  std::string jni_task_id_;
  CleanupNotifier cleanup_;
};
}  // namespace firebase::remote_config::internal
#endif
