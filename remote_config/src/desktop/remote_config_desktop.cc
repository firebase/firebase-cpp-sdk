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

#include "remote_config/src/desktop/remote_config_desktop.h"

#include <cctype>
#include <chrono>  // NOLINT
#include <cstdint>
#include <cstdlib>
#include <set>
#include <sstream>
#include <string>
#include <thread>  // NOLINT
#include <vector>

#include "app/src/callback.h"
#include "app/src/time.h"
#include "remote_config/src/common.h"
#include "remote_config/src/desktop/rest.h"

#ifndef SWIG
#include "firebase/variant.h"
#endif  // SWIG

namespace firebase {
namespace remote_config {
namespace internal {

using callback::NewCallback;

const char* const RemoteConfigInternal::kDefaultNamespace = "configns:firebase";
const char* const RemoteConfigInternal::kDefaultValueForString = "";
const int64_t RemoteConfigInternal::kDefaultValueForLong = 0L;
const double RemoteConfigInternal::kDefaultValueForDouble = 0.0;
const bool RemoteConfigInternal::kDefaultValueForBool = false;

static const char* kFilePathSuffix = "remote_config_data";

template <typename T>
struct RCDataHandle {
  RCDataHandle(
      ReferenceCountedFutureImpl* _future_api,
      const SafeFutureHandle<T>& _future_handle,
      RemoteConfigInternal* _rc_internal,
      std::vector<std::string> _default_keys = std::vector<std::string>())
      : future_api(_future_api),
        future_handle(_future_handle),
        rc_internal(_rc_internal),
        default_keys(_default_keys) {}
  ReferenceCountedFutureImpl* future_api;
  SafeFutureHandle<T> future_handle;
  RemoteConfigInternal* rc_internal;
  std::vector<std::string> default_keys;
};

RemoteConfigInternal::RemoteConfigInternal(
    const firebase::App& app, const RemoteConfigFileManager& file_manager)
    : app_(app),
      file_manager_(file_manager),
      is_fetch_process_have_task_(false),
      future_impl_(kRemoteConfigFnCount),
      safe_this_(this) {
  InternalInit();
}

RemoteConfigInternal::RemoteConfigInternal(const firebase::App& app)
    : app_(app),
      file_manager_(kFilePathSuffix),
      is_fetch_process_have_task_(false),
      future_impl_(kRemoteConfigFnCount),
      safe_this_(this) {
  InternalInit();
}

RemoteConfigInternal::~RemoteConfigInternal() {
  save_channel_.Close();
  if (save_thread_.joinable()) {
    save_thread_.join();
  }

  safe_this_.ClearReference();
}

void RemoteConfigInternal::InternalInit() {
  file_manager_.Load(&configs_);
  AsyncSaveToFile();
}

bool RemoteConfigInternal::Initialized() const{
  // TODO(cynthiajiang) implement
  return true;
}

void RemoteConfigInternal::Cleanup() {
  // TODO(cynthiajiang) implement
}

Future<ConfigInfo> RemoteConfigInternal::EnsureInitialized() {
  const auto handle =
      future_impl_.SafeAlloc<ConfigInfo>(kRemoteConfigFnEnsureInitialized);
  // TODO(cynthiajiang) implement
  return MakeFuture<ConfigInfo>(&future_impl_, handle);
}

Future<ConfigInfo> RemoteConfigInternal::EnsureInitializedLastResult() {
  return static_cast<const Future<ConfigInfo>&>(
      future_impl_.LastResult(kRemoteConfigFnEnsureInitialized));
}

Future<bool> RemoteConfigInternal::Activate() {
  const auto handle = future_impl_.SafeAlloc<bool>(kRemoteConfigFnActivate);
  // TODO(cynthiajiang) implement
  return MakeFuture<bool>(&future_impl_, handle);
}

Future<bool> RemoteConfigInternal::ActivateLastResult() {
  return static_cast<const Future<bool>&>(
      future_impl_.LastResult(kRemoteConfigFnActivate));
}

Future<bool> RemoteConfigInternal::FetchAndActivate() {
  const auto handle =
      future_impl_.SafeAlloc<bool>(kRemoteConfigFnFetchAndActivate);
  // TODO(cynthiajiang) implement
  return MakeFuture<bool>(&future_impl_, handle);
}

Future<bool> RemoteConfigInternal::FetchAndActivateLastResult() {
  return static_cast<const Future<bool>&>(
      future_impl_.LastResult(kRemoteConfigFnFetchAndActivate));
}

Future<void> RemoteConfigInternal::SetDefaultsLastResult() {
  return static_cast<const Future<void>&>(
      future_impl_.LastResult(kRemoteConfigFnSetDefaults));
}

#ifdef FIREBASE_EARLY_ACCESS_PREVIEW
Future<void> RemoteConfigInternal::SetConfigSettings(ConfigSettings settings) {
  const auto handle =
      future_impl_.SafeAlloc<void>(kRemoteConfigFnSetConfigSettings);
  // TODO(cynthiajiang) implement
  return MakeFuture<void>(&future_impl_, handle);
}
#endif  // FIREBASE_EARLY_ACCESS_PREVIEW

Future<void> RemoteConfigInternal::SetConfigSettingsLastResult() {
  return static_cast<const Future<void>&>(
      future_impl_.LastResult(kRemoteConfigFnSetConfigSettings));
}

void RemoteConfigInternal::AsyncSaveToFile() {
  save_thread_ = std::thread([this]() {
    while (save_channel_.Get()) {
      LayeredConfigs copy;
      {
        MutexLock lock(internal_mutex_);
        copy = configs_;
      }
      file_manager_.Save(copy);
    }
  });
}

std::string RemoteConfigInternal::VariantToString(const Variant& variant,
                                                  bool* failure) {
  if (variant.is_blob()) {
    const uint8_t* blob_data = variant.blob_data();
    size_t blob_size = variant.blob_size();
    std::string s;
    s.resize(blob_size);
    for (size_t i = 0; i < blob_size; i++) {
      s[i] = static_cast<char>(blob_data[i]);
    }
    return s;
  }

  if (!variant.is_bool() && !variant.is_int64() && !variant.is_double() &&
      !variant.is_string()) {
    *failure = true;
    return "";
  }
  return variant.AsString().string_value();
}

#ifndef SWIG
Future<void> RemoteConfigInternal::SetDefaults(
    const ConfigKeyValueVariant* defaults, size_t number_of_defaults) {
  const auto handle = future_impl_.SafeAlloc<void>(kRemoteConfigFnSetDefaults);
  // TODO(cynthiajiang) : wrap following blocking call into thread.
  if (defaults == nullptr) {
    return MakeFuture<void>(&future_impl_, handle);
  }
  std::map<std::string, std::string> defaults_map;
  for (size_t i = 0; i < number_of_defaults; i++) {
    const char* key = defaults[i].key;
    bool failure = false;
    std::string value = VariantToString(defaults[i].value, &failure);
    if (key && !failure) {
      defaults_map[key] = value;
    }
  }
  SetDefaults(defaults_map);
  return MakeFuture<void>(&future_impl_, handle);
}
#endif  // SWIG

Future<void> RemoteConfigInternal::SetDefaults(const ConfigKeyValue* defaults,
                                               size_t number_of_defaults) {
  const auto handle = future_impl_.SafeAlloc<void>(kRemoteConfigFnSetDefaults);
  // TODO(cynthiajiang) : wrap following blocking call into thread.

  if (defaults == nullptr) {
    return MakeFuture<void>(&future_impl_, handle);
  }
  std::map<std::string, std::string> defaults_map;
  for (size_t i = 0; i < number_of_defaults; i++) {
    const char* key = defaults[i].key;
    const char* value = defaults[i].value;
    if (key && value) {
      defaults_map[key] = value;
    }
  }
  SetDefaults(defaults_map);
  return MakeFuture<void>(&future_impl_, handle);
}

void RemoteConfigInternal::SetDefaults(
    const std::map<std::string, std::string>& defaults_map) {
  {
    MutexLock lock(internal_mutex_);
    configs_.defaults.SetNamespace(defaults_map, kDefaultNamespace);
  }
  save_channel_.Put();
}

std::string RemoteConfigInternal::GetConfigSetting(ConfigSetting setting) {
  MutexLock lock(internal_mutex_);
  return configs_.metadata.GetSetting(setting);
}

void RemoteConfigInternal::SetConfigSetting(ConfigSetting setting,
                                            const char* value) {
  if (value == nullptr) {
    return;
  }
  {
    MutexLock lock(internal_mutex_);
    configs_.metadata.AddSetting(setting, value);
  }
  save_channel_.Put();
}

bool RemoteConfigInternal::CheckValueInActiveAndDefault(const char* key,
                                                        ValueInfo* info,
                                                        std::string* value) {
  return CheckValueInConfig(configs_.active, kValueSourceRemoteValue, key, info,
                            value) ||
         CheckValueInConfig(configs_.defaults, kValueSourceDefaultValue, key,
                            info, value);
}

bool RemoteConfigInternal::CheckValueInConfig(
    const NamespacedConfigData& config, ValueSource source, const char* key,
    ValueInfo* info, std::string* value) {
  if (!key) return false;

  {
    MutexLock lock(internal_mutex_);
    if (!config.HasValue(key, kDefaultNamespace)) {
      return false;
    }
    *value = config.GetValue(key, kDefaultNamespace);
  }

  if (info) info->source = source;
  return true;
}

bool RemoteConfigInternal::IsBoolTrue(const std::string& str) {
  // regex: ^(1|true|t|yes|y|on)$
  return str == "1" || str == "true" || str == "t" || str == "yes" ||
         str == "y" || str == "on";
}

bool RemoteConfigInternal::IsBoolFalse(const std::string& str) {
  // regex: ^(0|false|f|no|n|off)$
  return str == "0" || str == "false" || str == "f" || str == "no" ||
         str == "n" || str == "off";
}

bool RemoteConfigInternal::IsLong(const std::string& str) {
  // regex: ^[-+]?[0-9]+$
  // Don't allow empty string.
  if (str.length() == 0) return false;
  // Don't allow leading space (strtol trims it).
  if (isspace(str[0])) return false;
  char* endptr;
  (void)strtol(str.c_str(), &endptr, 10);  // NOLINT
  return (*endptr == '\0');  // Ensure we consumed the whole string.
}

bool RemoteConfigInternal::IsDouble(const std::string& str) {
  // regex: ^[-+]?[0-9]*.?[0-9]+([eE][-+]?[0-9]+)?
  // Don't allow empty string.
  if (str.length() == 0) return false;
  // Don't allow leading space (strtod trims it).
  if (isspace(str[0])) return false;
  char* endptr;
  (void)strtod(str.c_str(), &endptr);
  return (*endptr == '\0');  // Ensure we consumed the whole string.
}

bool RemoteConfigInternal::GetBoolean(const char* key, ValueInfo* info) {
  std::string value;
  if (!CheckValueInActiveAndDefault(key, info, &value)) {
    if (info) {
      info->source = kValueSourceStaticValue;
      info->conversion_successful = true;
    }
    return kDefaultValueForBool;
  }

  if (IsBoolTrue(value)) {
    if (info) info->conversion_successful = true;
    return true;
  }
  if (IsBoolFalse(value)) {
    if (info) info->conversion_successful = true;
    return false;
  }
  if (info) info->conversion_successful = false;
  return kDefaultValueForBool;
}

std::string RemoteConfigInternal::GetString(const char* key, ValueInfo* info) {
  std::string value;
  if (!CheckValueInActiveAndDefault(key, info, &value)) {
    if (info) {
      info->source = kValueSourceStaticValue;
      info->conversion_successful = true;
    }
    return kDefaultValueForString;
  }

  if (info) info->conversion_successful = true;
  return value;
}

int64_t RemoteConfigInternal::GetLong(const char* key, ValueInfo* info) {
  std::string value;
  if (!CheckValueInActiveAndDefault(key, info, &value)) {
    if (info) {
      info->source = kValueSourceStaticValue;
      info->conversion_successful = true;
    }
    return kDefaultValueForLong;
  }

  int64_t long_value = kDefaultValueForLong;
  bool convertation_failure = !IsLong(value);

  std::stringstream convertor;
  convertor << value;
  convertor >> long_value;
  convertation_failure = convertation_failure || convertor.fail();

  if (info) info->conversion_successful = !convertation_failure;
  return convertation_failure ? kDefaultValueForLong : long_value;
}

double RemoteConfigInternal::GetDouble(const char* key, ValueInfo* info) {
  std::string value;
  if (!CheckValueInActiveAndDefault(key, info, &value)) {
    if (info) {
      info->source = kValueSourceStaticValue;
      info->conversion_successful = true;
    }
    return kDefaultValueForDouble;
  }

  double double_value = kDefaultValueForDouble;
  bool convertation_failure = !IsDouble(value);

  std::stringstream convertor;
  convertor << value;
  convertor >> double_value;
  convertation_failure = convertation_failure || convertor.fail();

  if (info) info->conversion_successful = !convertation_failure;
  return convertation_failure ? kDefaultValueForDouble : double_value;
}

std::vector<unsigned char> RemoteConfigInternal::GetData(const char* key,
                                                         ValueInfo* info) {
  std::string value;
  if (!CheckValueInActiveAndDefault(key, info, &value)) {
    if (info) {
      info->source = kValueSourceStaticValue;
      info->conversion_successful = true;
    }
    return kDefaultValueForData;
  }

  std::vector<unsigned char> data_value;
  data_value.reserve(value.size());
  for (const char& ch : value)
    data_value.push_back(static_cast<unsigned char>(ch));

  if (info) info->conversion_successful = true;
  return data_value;
}

std::vector<std::string> RemoteConfigInternal::GetKeys() {
  return GetKeysByPrefix("");
}

std::vector<std::string> RemoteConfigInternal::GetKeysByPrefix(
    const char* prefix) {
  if (prefix == nullptr) return std::vector<std::string>();
  std::set<std::string> unique_keys;
  {
    MutexLock lock(internal_mutex_);
    configs_.active.GetKeysByPrefix(prefix, kDefaultNamespace, &unique_keys);
    configs_.defaults.GetKeysByPrefix(prefix, kDefaultNamespace, &unique_keys);
  }
  return std::vector<std::string>(unique_keys.begin(), unique_keys.end());
}

std::map<std::string, Variant> RemoteConfigInternal::GetAll() {
  std::map<std::string, Variant> value;
  // TODO(cynthiajiang) implement
  return value;
}

bool RemoteConfigInternal::ActivateFetched() {
  {
    MutexLock lock(internal_mutex_);
    // Fetched config not found or already activated.
    if (configs_.fetched.timestamp() <= configs_.active.timestamp())
      return false;
    configs_.active = configs_.fetched;
  }
  save_channel_.Put();
  return true;
}

const ConfigInfo RemoteConfigInternal::GetInfo() const {
  MutexLock lock(internal_mutex_);
  return configs_.metadata.info();
}

Future<void> RemoteConfigInternal::Fetch(uint64_t cache_expiration_in_seconds) {
  MutexLock lock(internal_mutex_);

  uint64_t milliseconds_since_epoch =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::system_clock::now().time_since_epoch())
          .count();

  uint64_t cache_expiration_timestamp =
      configs_.fetched.timestamp() +
      ::firebase::internal::kMillisecondsPerSecond *
          cache_expiration_in_seconds;

  const auto future_handle = future_impl_.SafeAlloc<void>(kRemoteConfigFnFetch);

  // Need to fetch in two cases:
  // - we are not fetching at this moment
  // - cache_expiration_in_seconds is zero
  // or
  // - we are not fetching at this moment
  // - cache (fetched) data is older than cache_expiration_in_seconds
  if (!is_fetch_process_have_task_ &&
      ((cache_expiration_in_seconds == 0) ||
       (cache_expiration_timestamp < milliseconds_since_epoch))) {
    auto data_handle =
        MakeShared<RCDataHandle<void>>(&future_impl_, future_handle, this);

    auto callback = NewCallback(
        [](ThisRef ref, SharedPtr<RCDataHandle<void>> handle) {
          ThisRefLock lock(&ref);
          if (lock.GetReference() != nullptr) {
            MutexLock lock(handle->rc_internal->internal_mutex_);
            RemoteConfigREST* rest = new RemoteConfigREST(
                handle->rc_internal->app_.options(),
                handle->rc_internal->configs_,
                handle->rc_internal->cache_expiration_in_seconds_);

            // Fetch fresh config from server.
            rest->Fetch(handle->rc_internal->app_);

            // Need to copy everything to `configs_.fetched`.
            handle->rc_internal->configs_.fetched = rest->fetched();

            // Need to copy only info and digests to `configs_.metadata`.
            const RemoteConfigMetadata& metadata = rest->metadata();
            handle->rc_internal->configs_.metadata.set_info(metadata.info());
            handle->rc_internal->configs_.metadata.set_digest_by_namespace(
                metadata.digest_by_namespace());

            delete rest;

            handle->rc_internal->is_fetch_process_have_task_ = false;

            FutureStatus futureResult =
                (handle->rc_internal->GetInfo().last_fetch_status ==
                 kLastFetchStatusSuccess)
                    ? kFutureStatusSuccess
                    : kFutureStatusFailure;
            handle->future_api->Complete(handle->future_handle, futureResult);
          }
        },
        safe_this_, data_handle);

    scheduler_.Schedule(callback);
    is_fetch_process_have_task_ = true;
    cache_expiration_in_seconds_ = cache_expiration_in_seconds;
  } else {
    // Do not fetch, complete future immediately.
    future_impl_.Complete(future_handle, kFutureStatusSuccess);
  }
  return MakeFuture<void>(&future_impl_, future_handle);
}

Future<void> RemoteConfigInternal::FetchLastResult() {
  return static_cast<const Future<void>&>(
      future_impl_.LastResult(kRemoteConfigFnFetch));
}

}  // namespace internal
}  // namespace remote_config
}  // namespace firebase
