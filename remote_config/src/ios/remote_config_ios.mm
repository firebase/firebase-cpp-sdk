// Copyright 2016 Google LLC
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

#include "remote_config/src/ios/remote_config_ios.h"

#include <map>
#include <set>
#include <string>

#include "app/src/include/firebase/version.h"
#include "app/src/assert.h"
#include "app/src/log.h"
#include "app/src/time.h"
#include "remote_config/src/common.h"
#include "remote_config/src/include/firebase/remote_config.h"
#include "remote_config/src/config_update_listener_registration_internal.h"

namespace firebase {
namespace remote_config {

DEFINE_FIREBASE_VERSION_STRING(FirebaseRemoteConfig);

// Global reference to the Firebase App.
static const ::firebase::App *g_app = nullptr;

// Global reference to the Remote Config instance.
static FIRRemoteConfig *g_remote_config_instance;

// Maps FIRRemoteConfigSource values to the ValueSource enumeration.
static const ValueSource kFirebaseRemoteConfigSourceToValueSourceMap[] = {
    kValueSourceRemoteValue,   // FIRRemoteConfigSourceRemote
    kValueSourceDefaultValue,  // FIRRemoteConfigSourceDefault
    kValueSourceStaticValue,   // FIRRemoteConfigSourceStatic
};
static_assert(FIRRemoteConfigSourceRemote == 0, "FIRRemoteConfigSourceRemote is not 0");
static_assert(FIRRemoteConfigSourceDefault == 1, "FIRRemoteConfigSourceDefault is not 1");
static_assert(FIRRemoteConfigSourceStatic == 2, "FIRRemoteConfigSourceStatic is not 2");

// Regular expressions used to determine if the config value is a "valid" bool.
// Written to match what is used internally by the Java implementation.
static NSString *true_pattern = @"^(1|true|t|yes|y|on)$";
static NSString *false_pattern = @"^(0|false|f|no|n|off|)$";

// If a fetch was throttled, this is set to the time when the throttling is
// finished, in milliseconds since epoch.
static NSNumber *g_throttled_end_time = @0;

// Saved default keys.
static std::vector<std::string> *g_default_keys = nullptr;

static id VariantToNSObject(const Variant &variant) {
  if (variant.is_int64()) {
    return [NSNumber numberWithLongLong:variant.int64_value()];
  } else if (variant.is_bool()) {
    return [NSNumber numberWithBool:variant.bool_value() ? YES : NO];
  } else if (variant.is_double()) {
    return [NSNumber numberWithDouble:variant.double_value()];
  } else if (variant.is_string()) {
    return @(variant.string_value());
  } else if (variant.is_blob()) {
    return [NSData dataWithBytes:variant.blob_data() length:variant.blob_size()];
  } else {
    return nil;
  }
}

// Shared helper function for retrieving the FIRRemoteConfigValue.
static FIRRemoteConfigValue *GetValue(const char *key, ValueInfo *info, FIRRemoteConfig *rc_ptr) {
  FIRRemoteConfigValue *value;
  value = [rc_ptr configValueForKey:@(key)];
  if (info) {
    int source_index = static_cast<int>(value.source);
    if (source_index >= 0 && source_index < sizeof(kFirebaseRemoteConfigSourceToValueSourceMap)) {
      info->source = kFirebaseRemoteConfigSourceToValueSourceMap[value.source];
      info->conversion_successful = true;
    } else {
      info->conversion_successful = false;
      LogWarning("Remote Config: Failed to find a valid source for the requested key %s", key);
    }
  }
  return value;
}

static FIRRemoteConfigValue *GetValue(const char *key, ValueInfo *info) {
  return GetValue(key, info, g_remote_config_instance);
}

static void CheckBoolConversion(FIRRemoteConfigValue *value, ValueInfo *info) {
  if (info && info->conversion_successful) {
    NSError *error = nullptr;
    NSString *pattern = value.boolValue ? true_pattern : false_pattern;
    NSRegularExpression *regex =
        [NSRegularExpression regularExpressionWithPattern:pattern
                                                  options:NSRegularExpressionCaseInsensitive
                                                    error:&error];
    int matches = [regex numberOfMatchesInString:value.stringValue
                                         options:0
                                           range:NSMakeRange(0, [value.stringValue length])];
    info->conversion_successful = (matches == 1);
  }
}

static void CheckLongConversion(FIRRemoteConfigValue *value, ValueInfo *info) {
  if (info && info->conversion_successful) {
    NSError *error = nullptr;
    NSRegularExpression *regex =
        [NSRegularExpression regularExpressionWithPattern:@"^[0-9]+$"
                                                  options:NSRegularExpressionCaseInsensitive
                                                    error:&error];
    int matches = [regex numberOfMatchesInString:value.stringValue
                                         options:0
                                           range:NSMakeRange(0, [value.stringValue length])];
    info->conversion_successful = (matches == 1);
  }
}

static void CheckDoubleConversion(FIRRemoteConfigValue *value, ValueInfo *info) {
  if (info && info->conversion_successful) {
    NSNumberFormatter *formatter = [[NSNumberFormatter alloc] init];
    [formatter setNumberStyle:NSNumberFormatterDecimalStyle];
    NSNumber *number = [formatter numberFromString:value.stringValue];
    if (!number) {
      info->conversion_successful = false;
    }
  }
}

std::vector<unsigned char> ConvertData(FIRRemoteConfigValue *value) {
  NSData *data = value.dataValue;
  int size = [data length] / sizeof(unsigned char);
  const unsigned char *bytes = static_cast<const unsigned char *>([data bytes]);
  return std::vector<unsigned char>(bytes, bytes + size);
}

static void GetInfoFromFIRRemoteConfig(FIRRemoteConfig *rc_instance, ConfigInfo *out_info,
                                       int64_t throttled_end_time) {
  FIREBASE_DEV_ASSERT(out_info != nullptr);
  out_info->fetch_time = round(rc_instance.lastFetchTime.timeIntervalSince1970 *
                               ::firebase::internal::kMillisecondsPerSecond);
  out_info->throttled_end_time = throttled_end_time * ::firebase::internal::kMillisecondsPerSecond;
  switch (rc_instance.lastFetchStatus) {
    case FIRRemoteConfigFetchStatusNoFetchYet:
      out_info->last_fetch_status = kLastFetchStatusPending;
      out_info->last_fetch_failure_reason = kFetchFailureReasonInvalid;
      break;
    case FIRRemoteConfigFetchStatusSuccess:
      out_info->last_fetch_status = kLastFetchStatusSuccess;
      out_info->last_fetch_failure_reason = kFetchFailureReasonInvalid;
      break;
    case FIRRemoteConfigFetchStatusFailure:
      out_info->last_fetch_status = kLastFetchStatusFailure;
      out_info->last_fetch_failure_reason = kFetchFailureReasonError;
      break;
    case FIRRemoteConfigFetchStatusThrottled:
      out_info->last_fetch_status = kLastFetchStatusFailure;
      out_info->last_fetch_failure_reason = kFetchFailureReasonThrottled;
      break;
    default:
      LogError("Remote Config: Received unknown last fetch status: %d",
               rc_instance.lastFetchStatus);
      out_info->last_fetch_status = kLastFetchStatusFailure;
      out_info->last_fetch_failure_reason = kFetchFailureReasonError;
      break;
  }
}

static RemoteConfigError ConvertFIRRemoteConfigUpdateError(NSError *error) {
  switch(error.code) {
    case FIRRemoteConfigUpdateErrorStreamError:
      return kRemoteConfigErrorConfigUpdateStreamError;
    case FIRRemoteConfigUpdateErrorNotFetched:
      return kRemoteConfigErrorConfigUpdateNotFetched;
    case FIRRemoteConfigUpdateErrorMessageInvalid:
      return kRemoteConfigErrorConfigUpdateMessageInvalid;
    case FIRRemoteConfigUpdateErrorUnavailable:
      return kRemoteConfigErrorConfigUpdateUnavailable;
    default:
      return kRemoteConfigErrorUnimplemented;
  }
}

static ConfigUpdate ConvertConfigUpdateKeys(NSSet<NSString *> *keys) {
  ConfigUpdate config_update;

  for (NSString *key in keys) {
    config_update.updated_keys.push_back(util::NSStringToString(key).c_str());
  }
  return config_update;
}

namespace internal {
RemoteConfigInternal::RemoteConfigInternal(const firebase::App &app)
    : app_(app), future_impl_(kRemoteConfigFnCount) {
  FIRApp *platform_app = app_.GetPlatformApp();
  impl_.reset(new FIRRemoteConfigPointer([FIRRemoteConfig remoteConfigWithApp:platform_app]));
}

RemoteConfigInternal::~RemoteConfigInternal() {
  // Destructor is necessary for ARC garbage collection.
  
  // Trigger CleanupNotifier Cleanup. This will delete
  // ConfigUpdateListenerRegistrationInternal instances and it will update
  // ConfigUpdateListenerRegistration instances to no longer point to the
  // corresponding internal objects.
  cleanup_notifier().CleanupAll();
}

bool RemoteConfigInternal::Initialized() const{
  return true;
}

void RemoteConfigInternal::Cleanup() {}

template <typename THandle, typename TResult>
static int64_t FutureCompleteWithError(NSError *error, ReferenceCountedFutureImpl *future_impl,
                                       THandle handle, TResult result) {
  int64_t throttled_end_time = 0;
  if (error.userInfo) {
    throttled_end_time =
        ((NSNumber *)error.userInfo[FIRRemoteConfigThrottledEndTimeInSecondsKey]).longLongValue;
  }
  // If we got an error code back, return that, with the associated string.
  future_impl->CompleteWithResult(handle, kFutureStatusFailure,
                                  util::NSStringToString(error.localizedDescription).c_str(),
                                  result);
  return throttled_end_time;
}

Future<ConfigInfo> RemoteConfigInternal::EnsureInitialized() {
  const auto handle = future_impl_.SafeAlloc<ConfigInfo>(kRemoteConfigFnEnsureInitialized);
  [impl() ensureInitializedWithCompletionHandler:^(NSError *error) {
    ConfigInfo config_info;
    if (error) {
      throttled_end_time_in_sec_ =
          FutureCompleteWithError<SafeFutureHandle<ConfigInfo>, ConfigInfo>(error, &future_impl_,
                                                                            handle, config_info);
    } else {
      GetInfoFromFIRRemoteConfig(impl(), &config_info, throttled_end_time_in_sec_);
      future_impl_.CompleteWithResult(handle, kFutureStatusSuccess, "", config_info);
    }
  }];
  return MakeFuture<ConfigInfo>(&future_impl_, handle);
}

Future<ConfigInfo> RemoteConfigInternal::EnsureInitializedLastResult() {
  return static_cast<const Future<ConfigInfo> &>(
      future_impl_.LastResult(kRemoteConfigFnEnsureInitialized));
}

Future<bool> RemoteConfigInternal::Activate() {
  const auto handle = future_impl_.SafeAlloc<bool>(kRemoteConfigFnActivate);
  [impl() activateWithCompletion:^(BOOL changed, NSError *error) {
    if (error) {
      throttled_end_time_in_sec_ = FutureCompleteWithError<SafeFutureHandle<bool>, bool>(
          error, &future_impl_, handle, false);
    } else {
      future_impl_.CompleteWithResult(handle, kFutureStatusSuccess, "", changed ? true : false);
    }
  }];
  return MakeFuture<bool>(&future_impl_, handle);
}

Future<bool> RemoteConfigInternal::ActivateLastResult() {
  return static_cast<const Future<bool> &>(future_impl_.LastResult(kRemoteConfigFnActivate));
}

Future<bool> RemoteConfigInternal::FetchAndActivate() {
  const auto handle = future_impl_.SafeAlloc<bool>(kRemoteConfigFnFetchAndActivate);
  [impl() fetchAndActivateWithCompletionHandler:^(FIRRemoteConfigFetchAndActivateStatus status,
                                                  NSError *error) {
    if (error) {
      throttled_end_time_in_sec_ = FutureCompleteWithError<SafeFutureHandle<bool>, bool>(
          error, &future_impl_, handle, false);

    } else if (status == FIRRemoteConfigFetchAndActivateStatusSuccessUsingPreFetchedData) {
      future_impl_.CompleteWithResult(
          handle, kFutureStatusSuccess,
          "Succeeded from already fetched but yet unexpired config data.", false);
    } else {
      // Everything worked!
      future_impl_.CompleteWithResult(handle, kFutureStatusSuccess, "", true);
    }
  }];
  return MakeFuture<bool>(&future_impl_, handle);
}

Future<bool> RemoteConfigInternal::FetchAndActivateLastResult() {
  return static_cast<const Future<bool> &>(
      future_impl_.LastResult(kRemoteConfigFnFetchAndActivate));
}

Future<void> RemoteConfigInternal::Fetch(uint64_t cache_expiration_in_seconds) {
  const auto handle = future_impl_.SafeAlloc<void>(kRemoteConfigFnFetch);
  [impl()
      fetchWithExpirationDuration:cache_expiration_in_seconds
                completionHandler:^(FIRRemoteConfigFetchStatus status, NSError *error) {
                  if (error) {
                    if (error.userInfo) {
                      throttled_end_time_in_sec_ =
                          ((NSNumber *)error.userInfo[FIRRemoteConfigThrottledEndTimeInSecondsKey])
                              .longLongValue;
                    }
                    // If we got an error code back, return that, with the associated string.
                    future_impl_.Complete(
                        handle, kFutureStatusFailure,
                        util::NSStringToString(error.localizedDescription).c_str());
                  } else if (status != FIRRemoteConfigFetchStatusSuccess) {
                    future_impl_.Complete(
                        handle, kFutureStatusFailure,
                        "Fetch encountered an error, either failed or throttled.");
                  } else {
                    // Everything worked!
                    future_impl_.Complete(handle, kFutureStatusSuccess);
                  }
                }];
  return MakeFuture<void>(&future_impl_, handle);
}

Future<void> RemoteConfigInternal::FetchLastResult() {
  return static_cast<const Future<void> &>(future_impl_.LastResult(kRemoteConfigFnFetch));
}

Future<void> RemoteConfigInternal::SetDefaults(const ConfigKeyValueVariant *defaults,
                                               size_t number_of_defaults) {
  const auto handle = future_impl_.SafeAlloc<void>(kRemoteConfigFnSetDefaults);

  NSMutableDictionary *dict = [[NSMutableDictionary alloc] init];
  default_keys_.clear();
  default_keys_.reserve(number_of_defaults);
  for (size_t i = 0; i < number_of_defaults; ++i) {
    const char *key = defaults[i].key;
    id value = VariantToNSObject(defaults[i].value);
    if (value) {
      dict[@(key)] = value;
      default_keys_.push_back(key);
    } else if (!defaults[i].value.is_null()) {
      LogError("Remote Config: Invalid Variant type for SetDefaults() key %s", key);
    }
  }
  dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
    [impl() setDefaults:dict];
    future_impl_.Complete(handle, kFutureStatusSuccess);
  });
  return MakeFuture<void>(&future_impl_, handle);
}

Future<void> RemoteConfigInternal::SetDefaults(const ConfigKeyValue *defaults,
                                               size_t number_of_defaults) {
  const auto handle = future_impl_.SafeAlloc<void>(kRemoteConfigFnSetDefaults);
  NSMutableDictionary *dict = [[NSMutableDictionary alloc] init];
  default_keys_.clear();
  default_keys_.reserve(number_of_defaults);
  for (size_t i = 0; i < number_of_defaults; ++i) {
    const char *key = defaults[i].key;
    dict[@(key)] = @(defaults[i].value);
    default_keys_.push_back(key);
  }
  dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
    [impl() setDefaults:dict];
    future_impl_.Complete(handle, kFutureStatusSuccess);
  });
  return MakeFuture<void>(&future_impl_, handle);
}

Future<void> RemoteConfigInternal::SetDefaultsLastResult() {
  return static_cast<const Future<void> &>(future_impl_.LastResult(kRemoteConfigFnSetDefaults));
}

Future<void> RemoteConfigInternal::SetConfigSettings(ConfigSettings settings) {
  const auto handle = future_impl_.SafeAlloc<void>(kRemoteConfigFnSetConfigSettings);
  FIRRemoteConfigSettings *newConfigSettings = [[FIRRemoteConfigSettings alloc] init];
  newConfigSettings.minimumFetchInterval =
      static_cast<NSTimeInterval>(settings.minimum_fetch_interval_in_milliseconds /
                                  ::firebase::internal::kMillisecondsPerSecond);
  newConfigSettings.fetchTimeout = static_cast<NSTimeInterval>(
      settings.fetch_timeout_in_milliseconds / ::firebase::internal::kMillisecondsPerSecond);
  dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
    [impl() setConfigSettings:newConfigSettings];
    future_impl_.Complete(handle, kFutureStatusSuccess);
  });
  return MakeFuture<void>(&future_impl_, handle);
}

Future<void> RemoteConfigInternal::SetConfigSettingsLastResult() {
  return static_cast<const Future<void> &>(
      future_impl_.LastResult(kRemoteConfigFnSetConfigSettings));
}

ConfigSettings RemoteConfigInternal::GetConfigSettings() const {
  ConfigSettings settings;
  FIRRemoteConfigSettings *config_settings = impl().configSettings;
  settings.minimum_fetch_interval_in_milliseconds =
      config_settings.minimumFetchInterval * ::firebase::internal::kMillisecondsPerSecond;
  settings.fetch_timeout_in_milliseconds =
      config_settings.fetchTimeout * ::firebase::internal::kMillisecondsPerSecond;
  return settings;
}

bool RemoteConfigInternal::GetBoolean(const char *key, ValueInfo *info) {
  FIRRemoteConfigValue *value = GetValue(key, info, impl());
  CheckBoolConversion(value, info);
  return static_cast<bool>(value.boolValue);
}

int64_t RemoteConfigInternal::GetLong(const char *key, ValueInfo *info) {
  FIRRemoteConfigValue *value = GetValue(key, info, impl());
  CheckLongConversion(value, info);
  return value.numberValue.longLongValue;
}

double RemoteConfigInternal::GetDouble(const char *key, ValueInfo *info) {
  FIRRemoteConfigValue *value = GetValue(key, info, impl());
  CheckDoubleConversion(value, info);
  return value.numberValue.doubleValue;
}

std::string RemoteConfigInternal::GetString(const char *key, ValueInfo *info) {
  return util::NSStringToString(GetValue(key, info, impl()).stringValue);
}

std::vector<unsigned char> RemoteConfigInternal::GetData(const char *key, ValueInfo *info) {
  return ConvertData(GetValue(key, info, impl()));
}

std::vector<std::string> RemoteConfigInternal::GetKeysByPrefix(const char *prefix) {
  std::vector<std::string> keys;
  std::set<std::string> key_set;
  NSSet<NSString *> *ios_keys;
  NSString *prefix_string = prefix ? @(prefix) : nil;
  ios_keys = [impl() keysWithPrefix:prefix_string];
  for (NSString *key in ios_keys) {
    keys.push_back(key.UTF8String);
    key_set.insert(key.UTF8String);
  }

  // Add any extra keys that were previously included in defaults but not returned by
  // keysWithPrefix.
  size_t prefix_length = prefix ? strlen(prefix) : 0;
  for (auto i = default_keys_.begin(); i != default_keys_.end(); ++i) {
    if (key_set.find(*i) != key_set.end()) {
      // Already in the list of keys, no need to add it.
      continue;
    }
    // If the prefix matches (or we have no prefix to compare), add it to the
    // defaults list.
    if (prefix_length == 0 || strncmp(prefix, i->c_str(), prefix_length) == 0) {
      keys.push_back(*i);
      key_set.insert(*i);  // In case the defaults vector has duplicate keys.
    }
  }

  return keys;
}

std::vector<std::string> RemoteConfigInternal::GetKeys() { return GetKeysByPrefix(nullptr); }

static Variant FIRValueToVariant(FIRRemoteConfigValue *in_value, ValueInfo *value_info) {
  // int
  value_info->conversion_successful = true;  // reset to get into check.
  CheckLongConversion(in_value, value_info);
  if (value_info->conversion_successful) {
    return Variant::FromInt64(in_value.numberValue.longLongValue);
  }
  // double
  value_info->conversion_successful = true;  // reset to get into check.
  CheckDoubleConversion(in_value, value_info);
  if (value_info->conversion_successful) {
    return Variant::FromDouble(in_value.numberValue.doubleValue);
  }
  // boolean
  value_info->conversion_successful = true;  // reset to get into check.
  CheckBoolConversion(in_value, value_info);
  if (value_info->conversion_successful) {
    return Variant::FromBool(static_cast<bool>(in_value.boolValue));
  }
  // string
  if (in_value.stringValue) {
    return Variant::FromMutableString(util::NSStringToString(in_value.stringValue));
  }
  // data
  NSData *data = in_value.dataValue;
  if (data) {
    int size = [data length] / sizeof(unsigned char);
    const unsigned char *bytes = static_cast<const unsigned char *>([data bytes]);
    return Variant::FromMutableBlob(bytes, size);
  }
  // If get here, all conversion fails.
  LogError("Remote Config: Unable to convert a FIRRemoteConfigValue to "
           "Variant.");
  return Variant::Null();
}

std::map<std::string, Variant> RemoteConfigInternal::GetAll() {
  std::map<std::string, Variant> value;
  std::vector<std::string> keys = GetKeys();

  for (std::string key : keys) {
    ValueInfo value_info;
    FIRRemoteConfigValue *f_value;
    f_value = [impl() configValueForKey:@(key.c_str())];
    Variant v_value = FIRValueToVariant(f_value, &value_info);
    value.insert(std::make_pair(key, v_value));
  }

  return value;
}

const ConfigInfo RemoteConfigInternal::GetInfo() const {
  ConfigInfo config_info;
  GetInfoFromFIRRemoteConfig(impl(), &config_info, throttled_end_time_in_sec_);
  return config_info;
}

ConfigUpdateListenerRegistration RemoteConfigInternal::AddOnConfigUpdateListener(
      std::function<void(ConfigUpdate&&, RemoteConfigError)>
        config_update_listener) {
    FIRConfigUpdateListenerRegistration *registration;
    registration = [impl() addOnConfigUpdateListener: ^(FIRRemoteConfigUpdate *_Nullable update,
                                  NSError *_Nullable error) {
          if (error) {
            config_update_listener({}, ConvertFIRRemoteConfigUpdateError(error));
            return;
          }

          config_update_listener(ConvertConfigUpdateKeys(update.updatedKeys), kRemoteConfigErrorNone);
    }];

    ConfigUpdateListenerRegistrationInternal* registration_internal =
        new ConfigUpdateListenerRegistrationInternal(this, [registration]() {
          [registration remove];
        });
  // Delete the internal registration when RemoteConfigInternal is cleaned up.
  cleanup_notifier().RegisterObject(registration_internal, [](void* registration) {
    delete reinterpret_cast<ConfigUpdateListenerRegistrationInternal*>(registration);
  });

  ConfigUpdateListenerRegistration registration_wrapper(registration_internal);
  return registration_wrapper;
}

}  // namespace internal
}  // namespace remote_config
}  // namespace firebase
