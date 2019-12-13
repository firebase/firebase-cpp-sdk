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

#ifndef FIREBASE_REMOTE_CONFIG_CLIENT_CPP_SRC_IOS_REMOTE_CONFIG_IOS_H_
#define FIREBASE_REMOTE_CONFIG_CLIENT_CPP_SRC_IOS_REMOTE_CONFIG_IOS_H_

#include "firebase/app.h"
#include "app/src/reference_counted_future_impl.h"
#include "firebase/future.h"
#include "remote_config/src/include/firebase/remote_config.h"

namespace firebase {
namespace remote_config {
namespace internal {
// Remote Config Client implementation for iOS.
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

  Future<void> SetDefaultsLastResult();
#ifdef FIREBASE_EARLY_ACCESS_PREVIEW
  Future<void> SetConfigSettings(ConfigSettings settings);
#endif  // #ifdef FIREBASE_EARLY_ACCESS_PREVIEW
  Future<void> SetConfigSettingsLastResult();

  bool GetBoolean(const char* key, ValueInfo* info);

  std::string GetString(const char* key, ValueInfo* info);

  int64_t GetLong(const char* key, ValueInfo* info);

  double GetDouble(const char* key, ValueInfo* info);

  std::vector<unsigned char> GetData(const char* key, ValueInfo* info);

  std::vector<std::string> GetKeys();

  std::vector<std::string> GetKeysByPrefix(const char* prefix);

  std::map<std::string, Variant> GetAll();

  const ConfigInfo& GetInfo() const;

  bool Initialized() const;

  void Cleanup();

 private:
  // app
  const firebase::App& app_;

  /// Handle calls from Futures that the API returns.
  ReferenceCountedFutureImpl future_impl_;
};
}  // namespace internal
}  // namespace remote_config
}  // namespace firebase

#endif  // FIREBASE_REMOTE_CONFIG_CLIENT_CPP_SRC_IOS_REMOTE_CONFIG_IOS_H_
