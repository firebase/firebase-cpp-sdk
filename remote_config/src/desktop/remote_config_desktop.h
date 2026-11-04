#ifndef FIREBASE_REMOTE_CONFIG_SRC_DESKTOP_REMOTE_CONFIG_DESKTOP_H_
#define FIREBASE_REMOTE_CONFIG_SRC_DESKTOP_REMOTE_CONFIG_DESKTOP_H_
#include <cstdint>
#include <optional>
#include <string>
#include <thread>
#include "app/src/cleanup_notifier.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/reference_counted_future_impl.h"
#include "app/src/safe_reference.h"
#include "app/src/scheduler.h"
#include "firebase/app.h"
#include "firebase/future.h"
#include "remote_config/src/desktop/config_data.h"
#include "remote_config/src/desktop/file_manager.h"
#include "remote_config/src/desktop/notification_channel.h"
#include "remote_config/src/desktop/rest.h"
#include "remote_config/src/include/firebase/remote_config.h"
#ifdef FIREBASE_TESTING
#include "gtest/gtest.h"
#endif
#ifndef SWIG
#include "firebase/variant.h"
#endif
namespace firebase::remote_config::internal {
const char* const kFutureNoErrorMessage = "No Error Message";
class RemoteConfigInternal {
 public:
#ifdef FIREBASE_TESTING
  friend class RemoteConfigDesktopTest;
  FRIEND_TEST(RemoteConfigDesktopTest, GetBoolean);
  FRIEND_TEST(RemoteConfigDesktopTest, GetLong);
  FRIEND_TEST(RemoteConfigDesktopTest, GetDouble);
  FRIEND_TEST(RemoteConfigDesktopTest, GetString);
  FRIEND_TEST(RemoteConfigDesktopTest, GetData);
  FRIEND_TEST(RemoteConfigDesktopTest, GetKeys);
  FRIEND_TEST(RemoteConfigDesktopTest, GetKeysByPrefix);
  FRIEND_TEST(RemoteConfigDesktopTest, FailedLoadFromFile);
  FRIEND_TEST(RemoteConfigDesktopTest, SuccessLoadFromFile);
  FRIEND_TEST(RemoteConfigDesktopTest, SuccessAsyncSaveToFile);
  FRIEND_TEST(RemoteConfigDesktopTest, SetDefaultsKeyValueVariant);
  FRIEND_TEST(RemoteConfigDesktopTest, SetDefaultsKeyValue);
  FRIEND_TEST(RemoteConfigDesktopTest, ActivateFetched);
  FRIEND_TEST(RemoteConfigDesktopTest, Fetch);
  FRIEND_TEST(RemoteConfigDesktopTest, SetCustomSignals);
  FRIEND_TEST(RemoteConfigDesktopTest, SetCustomSignalsMergeAndRemove);
  FRIEND_TEST(RemoteConfigDesktopTest, SetCustomSignalsValidation);
  FRIEND_TEST(RemoteConfigDesktopTest, SetCustomSignalsPersistence);
#endif
  explicit RemoteConfigInternal(const firebase::App& app,
                                const RemoteConfigFileManager& file_manager);
  explicit RemoteConfigInternal(const firebase::App& app);
  RemoteConfigInternal(const RemoteConfigInternal&) = delete;
  RemoteConfigInternal& operator=(const RemoteConfigInternal&) = delete;
  RemoteConfigInternal(RemoteConfigInternal&&) = delete;
  RemoteConfigInternal& operator=(RemoteConfigInternal&&) = delete;
  ~RemoteConfigInternal();
  Future<ConfigInfo> EnsureInitialized();
  Future<ConfigInfo> EnsureInitializedLastResult();
  Future<bool> Activate();
  Future<bool> ActivateLastResult();
  Future<bool> FetchAndActivate();
  Future<bool> FetchAndActivateLastResult();
  Future<void> Fetch(uint64_t cache_expiration_in_seconds);
  Future<void> FetchLastResult();
  Future<void> SetConfigSettings(ConfigSettings settings);
  Future<void> SetConfigSettingsLastResult();
  [[nodiscard]] ConfigSettings GetConfigSettings();
  Future<void> SetCustomSignals(
      const std::map<std::string, Variant>& custom_signals);
  Future<void> SetCustomSignalsLastResult();
#ifndef SWIG
  Future<void> SetDefaults(const ConfigKeyValueVariant* defaults,
                           size_t number_of_defaults);
#endif
  Future<void> SetDefaults(const ConfigKeyValue* defaults,
                           size_t number_of_defaults);
  Future<void> SetDefaultsLastResult();
  [[nodiscard]] std::string GetConfigSetting(ConfigSetting setting);
  void SetConfigSetting(ConfigSetting setting, const char* value);
  [[nodiscard]] bool GetBoolean(const char* key, ValueInfo* info);
  [[nodiscard]] std::string GetString(const char* key, ValueInfo* info);
  [[nodiscard]] int64_t GetLong(const char* key, ValueInfo* info);
  [[nodiscard]] double GetDouble(const char* key, ValueInfo* info);
  [[nodiscard]] std::vector<unsigned char> GetData(const char* key,
                                                   ValueInfo* info);
  [[nodiscard]] std::vector<std::string> GetKeys();
  [[nodiscard]] std::vector<std::string> GetKeysByPrefix(const char* prefix);
  [[nodiscard]] std::map<std::string, Variant> GetAll();
  bool ActivateFetched();
  [[nodiscard]] ConfigInfo GetInfo() const;
  [[nodiscard]] ConfigUpdateListenerRegistration AddOnConfigUpdateListener(
      std::function<void(ConfigUpdate&&, RemoteConfigError)>
          config_update_listener);
  [[nodiscard]] static bool IsBoolTrue(const std::string& str);
  [[nodiscard]] static bool IsBoolFalse(const std::string& str);
  [[nodiscard]] static bool ConvertToBool(const std::string& from, bool* out);
  [[nodiscard]] static bool IsLong(const std::string& str);
  [[nodiscard]] static bool ConvertToLong(const std::string& from,
                                          int64_t* out);
  [[nodiscard]] static bool IsDouble(const std::string& str);
  [[nodiscard]] static bool ConvertToDouble(const std::string& from,
                                            double* out);
  [[nodiscard]] static Variant StringToVariant(const std::string& from);
  [[nodiscard]] bool Initialized() const;
  void Cleanup();
  CleanupNotifier& cleanup_notifier() { return cleanup_; }
 private:
  void AsyncSaveToFile();
  void InternalInit();
  [[nodiscard]] std::optional<std::string> VariantToString(
      const Variant& variant);
  void SetDefaults(const std::map<std::string, std::string>& defaults_map);
  bool CheckValueInActiveAndDefault(const char* key, ValueInfo* info,
                                    std::string* value);
  bool CheckValueInConfigLocked(const NamespacedConfigData& config,
                                ValueSource source, const char* key,
                                ValueInfo* info, std::string* value);
  bool IsCacheExpiredLocked(uint64_t cache_expiration_in_seconds) const;
  void FetchInternal();
  static const char* const kDefaultNamespace;
  static const char* const kDefaultValueForString;
  static const int64_t kDefaultValueForLong;
  static const double kDefaultValueForDouble;
  static const bool kDefaultValueForBool;
  const std::vector<unsigned char> kDefaultValueForData;
  const firebase::App& app_;
  LayeredConfigs configs_;
  RemoteConfigFileManager file_manager_;
  std::thread save_thread_;
  NotificationChannel save_channel_;
  uint64_t cache_expiration_in_seconds_;
  bool is_fetch_process_have_task_;
  mutable Mutex internal_mutex_;
  ReferenceCountedFutureImpl future_impl_;
  scheduler::Scheduler scheduler_;
  typedef firebase::internal::SafeReference<RemoteConfigInternal> ThisRef;
  typedef firebase::internal::SafeReferenceLock<RemoteConfigInternal>
      ThisRefLock;
  ThisRef safe_this_;
  RemoteConfigREST rest_;
  bool initialized_;
  ConfigSettings config_settings_;
  CleanupNotifier cleanup_;
};
}  // namespace firebase::remote_config::internal
#endif
