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
#include "firebase/future.h"
#include "remote_config/src/desktop/config_data.h"
#include "remote_config/src/desktop/file_manager.h"
#include "remote_config/src/desktop/notification_channel.h"
#include "remote_config/src/include/firebase/remote_config.h"

#ifdef FIREBASE_TESTING
#include "testing/base/public/gunit.h"
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
class RemoteConfigDesktop {
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

  explicit RemoteConfigDesktop(const firebase::App& app,
                               const RemoteConfigFileManager& file_manager);
  ~RemoteConfigDesktop();

#ifndef SWIG
  void SetDefaults(const ConfigKeyValueVariant* defaults,
                   size_t number_of_defaults, const char* config_namespace);
#endif  // SWIG
  void SetDefaults(const ConfigKeyValue* defaults, size_t number_of_defaults,
                   const char* config_namespace);

  std::string GetConfigSetting(ConfigSetting setting);
  void SetConfigSetting(ConfigSetting setting, const char* value);

  bool GetBoolean(const char* key, const char* config_namespace,
                  ValueInfo* info);

  std::string GetString(const char* key, const char* config_namespace,
                        ValueInfo* info);

  int64_t GetLong(const char* key, const char* config_namespace,
                  ValueInfo* info);

  double GetDouble(const char* key, const char* config_namespace,
                   ValueInfo* info);

  std::vector<unsigned char> GetData(const char* key,
                                     const char* config_namespace,
                                     ValueInfo* info);

  std::vector<std::string> GetKeys(const char* config_namespace);

  std::vector<std::string> GetKeysByPrefix(const char* prefix,
                                           const char* config_namespace);

  bool ActivateFetched();

  const ConfigInfo& GetInfo() const;

  Future<void> Fetch(uint64_t cache_expiration_in_seconds);

  Future<void> FetchLastResult();

  static bool IsBoolTrue(const std::string& str);
  static bool IsBoolFalse(const std::string& str);
  static bool IsLong(const std::string& str);
  static bool IsDouble(const std::string& str);

 private:
  // Open a new thread for saving state in the file. Thread will wait
  // notifications in loop from the `save_channel_` until it will be closed.
  void AsyncSaveToFile();

  // Open a new thread for fetching fresh config. Thread will wait nofitications
  // in loop from the `fetch_channel_` until it will be closed.
  void AsyncFetch();

  // Convert the `firebase::Variant` type to the `std::string` type.
  //
  // Support only boolean, integer, double, string values and binary data.
  // Otherwise will assign `true` to the `failure` variable.
  std::string VariantToString(const Variant& variant, bool* failure);

  // Set default values by namespace to `configs_.defaults` holder.
  void SetDefaults(const std::map<std::string, std::string>& defaults_map,
                   const char* config_namespace);

  // Return true and assign the found record to the `value` if `active` or
  // `defaults` holders contatin record by the key and the namespace.
  //
  // Assing `info->source` If info is not nullptr.
  bool CheckValueInActiveAndDefault(const char* key,
                                    const char* config_namespace,
                                    ValueInfo* info, std::string* value);

  // Return true and assign the found record to the `value` if `holder`
  // contatins record by the key and the namespace.
  //
  // Assing `info->source` If info is not nullptr.
  bool CheckValueInConfig(const NamespacedConfigData& config,
                          ValueSource source, const char* key,
                          const char* config_namespace, ValueInfo* info,
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

  // Used for fetching the fresh config in background. Will be created in the
  // constructor and removed in the destructor.
  //
  // Wait notifications in loop from `fetch_channel_` until it will be closed.
  std::thread fetch_thread_;

  // Thread safety notification channel.
  //
  // Call non blocking `fetch_channel_.Put()` function to fetch config. Call
  // `fetch_channel_.Close()` to close the channel.
  NotificationChannel fetch_channel_;

  // Create new FutureHandle when it's possible to start fetching and complete
  // future with `fetch_handle_` after fetching.
  FutureHandle fetch_handle_;

  // Last value of `Fetch` function argument. Update only if we will fetch.
  uint64_t cache_expiration_in_seconds_;

  // Avoid using more than one fetching process per time.
  //
  // Call `fetch_channel_.Put()` and assing `true` only if value is
  // `false`. Fetching thread will notify and fetch config. When fetching
  // will finish it will be assigned to `false`.
  bool is_fetch_process_have_task_;

  mutable std::mutex mutex_;
};

}  // namespace internal
}  // namespace remote_config
}  // namespace firebase

#endif  // FIREBASE_REMOTE_CONFIG_CLIENT_CPP_SRC_DESKTOP_REMOTE_CONFIG_DESKTOP_H_
