#ifndef FIREBASE_REMOTE_CONFIG_SRC_IOS_REMOTE_CONFIG_IOS_H_
#define FIREBASE_REMOTE_CONFIG_SRC_IOS_REMOTE_CONFIG_IOS_H_
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "app/src/cleanup_notifier.h"
#include "app/src/util_ios.h"
#include "firebase/app.h"
#include "firebase/future.h"
#include "remote_config/src/include/firebase/remote_config.h"
#ifdef __OBJC__
#import "FIRRemoteConfig.h"
#endif
namespace firebase {
namespace remote_config {
namespace internal {
OBJ_C_PTR_WRAPPER(FIRRemoteConfig);
struct RemoteConfigIosState;
class RemoteConfigInternal {
 public:
  explicit RemoteConfigInternal(const firebase::App& app);
  ~RemoteConfigInternal();
  RemoteConfigInternal(const RemoteConfigInternal&) = delete;
  RemoteConfigInternal& operator=(const RemoteConfigInternal&) = delete;
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
  Future<void> SetDefaultsLastResult();
  Future<void> SetConfigSettings(ConfigSettings settings);
  Future<void> SetConfigSettingsLastResult();
  Future<void> SetCustomSignals(
      const std::map<std::string, Variant>& custom_signals);
  Future<void> SetCustomSignalsLastResult();
  ConfigSettings GetConfigSettings() const;
  bool GetBoolean(const char* key, ValueInfo* info);
  std::string GetString(const char* key, ValueInfo* info);
  int64_t GetLong(const char* key, ValueInfo* info);
  double GetDouble(const char* key, ValueInfo* info);
  std::vector<unsigned char> GetData(const char* key, ValueInfo* info);
  std::vector<std::string> GetKeys();
  std::vector<std::string> GetKeysByPrefix(const char* prefix);
  std::map<std::string, Variant> GetAll();
  ConfigInfo GetInfo() const;
  ConfigUpdateListenerRegistration AddOnConfigUpdateListener(
      std::function<void(ConfigUpdate&&, RemoteConfigError)>
          config_update_listener);
  bool Initialized() const;
  void Cleanup();
  CleanupNotifier& cleanup_notifier() { return cleanup_; }
 private:
#ifdef __OBJC__
  FIRRemoteConfig* _Nullable impl() const {
    return impl_ ? impl_->get() : nil;
  }
#endif
  const firebase::App& app_;
  std::unique_ptr<FIRRemoteConfigPointer> impl_;
  std::shared_ptr<RemoteConfigIosState> state_;
  CleanupNotifier cleanup_;
};
}  // namespace internal
}  // namespace remote_config
}  // namespace firebase
#endif
