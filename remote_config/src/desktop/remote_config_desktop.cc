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

#include <chrono>  // NOLINT
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <set>
#include <string>
#include <thread>  // NOLINT
#include <utility>
#include <vector>

#include "app/src/callback.h"
#include "app/src/include/firebase/internal/platform.h"
#include "app/src/log.h"
#include "app/src/time.h"
#include "remote_config/src/common.h"
#include "remote_config/src/include/firebase/remote_config.h"
#include "remote_config/src/value_parser.h"

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
  RCDataHandle(const firebase::internal::SafeReference<RemoteConfigInternal>&
                   _rc_ref,
               ReferenceCountedFutureImpl* _future_api,
               const SafeFutureHandle<T>& _future_handle,
               uint64_t _cache_expiration)
      : rc_ref(_rc_ref),
        future_api(_future_api),
        future_handle(_future_handle),
        cache_expiration_in_seconds(_cache_expiration),
        completed(false) {}

  ~RCDataHandle() {
    if (!completed) {
      firebase::internal::SafeReferenceLock<RemoteConfigInternal> lock(&rc_ref);
      RemoteConfigInternal* self = lock.GetReference();
      if (self != nullptr && future_api != nullptr) {
        future_api->Complete(
            future_handle, kFutureStatusFailure,
            "RemoteConfig instance destroyed before operation completed.");
      }
    }
  }

  firebase::internal::SafeReference<RemoteConfigInternal> rc_ref;
  ReferenceCountedFutureImpl* future_api;
  SafeFutureHandle<T> future_handle;
  uint64_t cache_expiration_in_seconds;
  bool completed;
};

RemoteConfigInternal::RemoteConfigInternal(
    const firebase::App& app, const RemoteConfigFileManager& file_manager)
    : app_(app),
      file_manager_(file_manager),
      cache_expiration_in_seconds_(kDefaultCacheExpiration /
                                   ::firebase::internal::kMillisecondsPerSecond),
      is_fetch_process_have_task_(false),
      future_impl_(kRemoteConfigFnCount),
      safe_this_(this),
      rest_(app.options(), configs_, kDefaultNamespace),
      initialized_(false) {
  InternalInit();
}

RemoteConfigInternal::RemoteConfigInternal(const firebase::App& app)
    : app_(app),
      file_manager_(kFilePathSuffix, app),
      cache_expiration_in_seconds_(kDefaultCacheExpiration /
                                   ::firebase::internal::kMillisecondsPerSecond),
      is_fetch_process_have_task_(false),
      future_impl_(kRemoteConfigFnCount),
      safe_this_(this),
      rest_(app.options(), configs_, kDefaultNamespace),
      initialized_(false) {
  InternalInit();
}

RemoteConfigInternal::~RemoteConfigInternal() {
  scheduler_.CancelAllAndShutdownWorkerThread();
  safe_this_.ClearReference();

  save_channel_.Close();
  if (save_thread_.joinable()) {
    save_thread_.join();
  }
}

void RemoteConfigInternal::InternalInit() {
  MutexLock lock(internal_mutex_);
  if (!file_manager_.Load(&configs_)) {
    configs_ = LayeredConfigs();
  }
  AsyncSaveToFile();
  initialized_ = true;
}

bool RemoteConfigInternal::Initialized() const {
  MutexLock lock(internal_mutex_);
  return initialized_;
}

void RemoteConfigInternal::Cleanup() {
  // Do nothing.
}

Future<ConfigInfo> RemoteConfigInternal::EnsureInitialized() {
  MutexLock lock(internal_mutex_);
  const auto handle =
      future_impl_.SafeAlloc<ConfigInfo>(kRemoteConfigFnEnsureInitialized);
  future_impl_.CompleteWithResult(handle, kFutureStatusSuccess, "",
                                  configs_.metadata.info());
  return MakeFuture<ConfigInfo>(&future_impl_, handle);
}

Future<ConfigInfo> RemoteConfigInternal::EnsureInitializedLastResult() {
  return static_cast<const Future<ConfigInfo>&>(
      future_impl_.LastResult(kRemoteConfigFnEnsureInitialized));
}

Future<bool> RemoteConfigInternal::Activate() {
  const auto handle = future_impl_.SafeAlloc<bool>(kRemoteConfigFnActivate);
  bool active_result = ActivateFetched();
  future_impl_.CompleteWithResult(handle, kFutureStatusSuccess, "",
                                  active_result);
  return MakeFuture<bool>(&future_impl_, handle);
}

Future<bool> RemoteConfigInternal::ActivateLastResult() {
  return static_cast<const Future<bool>&>(
      future_impl_.LastResult(kRemoteConfigFnActivate));
}

bool RemoteConfigInternal::IsCacheExpiredLocked(
    uint64_t cache_expiration_in_seconds) const {
  if (cache_expiration_in_seconds == 0 ||
      configs_.fetched.timestamp() == 0) {
    return true;
  }
  uint64_t now_ms =
      static_cast<uint64_t>(
          std::chrono::duration_cast<std::chrono::milliseconds>(
              std::chrono::system_clock::now().time_since_epoch())
              .count());
  uint64_t fetched_ts = configs_.fetched.timestamp();
  if (now_ms <= fetched_ts) {
    return false;
  }
  constexpr uint64_t kMsPerSec = ::firebase::internal::kMillisecondsPerSecond;
  if (cache_expiration_in_seconds >
      std::numeric_limits<uint64_t>::max() / kMsPerSec) {
    return false;
  }
  uint64_t expiration_ms = cache_expiration_in_seconds * kMsPerSec;
  return (now_ms - fetched_ts) > expiration_ms;
}

Future<bool> RemoteConfigInternal::FetchAndActivate() {
  MutexLock lock(internal_mutex_);
  const auto future_handle =
      future_impl_.SafeAlloc<bool>(kRemoteConfigFnFetchAndActivate);

  uint64_t min_interval_ms =
      config_settings_.minimum_fetch_interval_in_milliseconds;
  // Round up sub-second non-zero intervals to 1 second so non-zero intervals
  // are not truncated to 0.
  cache_expiration_in_seconds_ =
      (min_interval_ms > 0 &&
       min_interval_ms < ::firebase::internal::kMillisecondsPerSecond)
          ? 1
          : (min_interval_ms / ::firebase::internal::kMillisecondsPerSecond);

  if (is_fetch_process_have_task_ ||
      IsCacheExpiredLocked(cache_expiration_in_seconds_)) {
    auto data_handle = std::make_shared<RCDataHandle<bool>>(
        safe_this_, &future_impl_, future_handle, cache_expiration_in_seconds_);

    is_fetch_process_have_task_ = true;
    auto callback = NewCallback(
        [](ThisRef ref, std::shared_ptr<RCDataHandle<bool>> handle) {
          ThisRefLock ref_lock(&ref);
          RemoteConfigInternal* self = ref_lock.GetReference();
          if (self == nullptr) {
            return;
          }
          bool need_fetch = true;
          {
            MutexLock lock(self->internal_mutex_);
            need_fetch =
                self->IsCacheExpiredLocked(handle->cache_expiration_in_seconds);
            if (!need_fetch) {
              self->is_fetch_process_have_task_ = false;
            }
          }
          if (need_fetch) {
            self->FetchInternal();
          }

          LastFetchStatus fetch_status = self->GetInfo().last_fetch_status;
          handle->completed = true;
          if (fetch_status == kLastFetchStatusSuccess) {
            bool activated = self->ActivateFetched();
            handle->future_api->CompleteWithResult(
                handle->future_handle, kFutureStatusSuccess, "", activated);
          } else {
            handle->future_api->CompleteWithResult(
                handle->future_handle, kFutureStatusFailure, "Fetch failed",
                false);
          }
        },
        safe_this_, data_handle);

    scheduler_.Schedule(callback);
  } else {
    bool activated = ActivateFetched();
    future_impl_.CompleteWithResult(future_handle, kFutureStatusSuccess, "",
                                    activated);
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
  MutexLock lock(internal_mutex_);
  const auto handle =
      future_impl_.SafeAlloc<void>(kRemoteConfigFnSetConfigSettings);
  config_settings_ = settings;
  future_impl_.Complete(handle, kFutureStatusSuccess, "");
  return MakeFuture<void>(&future_impl_, handle);
}

Future<void> RemoteConfigInternal::SetConfigSettingsLastResult() {
  return static_cast<const Future<void>&>(
      future_impl_.LastResult(kRemoteConfigFnSetConfigSettings));
}

ConfigSettings RemoteConfigInternal::GetConfigSettings() {
  MutexLock lock(internal_mutex_);
  return config_settings_;
}

// Validation limits for custom signals, aligned with backend and iOS/Android
// SDKs.
static constexpr size_t kMaxCustomSignalsKeyLength = kMaxCustomSignalKeyLength;
static constexpr size_t kMaxCustomSignalsStringValueLength =
    kMaxCustomSignalValueLength;

Future<void> RemoteConfigInternal::SetCustomSignals(
    const std::map<std::string, Variant>& custom_signals) {
  const auto handle =
      future_impl_.SafeAlloc<void>(kRemoteConfigFnSetCustomSignals);

  // Validate keys, values, and types before modifying stored state.
  for (const auto& kv : custom_signals) {
    const std::string& key = kv.first;
    const Variant& value = kv.second;

    // Custom signals support String, Int64, Double, or Null (used to clear a
    // key).
    if (!value.is_null() && !value.is_string() && !value.is_int64() &&
        !value.is_double()) {
      future_impl_.Complete(
          handle, kFutureStatusFailure,
          "Invalid value type. Must be String, Int, Double, or Null.");
      return MakeFuture<void>(&future_impl_, handle);
    }

    if (key.length() > kMaxCustomSignalsKeyLength) {
      future_impl_.Complete(
          handle, kFutureStatusFailure,
          ("Custom signal keys must be " +
           std::to_string(kMaxCustomSignalsKeyLength) + " characters or less.")
              .c_str());
      return MakeFuture<void>(&future_impl_, handle);
    }

    if (value.is_string()) {
      size_t str_len = 0;
      if (value.is_mutable_string()) {
        str_len = value.mutable_string().size();
      } else if (value.string_value() != nullptr) {
        str_len = std::strlen(value.string_value());
      }
      if (str_len > kMaxCustomSignalsStringValueLength) {
        future_impl_.Complete(
            handle, kFutureStatusFailure,
            ("Custom signal string values must be " +
             std::to_string(kMaxCustomSignalsStringValueLength) +
             " characters or less.")
                .c_str());
        return MakeFuture<void>(&future_impl_, handle);
      }
    }
  }

  {
    MutexLock lock(internal_mutex_);
    MetaCustomSignalsMap updated_signals = configs_.metadata.custom_signals();

    // Merge incoming signals with existing metadata:
    // - Null variants remove the existing signal entry.
    // - Non-null variants (String, Int64, Double) are stored with their
    // original type preserved.
    for (const auto& kv : custom_signals) {
      const std::string& key = kv.first;
      const Variant& value = kv.second;

      if (value.is_null()) {
        updated_signals.erase(key);
      } else {
        updated_signals[key] = value;
      }
    }

    // Verify the merged map size does not exceed the maximum allowed count.
    if (updated_signals.size() > kMaxCustomSignalsCount) {
      future_impl_.Complete(handle, kFutureStatusFailure,
                            ("Custom signals count exceeds the limit of " +
                             std::to_string(kMaxCustomSignalsCount) + ".")
                                .c_str());
      return MakeFuture<void>(&future_impl_, handle);
    }

    // If signals were modified, update metadata and notify the background
    // thread to persist.
    if (updated_signals != configs_.metadata.custom_signals()) {
      configs_.metadata.set_custom_signals(updated_signals);
      save_channel_.Put();
    }
  }

  future_impl_.Complete(handle, kFutureStatusSuccess, "");
  return MakeFuture<void>(&future_impl_, handle);
}

Future<void> RemoteConfigInternal::SetCustomSignalsLastResult() {
  return static_cast<const Future<void>&>(
      future_impl_.LastResult(kRemoteConfigFnSetCustomSignals));
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
    if (!blob_data || blob_size == 0) {
      return std::string();
    }
    return std::string(reinterpret_cast<const char*>(blob_data), blob_size);
  }
  if (variant.is_mutable_string()) {
    return variant.mutable_string();
  }
  if (variant.is_static_string()) {
    const char* s = variant.string_value();
    if (!s) {
      *failure = true;
      return std::string();
    }
    return std::string(s);
  }
  if (variant.is_bool()) {
    return variant.bool_value() ? "true" : "false";
  }
  if (variant.is_int64()) {
    return std::to_string(variant.int64_value());
  }
  if (variant.is_double()) {
    return ValueParser::FormatGeneralDouble(variant.double_value());
  }
  *failure = true;
  return std::string();
}

#ifndef SWIG
Future<void> RemoteConfigInternal::SetDefaults(
    const ConfigKeyValueVariant* defaults, size_t number_of_defaults) {
  const auto handle = future_impl_.SafeAlloc<void>(kRemoteConfigFnSetDefaults);
  if (defaults == nullptr && number_of_defaults > 0) {
    LogError("SetDefaults: defaults array is null with non-zero count.");
    future_impl_.Complete(handle, kFutureStatusFailure,
                          "defaults array must not be null.");
    return MakeFuture<void>(&future_impl_, handle);
  }
  std::map<std::string, std::string> defaults_map;
  for (size_t i = 0; i < number_of_defaults; i++) {
    const char* key = defaults[i].key;
    bool failure = false;
    std::string value = VariantToString(defaults[i].value, &failure);
    if (key && !failure) {
      defaults_map[key] = std::move(value);
    } else {
      LogWarning("SetDefaults: ignoring invalid default entry at index %zu.",
                 i);
    }
  }
  SetDefaults(defaults_map);
  future_impl_.Complete(handle, kFutureStatusSuccess, "");
  return MakeFuture<void>(&future_impl_, handle);
}
#endif  // SWIG

Future<void> RemoteConfigInternal::SetDefaults(const ConfigKeyValue* defaults,
                                               size_t number_of_defaults) {
  const auto handle = future_impl_.SafeAlloc<void>(kRemoteConfigFnSetDefaults);
  if (defaults == nullptr && number_of_defaults > 0) {
    LogError("SetDefaults: defaults array is null with non-zero count.");
    future_impl_.Complete(handle, kFutureStatusFailure,
                          "defaults array must not be null.");
    return MakeFuture<void>(&future_impl_, handle);
  }

  std::map<std::string, std::string> defaults_map;
  for (size_t i = 0; i < number_of_defaults; i++) {
    const char* key = defaults[i].key;
    const char* value = defaults[i].value;
    if (key && value) {
      defaults_map[key] = value;
    } else {
      LogWarning("SetDefaults: ignoring null key or value at index %zu.", i);
    }
  }
  SetDefaults(defaults_map);

  future_impl_.Complete(handle, kFutureStatusSuccess, "");

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
  if (!key) {
    return false;
  }
  MutexLock lock(internal_mutex_);
  return CheckValueInConfigLocked(configs_.active, kValueSourceRemoteValue, key,
                                  info, value) ||
         CheckValueInConfigLocked(configs_.defaults, kValueSourceDefaultValue,
                                  key, info, value);
}

bool RemoteConfigInternal::CheckValueInConfigLocked(
    const NamespacedConfigData& config, ValueSource source, const char* key,
    ValueInfo* info, std::string* value) {
  if (!key) return false;
  if (!config.HasValue(key, kDefaultNamespace)) {
    return false;
  }
  *value = config.GetValue(key, kDefaultNamespace);
  if (info) info->source = source;
  return true;
}

bool RemoteConfigInternal::IsBoolTrue(const std::string& str) {
  return ValueParser::IsBoolTrue(str);
}

bool RemoteConfigInternal::IsBoolFalse(const std::string& str) {
  return ValueParser::IsBoolFalse(str);
}

bool RemoteConfigInternal::IsLong(const std::string& str) {
  return ValueParser::IsLong(str);
}

bool RemoteConfigInternal::IsDouble(const std::string& str) {
  return ValueParser::IsDouble(str);
}

bool RemoteConfigInternal::ConvertToBool(const std::string& from, bool* out) {
  return ValueParser::ParseBoolean(from, out);
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

  bool bool_value = kDefaultValueForBool;
  bool conversion_successful = ConvertToBool(value, &bool_value);

  if (info) info->conversion_successful = conversion_successful;
  return conversion_successful ? bool_value : kDefaultValueForBool;
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
  bool conversion_successful = ValueParser::ParseInt64(from, &long_value);
  if (out) {
    *out = conversion_successful ? long_value : kDefaultValueForLong;
  }
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
  bool conversion_successful = ValueParser::ParseDouble(from, &double_value);
  if (out) {
    *out = conversion_successful ? double_value : kDefaultValueForDouble;
  }
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

  if (info) info->conversion_successful = true;
  return std::vector<unsigned char>(
      reinterpret_cast<const unsigned char*>(value.data()),
      reinterpret_cast<const unsigned char*>(value.data()) + value.size());
}

std::vector<std::string> RemoteConfigInternal::GetKeys() {
  return GetKeysByPrefix("");
}

std::vector<std::string> RemoteConfigInternal::GetKeysByPrefix(
    const char* prefix) {
  const char* safe_prefix = prefix ? prefix : "";
  std::set<std::string> unique_keys;
  {
    MutexLock lock(internal_mutex_);
    configs_.active.GetKeysByPrefix(safe_prefix, kDefaultNamespace,
                                    &unique_keys);
    configs_.defaults.GetKeysByPrefix(safe_prefix, kDefaultNamespace,
                                      &unique_keys);
  }
  return std::vector<std::string>(unique_keys.begin(), unique_keys.end());
}

// String -> Variant
Variant RemoteConfigInternal::StringToVariant(const std::string& from) {
  return ValueParser::CoerceToVariant(from);
}

std::map<std::string, Variant> RemoteConfigInternal::GetAll() {
  std::map<std::string, Variant> result;
  MutexLock lock(internal_mutex_);
  std::set<std::string> unique_keys;
  configs_.active.GetKeysByPrefix("", kDefaultNamespace, &unique_keys);
  configs_.defaults.GetKeysByPrefix("", kDefaultNamespace, &unique_keys);
  for (const std::string& key : unique_keys) {
    std::string value;
    if (CheckValueInConfigLocked(configs_.active, kValueSourceRemoteValue,
                                 key.c_str(), nullptr, &value) ||
        CheckValueInConfigLocked(configs_.defaults, kValueSourceDefaultValue,
                                 key.c_str(), nullptr, &value)) {
      result[key] = StringToVariant(value);
    }
  }
  return result;
}

bool RemoteConfigInternal::ActivateFetched() {
  bool changed = false;
  {
    MutexLock lock(internal_mutex_);
    // Fetched config not found or older than active config.
    if (configs_.fetched.timestamp() == 0 ||
        configs_.fetched.timestamp() < configs_.active.timestamp()) {
      return false;
    }
    if (configs_.fetched.timestamp() == configs_.active.timestamp() &&
        configs_.fetched.config() == configs_.active.config()) {
      return false;
    }
    changed = (configs_.fetched.config() != configs_.active.config());
    configs_.active = configs_.fetched;
  }
  save_channel_.Put();
  return changed;
}

ConfigInfo RemoteConfigInternal::GetInfo() const {
  MutexLock lock(internal_mutex_);
  return configs_.metadata.info();
}

ConfigUpdateListenerRegistration
RemoteConfigInternal::AddOnConfigUpdateListener(
    std::function<void(ConfigUpdate&&, RemoteConfigError)>
        config_update_listener) {
  LogWarning("AddOnConfigUpdateListener is not supported on desktop.");
  if (config_update_listener) {
    config_update_listener(ConfigUpdate{}, kRemoteConfigErrorUnimplemented);
  }
  return ConfigUpdateListenerRegistration();
}

void RemoteConfigInternal::FetchInternal() {
  LayeredConfigs local_configs;
  uint64_t timeout_ms = kDefaultTimeoutInMilliseconds;
  {
    MutexLock lock(internal_mutex_);
    local_configs = configs_;
    timeout_ms = config_settings_.fetch_timeout_in_milliseconds;
  }

  RemoteConfigREST local_rest(app_.options(), local_configs, kDefaultNamespace);
  local_rest.Fetch(app_, timeout_ms);

  {
    MutexLock lock(internal_mutex_);
    configs_.fetched = local_rest.fetched();
    const RemoteConfigMetadata& metadata = local_rest.metadata();
    configs_.metadata.set_info(metadata.info());
    configs_.metadata.set_digest_by_namespace(metadata.digest_by_namespace());
    is_fetch_process_have_task_ = false;
  }
  save_channel_.Put();
}

Future<void> RemoteConfigInternal::Fetch(uint64_t cache_expiration_in_seconds) {
  MutexLock lock(internal_mutex_);
  const auto future_handle = future_impl_.SafeAlloc<void>(kRemoteConfigFnFetch);

  cache_expiration_in_seconds_ = cache_expiration_in_seconds;

  if (is_fetch_process_have_task_ ||
      IsCacheExpiredLocked(cache_expiration_in_seconds_)) {
    auto data_handle = std::make_shared<RCDataHandle<void>>(
        safe_this_, &future_impl_, future_handle, cache_expiration_in_seconds_);

    is_fetch_process_have_task_ = true;
    auto callback = NewCallback(
        [](ThisRef ref, std::shared_ptr<RCDataHandle<void>> handle) {
          ThisRefLock ref_lock(&ref);
          RemoteConfigInternal* self = ref_lock.GetReference();
          if (self == nullptr) {
            return;
          }
          bool need_fetch = true;
          {
            MutexLock lock(self->internal_mutex_);
            need_fetch =
                self->IsCacheExpiredLocked(handle->cache_expiration_in_seconds);
            if (!need_fetch) {
              self->is_fetch_process_have_task_ = false;
            }
          }
          if (need_fetch) {
            self->FetchInternal();
          }

          LastFetchStatus fetch_status = self->GetInfo().last_fetch_status;
          handle->completed = true;
          if (fetch_status == kLastFetchStatusSuccess) {
            handle->future_api->Complete(handle->future_handle,
                                         kFutureStatusSuccess, "");
          } else {
            handle->future_api->Complete(handle->future_handle,
                                         kFutureStatusFailure, "Fetch failed");
          }
        },
        safe_this_, data_handle);

    scheduler_.Schedule(callback);
  } else {
    // Do not fetch, complete future immediately.
    future_impl_.Complete(future_handle, kFutureStatusSuccess, "");
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
