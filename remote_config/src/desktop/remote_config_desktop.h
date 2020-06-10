// Copyright 2017 Google LLC
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

#ifndef FIREBASE_REMOTE_CONFIG_CLIENT_CPP_SRC_DESKTOP_REMOTE_CONFIG_DESKTOP_H_
#define FIREBASE_REMOTE_CONFIG_CLIENT_CPP_SRC_DESKTOP_REMOTE_CONFIG_DESKTOP_H_

#include <cstdint>
#include <string>
#include <thread>  // NOLINT

#include "firebase/app.h"
#include "app/src/mutex.h"
#include "app/src/reference_counted_future_impl.h"
#include "app/src/safe_reference.h"
#include "app/src/scheduler.h"
#include "firebase/future.h"
#include "remote_config/src/desktop/config_data.h"
#include "remote_config/src/desktop/file_manager.h"
#include "remote_config/src/desktop/notification_channel.h"
#include "remote_config/src/include/firebase/remote_config.h"

#ifdef FIREBASE_TESTING
#include "gtest/gtest.h"
#endif  // FIREBASE_TESTING

#ifndef SWIG
#include "firebase/variant.h"
#endif  // SWIG

namespace firebase {
namespace remote_config {
namespace internal {

// Remote Config Client implementation for Desktop support.
//
// This class implements functions from `firebase/remote_config.h` header.
// See `firebase/remote_config.h` for all public functions documentation.
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
#endif  // FIREBASE_TESTING

  explicit RemoteConfigInternal(const firebase::App& app,
                                const RemoteConfigFileManager& file_manager);

  explicit RemoteConfigInternal(const firebase::App& app);

  ~RemoteConfigInternal();

  Future<ConfigInfo> EnsureInitialized();
  Future<ConfigInfo> EnsureInitializedLastResult();

  Future<bool> Activate();
  Future<bool> ActivateLastResult();

  Future<bool> FetchAndActivate();
  Future<bool> FetchAndActivateLastResult();

  Future<void> Fetch(uint64_t cache_expiration_in_seconds);
  Future<void> FetchLastResult();

#ifdef FIREBASE_EARLY_ACCESS_PREVIEW
  Future<void> SetConfigSettings(ConfigSettings settings);
#endif  // FIREBASE_EARLY_ACCESS_PREVIEW
  Future<void> SetConfigSettingsLastResult();

#ifndef SWIG
  Future<void> SetDefaults(const ConfigKeyValueVariant* defaults,
                           size_t number_of_defaults);
#endif  // SWIG
  Future<void> SetDefaults(const ConfigKeyValue* defaults,
                           size_t number_of_defaults);
  Future<void> SetDefaultsLastResult();

  std::string GetConfigSetting(ConfigSetting setting);
  void SetConfigSetting(ConfigSetting setting, const char* value);

  bool GetBoolean(const char* key, ValueInfo* info);

  std::string GetString(const char* key, ValueInfo* info);

  int64_t GetLong(const char* key, ValueInfo* info);

  double GetDouble(const char* key, ValueInfo* info);

  std::vector<unsigned char> GetData(const char* key, ValueInfo* info);

  std::vector<std::string> GetKeys();

  std::vector<std::string> GetKeysByPrefix(const char* prefix);

  std::map<std::string, Variant> GetAll();

  bool ActivateFetched();

  const ConfigInfo GetInfo() const;

  static bool IsBoolTrue(const std::string& str);
  static bool IsBoolFalse(const std::string& str);
  static bool IsLong(const std::string& str);
  static bool IsDouble(const std::string& str);

  bool Initialized() const;
  void Cleanup();

 private:
  // Open a new thread for saving state in the file. Thread will wait
  // notifications in loop from the `save_channel_` until it will be closed.
  void AsyncSaveToFile();

  void InternalInit();

  // Convert the `firebase::Variant` type to the `std::string` type.
  //
  // Support only boolean, integer, double, string values and binary data.
  // Otherwise will assign `true` to the `failure` variable.
  std::string VariantToString(const Variant& variant, bool* failure);

  // Set default values to `configs_.defaults` holder.
  void SetDefaults(const std::map<std::string, std::string>& defaults_map);

  // Returns true and assigns the found record to the `value` if the `active` or
  // `defaults` holders contains a record for the key.
  //
  // Assign `info->source` If info is not nullptr.
  bool CheckValueInActiveAndDefault(const char* key,
                                    ValueInfo* info, std::string* value);

  // Returns true and assigns the found record to the `value` if the `holder`
  // contains a record for the key.
  //
  // Assign `info->source` If info is not nullptr.
  bool CheckValueInConfig(const NamespacedConfigData& config,
                          ValueSource source, const char* key, ValueInfo* info,
                          std::string* value);

  static const char* const kDefaultNamespace;
  static const char* const kDefaultValueForString;
  static const int64_t kDefaultValueForLong;
  static const double kDefaultValueForDouble;
  static const bool kDefaultValueForBool;

  const std::vector<unsigned char> kDefaultValueForData;

  // app
  const firebase::App& app_;

  // Contains all config records and metadata variables.
  LayeredConfigs configs_;

  // Provides saving to the file and load from the tile the `configs_` variable.
  RemoteConfigFileManager file_manager_;

  // Used for saving the `configs_` variable to file in background. Will be
  // created in the constructor and removed in the destructor.
  //
  // Wait notifications in loop from `save_channel_` until it will be closed.
  std::thread save_thread_;

  // Thread safety notification channel.
  //
  // Call non blocking `save_channel_.Put()` function after changing the
  // `configs_` variable. Call `save_channel_.Close()` to close the channel.
  NotificationChannel save_channel_;

  // Last value of `Fetch` function argument. Update only if we will fetch.
  uint64_t cache_expiration_in_seconds_;

  // Avoid using more than one fetching process per time.
  //
  // Call `fetch_channel_.Put()` and assign `true` only if value is
  // `false`. Fetching thread will notify and fetch config. When fetching
  // will finish it will be assigned to `false`.
  bool is_fetch_process_have_task_;

  mutable Mutex internal_mutex_;

  // Handle calls from Futures that the API returns.
  ReferenceCountedFutureImpl future_impl_;

  scheduler::Scheduler scheduler_;

  // Safe reference to this.  Set in constructor and cleared in destructor
  // Should be safe to be copied in any thread because the SharedPtr never
  // changes, until safe_this_ is completely destroyed.
  typedef firebase::internal::SafeReference<RemoteConfigInternal> ThisRef;
  typedef firebase::internal::SafeReferenceLock<RemoteConfigInternal>
      ThisRefLock;
  ThisRef safe_this_;
};

}  // namespace internal
}  // namespace remote_config
}  // namespace firebase

#endif  // FIREBASE_REMOTE_CONFIG_CLIENT_CPP_SRC_DESKTOP_REMOTE_CONFIG_DESKTOP_H_
