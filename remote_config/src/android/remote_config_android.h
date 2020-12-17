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

#ifndef FIREBASE_REMOTE_CONFIG_CLIENT_CPP_SRC_ANDROID_REMOTE_CONFIG_ANDROID_H_
#define FIREBASE_REMOTE_CONFIG_CLIENT_CPP_SRC_ANDROID_REMOTE_CONFIG_ANDROID_H_

#include "firebase/app.h"
#include "app/meta/move.h"
#include "app/src/mutex.h"
#include "app/src/reference_count.h"
#include "app/src/reference_counted_future_impl.h"
#include "app/src/util_android.h"
#include "app/src/include/firebase/internal/common.h"
#include "firebase/future.h"
#include "remote_config/src/include/firebase/remote_config.h"

namespace firebase {
namespace remote_config {
namespace internal {
// Remote Config Client implementation for Android.
//
// This class implements functions from `firebase/remote_config.h` header.
// See `firebase/remote_config.h` for all public functions documentation.
class RemoteConfigInternal {
 public:
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

#ifndef SWIG
  Future<void> SetDefaults(const ConfigKeyValueVariant* defaults,
                           size_t number_of_defaults);
#endif  // SWIG
  Future<void> SetDefaults(const ConfigKeyValue* defaults,
                           size_t number_of_defaults);

  Future<void> SetDefaults(int defaults_resource_id);

  Future<void> SetDefaultsLastResult();
#ifdef FIREBASE_EARLY_ACCESS_PREVIEW
  Future<void> SetConfigSettings(ConfigSettings settings);
  Future<void> SetConfigSettingsLastResult();
#endif  // FIREBASE_EARLY_ACCESS_PREVIEW
  bool GetBoolean(const char* key, ValueInfo* info);

  std::string GetString(const char* key, ValueInfo* info);

  int64_t GetLong(const char* key, ValueInfo* info);

  double GetDouble(const char* key, ValueInfo* info);

  std::vector<unsigned char> GetData(const char* key, ValueInfo* info);

  std::vector<std::string> GetKeys();

  std::vector<std::string> GetKeysByPrefix(const char* prefix);

  std::map<std::string, Variant> GetAll();

  const ConfigInfo GetInfo() const;

  bool Initialized() const;

  void Cleanup();

  void set_throttled_end_time(int64_t end_time) {
    throttled_end_time_ = end_time;
  }

  void SaveTmpKeysToDefault(std::vector<std::string> tmp_default_keys) {
    MutexLock lock(default_key_mutex_);
#if defined(FIREBASE_USE_MOVE_OPERATORS)
    default_keys_ = firebase::Move(tmp_default_keys);
#else
    default_keys_ = tmp_default_keys;
#endif  // FIREBASE_USE_MOVE_OPERATORS
  }

 private:
  static firebase::internal::ReferenceCount initializer_;
  // app
  const firebase::App& app_;

  /// Handle calls from Futures that the API returns.
  ReferenceCountedFutureImpl future_impl_;

  jobject internal_obj_;

  Mutex default_key_mutex_;
  std::vector<std::string> tmp_default_keys_;
  std::vector<std::string> default_keys_;

  // If a fetch was throttled, this is set to the time when throttling is
  // finished, in milliseconds since epoch.
  int64_t throttled_end_time_ = 0;
};

}  // namespace internal
}  // namespace remote_config
}  // namespace firebase

#endif  // FIREBASE_REMOTE_CONFIG_CLIENT_CPP_SRC_ANDROID_REMOTE_CONFIG_ANDROID_H_
