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
#include "remote_config/src/include/firebase/remote_config.h"

#ifndef SWIG
#include "firebase/variant.h"
#endif  // SWIG

namespace firebase {
namespace remote_config {
namespace internal {

using callback::NewCallback;

const char* const RemoteConfigInternal::kDefaultNamespace = "firebase";
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
      safe_this_(this),
      rest_(app.options(), configs_, kDefaultNamespace),
      initialized_(false) {
  InternalInit();
}

RemoteConfigInternal::RemoteConfigInternal(const firebase::App& app)
    : app_(app),
      file_manager_(kFilePathSuffix),
      is_fetch_process_have_task_(false),
      future_impl_(kRemoteConfigFnCount),
      safe_this_(this),
      rest_(app.options(), configs_, kDefaultNamespace),
      initialized_(false) {
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
  initialized_ = true;
}

bool RemoteConfigInternal::Initialized() const { return initialized_; }

void RemoteConfigInternal::Cleanup() {
  // Do nothing.
}

Future<ConfigInfo> RemoteConfigInternal::EnsureInitialized() {
  const auto handle =
      future_impl_.SafeAlloc<ConfigInfo>(kRemoteConfigFnEnsureInitialized);
  future_impl_.CompleteWithResult(handle, kFutureStatusSuccess,
                                  kFutureNoErrorMessage,
                                  rest_.metadata().info());
  return MakeFuture<ConfigInfo>(&future_impl_, handle);
}

Future<ConfigInfo> RemoteConfigInternal::EnsureInitializedLastResult() {
  return static_cast<const Future<ConfigInfo>&>(
      future_impl_.LastResult(kRemoteConfigFnEnsureInitialized));
}

Future<bool> RemoteConfigInternal::Activate() {
  const auto handle = future_impl_.SafeAlloc<bool>(kRemoteConfigFnActivate);
  bool activeResult = ActivateFetched();
  future_impl_.CompleteWithResult(handle, kFutureStatusSuccess,
                                  kFutureNoErrorMessage, activeResult);
  return MakeFuture<bool>(&future_impl_, handle);
}

Future<bool> RemoteConfigInternal::ActivateLastResult() {
  return static_cast<const Future<bool>&>(
      future_impl_.LastResult(kRemoteConfigFnActivate));
}

Future<bool> RemoteConfigInternal::FetchAndActivate() {
  const auto future_handle =
      future_impl_.SafeAlloc<bool>(kRemoteConfigFnFetchAndActivate);

  cache_expiration_in_seconds_ =
      config_settings_.minimum_fetch_interval_in_milliseconds /
      ::firebase::internal::kMillisecondsPerSecond;

  uint64_t milliseconds_since_epoch =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::system_clock::now().time_since_epoch())
          .count();

  uint64_t cache_expiration_timestamp =
      configs_.fetched.timestamp() +
      ::firebase::internal::kMillisecondsPerSecond *
          cache_expiration_in_seconds_;

  // Need to fetch in two cases:
  // - we are not fetching at this moment
  // - cache_expiration_in_seconds is zero
  // or
  // - we are not fetching at this moment
  // - cache (fetched) data is older than cache_expiration_in_seconds
  if (!is_fetch_process_have_task_ &&
      ((cache_expiration_in_seconds_ == 0) ||
       (cache_expiration_timestamp < milliseconds_since_epoch))) {
    auto data_handle =
        MakeShared<RCDataHandle<bool>>(&future_impl_, future_handle, this);

    auto callback = NewCallback(
        [](ThisRef ref, SharedPtr<RCDataHandle<bool>> handle) {
          ThisRefLock lock(&ref);
          if (lock.GetReference() != nullptr) {
            MutexLock lock(handle->rc_internal->internal_mutex_);

            handle->rc_internal->FetchInternal();

            FutureStatus futureResult =
                (handle->rc_internal->GetInfo().last_fetch_status ==
                 kLastFetchStatusSuccess)
                    ? kFutureStatusSuccess
                    : kFutureStatusFailure;

            bool activated = handle->rc_internal->ActivateFetched();
            handle->future_api->CompleteWithResult(
                handle->future_handle, futureResult, kFutureNoErrorMessage,
                activated);
          }
        },
        safe_this_, data_handle);

    scheduler_.Schedule(callback);
    is_fetch_process_have_task_ = true;
  } else {
    // Do not fetch, complete future immediately.
    future_impl_.CompleteWithResult(future_handle, kFutureStatusSuccess,
                                    kFutureNoErrorMessage, false);
  }
  return MakeFuture<bool>(&future_impl_, future_handle);
}

Future<bool> RemoteConfigInternal::FetchAndActivateLastResult() {
  return static_cast<const Future<bool>&>(
      future_impl_.LastResult(kRemoteConfigFnFetchAndActivate));
}

Future<void> RemoteConfigInternal::SetDefaultsLastResult() {
  return static_cast<const Future<void>&>(
      future_impl_.LastResult(kRemoteConfigFnSetDefaults));
}

Future<void> RemoteConfigInternal::SetConfigSettings(ConfigSettings settings) {
  const auto handle =
      future_impl_.SafeAlloc<void>(kRemoteConfigFnSetConfigSettings);
  config_settings_ = settings;
  future_impl_.Complete(handle, kFutureStatusSuccess);
  return MakeFuture<void>(&future_impl_, handle);
}

Future<void> RemoteConfigInternal::SetConfigSettingsLastResult() {
  return static_cast<const Future<void>&>(
      future_impl_.LastResult(kRemoteConfigFnSetConfigSettings));
}

ConfigSettings RemoteConfigInternal::GetConfigSettings() {
  return config_settings_;
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
  if (defaults == nullptr) {
    future_impl_.Complete(handle, kFutureStatusSuccess);
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
  future_impl_.Complete(handle, kFutureStatusSuccess);
  return MakeFuture<void>(&future_impl_, handle);
}
#endif  // SWIG

Future<void> RemoteConfigInternal::SetDefaults(const ConfigKeyValue* defaults,
                                               size_t number_of_defaults) {
  const auto handle = future_impl_.SafeAlloc<void>(kRemoteConfigFnSetDefaults);
  if (defaults == nullptr) {
    future_impl_.Complete(handle, kFutureStatusSuccess);
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

  future_impl_.Complete(handle, kFutureStatusSuccess);

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

bool RemoteConfigInternal::ConvertToBool(const std::string& from, bool* out) {
  bool conversion_successful = false;
  if (IsBoolTrue(from)) {
    conversion_successful = true;
    *out = true;
  }
  if (IsBoolFalse(from)) {
    conversion_successful = true;
    *out = false;
  }
  return conversion_successful;
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

  bool bool_value;
  bool conversion_successful = ConvertToBool(value, &bool_value);

  if (info) info->conversion_successful = conversion_successful;
  return bool_value;
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

bool RemoteConfigInternal::ConvertToLong(const std::string& from,
                                         int64_t* out) {
  int64_t long_value = kDefaultValueForLong;
  bool conversion_successful = IsLong(from);

  std::stringstream converter;
  converter << from;
  converter >> long_value;
  conversion_successful = conversion_successful && !converter.fail();

  *out = conversion_successful ? long_value : kDefaultValueForLong;
  return conversion_successful;
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
  bool conversion_successful = ConvertToLong(value, &long_value);
  if (info) info->conversion_successful = conversion_successful;
  return long_value;
}

bool RemoteConfigInternal::ConvertToDouble(const std::string& from,
                                           double* out) {
  double double_value = kDefaultValueForDouble;
  bool conversion_successful = IsDouble(from);

  std::stringstream converter;
  converter << from;
  converter >> double_value;
  conversion_successful = conversion_successful && !converter.fail();

  *out = conversion_successful ? double_value : kDefaultValueForDouble;
  return conversion_successful;
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
  bool conversion_successful = ConvertToDouble(value, &double_value);

  if (info) info->conversion_successful = conversion_successful;
  return double_value;
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

// String -> Variant
Variant RemoteConfigInternal::StringToVariant(const std::string& from) {
  // Try int
  int64_t long_value;
  bool conversion_successful = ConvertToLong(from, &long_value);
  if (conversion_successful) {
    return Variant(long_value);
  }
  // Not int, try double
  double double_value;
  conversion_successful = ConvertToDouble(from, &double_value);
  if (conversion_successful) {
    return Variant(double_value);
  }
  // Not double, try bool
  bool bool_value;
  conversion_successful = ConvertToBool(from, &bool_value);
  if (conversion_successful) {
    return Variant(bool_value);
  }
  return Variant::FromMutableString(from);
}

std::map<std::string, Variant> RemoteConfigInternal::GetAll() {
  std::map<std::string, Variant> result;
  for (const std::string& key : GetKeys()) {
    std::string value =
        GetString(key.c_str(), static_cast<ValueInfo*>(nullptr));

    result[key] = StringToVariant(value);
  }
  return result;
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

ConfigUpdateListenerRegistration
RemoteConfigInternal::AddOnConfigUpdateListener(
    std::function<void(ConfigUpdate&&, RemoteConfigError)>
        config_update_listener) {
  // Realtime RC is not yet implemented on desktop, so just return a
  // registration object that is no-op.
  return ConfigUpdateListenerRegistration();
}

void RemoteConfigInternal::FetchInternal() {
  // Fetch fresh config from server.
  rest_.Fetch(app_, config_settings_.fetch_timeout_in_milliseconds);
  // Need to copy everything to `configs_.fetched`.
  configs_.fetched = rest_.fetched();

  // Need to copy only info and digests to `configs_.metadata`.
  const RemoteConfigMetadata& metadata = rest_.metadata();
  configs_.metadata.set_info(metadata.info());
  configs_.metadata.set_digest_by_namespace(metadata.digest_by_namespace());

  is_fetch_process_have_task_ = false;
}

Future<void> RemoteConfigInternal::Fetch(uint64_t cache_expiration_in_seconds) {
  MutexLock lock(internal_mutex_);
  const auto future_handle = future_impl_.SafeAlloc<void>(kRemoteConfigFnFetch);

  cache_expiration_in_seconds_ = cache_expiration_in_seconds;

  uint64_t milliseconds_since_epoch =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::system_clock::now().time_since_epoch())
          .count();

  uint64_t cache_expiration_timestamp =
      configs_.fetched.timestamp() +
      ::firebase::internal::kMillisecondsPerSecond *
          cache_expiration_in_seconds_;

  // Need to fetch in two cases:
  // - we are not fetching at this moment
  // - cache_expiration_in_seconds is zero
  // or
  // - we are not fetching at this moment
  // - cache (fetched) data is older than cache_expiration_in_seconds
  if (!is_fetch_process_have_task_ &&
      ((cache_expiration_in_seconds_ == 0) ||
       (cache_expiration_timestamp < milliseconds_since_epoch))) {
    auto data_handle =
        MakeShared<RCDataHandle<void>>(&future_impl_, future_handle, this);

    auto callback = NewCallback(
        [](ThisRef ref, SharedPtr<RCDataHandle<void>> handle) {
          ThisRefLock lock(&ref);
          if (lock.GetReference() != nullptr) {
            MutexLock lock(handle->rc_internal->internal_mutex_);

            handle->rc_internal->FetchInternal();

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
