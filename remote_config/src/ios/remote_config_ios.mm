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

#include "app/src/assert.h"
#include "app/src/include/firebase/version.h"
#include "app/src/log.h"
#include "app/src/time.h"
#include "remote_config/src/common.h"

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

InitResult Initialize(const App &app) {
  if (g_app) {
    LogWarning("Remote Config API already initialized");
    return kInitResultSuccess;
  }
  internal::RegisterTerminateOnDefaultAppDestroy();
  LogInfo("Remote Config API Initializing");
  FIREBASE_ASSERT(!g_remote_config_instance);
  g_app = &app;

  // Create the Remote Config instance.
  g_remote_config_instance = [FIRRemoteConfig remoteConfig];

  FutureData::Create();
  g_default_keys = new std::vector<std::string>;

  LogInfo("Remote Config API Initialized");
  return kInitResultSuccess;
}

namespace internal {

bool IsInitialized() { return g_app != nullptr; }

}  // namespace internal


void Terminate() {
  if (g_app) {
    LogWarning("Remove Config API already shut down.");
    return;
  }
  internal::UnregisterTerminateOnDefaultAppDestroy();
  g_app = nullptr;
  g_remote_config_instance = nil;
  FutureData::Destroy();
  delete g_default_keys;
  g_default_keys = nullptr;
}

void SetDefaults(const ConfigKeyValue *defaults, size_t number_of_defaults) {
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  NSMutableDictionary *dict = [[NSMutableDictionary alloc] init];
  g_default_keys->clear();
  g_default_keys->reserve(number_of_defaults);
  for (size_t i = 0; i < number_of_defaults; ++i) {
    const char* key = defaults[i].key;
    dict[@(key)] = @(defaults[i].value);
    g_default_keys->push_back(key);
  }
  [g_remote_config_instance setDefaults:dict];
}

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

void SetDefaults(const ConfigKeyValueVariant *defaults, size_t number_of_defaults) {
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  NSMutableDictionary *dict = [[NSMutableDictionary alloc] init];
  g_default_keys->clear();
  g_default_keys->reserve(number_of_defaults);
  for (size_t i = 0; i < number_of_defaults; ++i) {
    const char* key = defaults[i].key;
    id value = VariantToNSObject(defaults[i].value);
    if (value) {
      dict[@(key)] = value;
      g_default_keys->push_back(key);
    } else {
      LogError("Remote Config: Invalid Variant type for SetDefaults() key %s", key);
    }
  }
  [g_remote_config_instance setDefaults:dict];
}

std::string GetConfigSetting(ConfigSetting setting) {
  FIREBASE_ASSERT_RETURN(std::string(), internal::IsInitialized());
  switch (setting) {
  case kConfigSettingDeveloperMode:
    // This setting is deprecated.
    LogWarning("Remote Config: Developer mode setting is deprecated.");
    return "1";
  default:
    LogError("Remote Config: GetConfigSetting called with unknown setting: %d", setting);
    return std::string();
  }
}

void SetConfigSetting(ConfigSetting setting, const char *value) {
  switch (setting) {
  case kConfigSettingDeveloperMode:
    LogWarning("Remote Config: Developer mode setting is deprecated.");
    break;
  default:
    LogError("Remote Config: SetConfigSetting called with unknown setting: %d", setting);
    break;
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

bool GetBoolean(const char *key) { return GetBoolean(key, nullptr); }
bool GetBoolean(const char *key, ValueInfo *info) {
  FIREBASE_ASSERT_RETURN(false, internal::IsInitialized());
  FIRRemoteConfigValue *value = GetValue(key, info);
  CheckBoolConversion(value, info);
  return static_cast<bool>(value.boolValue);
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

int64_t GetLong(const char *key) { return GetLong(key, nullptr); }
int64_t GetLong(const char *key, ValueInfo *info) {
  FIREBASE_ASSERT_RETURN(0, internal::IsInitialized());
  FIRRemoteConfigValue *value = GetValue(key, info);
  CheckLongConversion(value, info);
  return value.numberValue.longLongValue;
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

double GetDouble(const char *key) { return GetDouble(key, nullptr); }
double GetDouble(const char *key, ValueInfo *info) {
  FIREBASE_ASSERT_RETURN(0.0, internal::IsInitialized());
  FIRRemoteConfigValue *value = GetValue(key, info);
  CheckDoubleConversion(value, info);
  return value.numberValue.doubleValue;
}

std::string GetString(const char *key) { return GetString(key, nullptr); }
std::string GetString(const char *key, ValueInfo *info) {
  FIREBASE_ASSERT_RETURN(std::string(), internal::IsInitialized());
  return util::NSStringToString(GetValue(key, info).stringValue);
}

std::vector<unsigned char> ConvertData(FIRRemoteConfigValue *value) {
  NSData *data = value.dataValue;
  int size = [data length] / sizeof(unsigned char);
  const unsigned char *bytes = static_cast<const unsigned char *>([data bytes]);
  return std::vector<unsigned char>(bytes, bytes + size);
}

std::vector<unsigned char> GetData(const char *key) { return GetData(key, nullptr); }

std::vector<unsigned char> GetData(const char *key, ValueInfo *info) {
  FIREBASE_ASSERT_RETURN(std::vector<unsigned char>(), internal::IsInitialized());
  return ConvertData(GetValue(key, info));
}

std::vector<std::string> GetKeysByPrefix(const char *prefix) {
  FIREBASE_ASSERT_RETURN(std::vector<std::string>(), internal::IsInitialized());
  std::vector<std::string> keys;
  std::set<std::string> key_set;
  NSSet<NSString *> *ios_keys;
  NSString *prefix_string = prefix ? @(prefix) : nil;
  ios_keys = [g_remote_config_instance keysWithPrefix:prefix_string];
  for (NSString *key in ios_keys) {
    keys.push_back(key.UTF8String);
    key_set.insert(key.UTF8String);
  }

  // Add any extra keys that were previously included in defaults but not returned by
  // keysWithPrefix.
  size_t prefix_length = prefix ? strlen(prefix) : 0;
  for (auto i = g_default_keys->begin(); i != g_default_keys->end(); ++i) {
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

std::vector<std::string> GetKeys() { return GetKeysByPrefix(nullptr); }

Future<void> Fetch() { return Fetch(kDefaultCacheExpiration); }

Future<void> Fetch(uint64_t cache_expiration_in_seconds) {
  FIREBASE_ASSERT_RETURN(FetchLastResult(), internal::IsInitialized());
  ReferenceCountedFutureImpl *api = FutureData::Get()->api();
  const auto handle = api->SafeAlloc<void>(kRemoteConfigFnFetch);

  FIRRemoteConfigFetchCompletion completion = ^(FIRRemoteConfigFetchStatus status, NSError *error) {
    if (error) {
      LogError("Remote Config: Fetch encountered an error: %s",
               util::NSStringToString(error.localizedDescription).c_str());
      if (error.userInfo) {
        g_throttled_end_time =
            ((NSNumber *)error.userInfo[FIRRemoteConfigThrottledEndTimeInSecondsKey]);
      }
      // If we got an error code back, return that, with the associated string.
      api->Complete(handle, kFutureStatusFailure,
                    util::NSStringToString(error.localizedDescription).c_str());
    } else if (status != FIRRemoteConfigFetchStatusSuccess) {
      api->Complete(handle, kFutureStatusFailure, "Fetch encountered an error.");
    } else {
      // Everything worked!
      api->Complete(handle, kFutureStatusSuccess);
    }
  };
  [g_remote_config_instance fetchWithExpirationDuration:cache_expiration_in_seconds
                                      completionHandler:completion];

  return MakeFuture<void>(api, handle);
}

Future<void> FetchLastResult() {
  FIREBASE_ASSERT_RETURN(Future<void>(), internal::IsInitialized());
  ReferenceCountedFutureImpl *api = FutureData::Get()->api();
  return static_cast<const Future<void> &>(api->LastResult(kRemoteConfigFnFetch));
}

bool ActivateFetched() {
  FIREBASE_ASSERT_RETURN(false, internal::IsInitialized());
  __block bool succeeded = true;
  __block dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
  [g_remote_config_instance activateWithCompletion:^(BOOL changed, NSError *_Nullable error) {
      if (error) succeeded = false;
      dispatch_semaphore_signal(semaphore);
    }];
  dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);
  return succeeded;
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

const ConfigInfo &GetInfo() {
  static ConfigInfo kConfigInfo;
  FIREBASE_ASSERT_RETURN(kConfigInfo, internal::IsInitialized());
  GetInfoFromFIRRemoteConfig(g_remote_config_instance, &kConfigInfo,
                             g_throttled_end_time.longLongValue);
  return kConfigInfo;
}

namespace internal {
RemoteConfigInternal::RemoteConfigInternal(const firebase::App &app)
    : app_(app), future_impl_(kRemoteConfigFnCount) {
  FIRApp *platform_app = app_.GetPlatformApp();
  impl_.reset(new FIRRemoteConfigPointer([FIRRemoteConfig remoteConfigWithApp:platform_app]));
}

RemoteConfigInternal::~RemoteConfigInternal() {
  // Destructor is necessary for ARC garbage collection.
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

#ifdef FIREBASE_EARLY_ACCESS_PREVIEW
Future<void> RemoteConfigInternal::SetConfigSettings(ConfigSettings settings) {
  const auto handle = future_impl_.SafeAlloc<void>(kRemoteConfigFnSetConfigSettings);
  FIRRemoteConfigSettings *config_settings = impl().configSettings;
  config_settings.minimumFetchInterval =
      static_cast<NSTimeInterval>(settings.minimum_fetch_interval_in_milliseconds /
                                  ::firebase::internal::kMillisecondsPerSecond);
  config_settings.fetchTimeout = static_cast<NSTimeInterval>(
      settings.fetch_timeout_in_milliseconds / ::firebase::internal::kMillisecondsPerSecond);
  return MakeFuture<void>(&future_impl_, handle);
}
#endif  // FIREBASE_EARLY_ACCESS_PREVIEW
Future<void> RemoteConfigInternal::SetConfigSettingsLastResult() {
  return static_cast<const Future<void> &>(
      future_impl_.LastResult(kRemoteConfigFnSetConfigSettings));
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
}  // namespace internal
}  // namespace remote_config
}  // namespace firebase
