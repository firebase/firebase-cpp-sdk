// Copyright 2019 Google LLC
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
#include "app/src/reference_count.h"
#include "app/src/reference_counted_future_impl.h"
#include "app/src/util_android.h"
#include "firebase/app.h"
#include "firebase/future.h"
#include "remote_config/src/include/firebase/remote_config.h"

namespace firebase::remote_config::internal {
// Remote Config Client implementation for Android.
//
// This class implements functions from `firebase/remote_config.h` header.
// See `firebase/remote_config.h` for all public functions documentation.
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
#endif  // SWIG
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

  // When this is deleted, it will clean up all ListenerRegistrations.
  [[nodiscard]] CleanupNotifier& cleanup_notifier() { return cleanup_; }

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
  static firebase::internal::ReferenceCount initializer_;
  // app
  const firebase::App& app_;

  /// Handle calls from Futures that the API returns.
  ReferenceCountedFutureImpl future_impl_;

  CleanupNotifier cleanup_;

  jobject internal_obj_ = nullptr;
  bool initialized_ref_ = false;

  Mutex default_key_mutex_;
  std::vector<std::string> default_keys_;

  // If a fetch was throttled, this is set to the time when throttling is
  // finished, in milliseconds since epoch.
  std::atomic<int64_t> throttled_end_time_{0};

  // String to be used when registering for JNI task callbacks.
  std::string jni_task_id_;
};

}  // namespace firebase::remote_config::internal

#endif  // FIREBASE_REMOTE_CONFIG_SRC_ANDROID_REMOTE_CONFIG_ANDROID_H_
