#include "remote_config/src/ios/remote_config_ios.h"
#include <dispatch/dispatch.h>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include "app/src/assert.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/include/firebase/version.h"
#include "app/src/log.h"
#include "app/src/reference_counted_future_impl.h"
#include "app/src/time.h"
#include "remote_config/src/common.h"
#include "remote_config/src/config_update_listener_registration_internal.h"
#include "remote_config/src/include/firebase/remote_config.h"
#include "remote_config/src/value_parser.h"
namespace firebase::remote_config {
DEFINE_FIREBASE_VERSION_STRING(FirebaseRemoteConfig);
static const ValueSource kFirebaseRemoteConfigSourceToValueSourceMap[] = {
    kValueSourceRemoteValue, kValueSourceDefaultValue, kValueSourceStaticValue};
static_assert(FIRRemoteConfigSourceRemote == 0, "FIRRemoteConfigSourceRemote is not 0");
static_assert(FIRRemoteConfigSourceDefault == 1, "FIRRemoteConfigSourceDefault is not 1");
static_assert(FIRRemoteConfigSourceStatic == 2, "FIRRemoteConfigSourceStatic is not 2");
static constexpr size_t kCustomSignalsMaxCount = 100;
static constexpr size_t kCustomSignalsMaxKeyLength = 256;
static constexpr size_t kCustomSignalsMaxStringValueLength = 500;
static id VariantToNSObject(const Variant &variant) {
  if (variant.is_int64()) return [NSNumber numberWithLongLong:variant.int64_value()];
  if (variant.is_bool()) return [NSNumber numberWithBool:variant.bool_value() ? YES : NO];
  if (variant.is_double()) return [NSNumber numberWithDouble:variant.double_value()];
  if (variant.is_mutable_string()) {
    const std::string &str = variant.mutable_string();
    return [[NSString alloc] initWithBytes:str.data()
                                    length:str.size()
                                  encoding:NSUTF8StringEncoding];
  }
  if (variant.is_static_string()) {
    const char *str = variant.string_value();
    return str ? @(str) : @"";
  }
  if (variant.is_blob()) {
    const uint8_t *data = variant.blob_data();
    size_t size = variant.blob_size();
    return (!data || size == 0) ? [NSData data]
                                : [NSData dataWithBytes:data length:size];
  }
  return nil;
}
static FIRRemoteConfigValue *GetValue(const char *key, ValueInfo *info,
                                      FIRRemoteConfig *rc_ptr) {
  auto fail = [&]() -> FIRRemoteConfigValue * {
    if (info) {
      info->source = kValueSourceStaticValue;
      info->conversion_successful = false;
    }
    return nil;
  };
  if (!key || !rc_ptr) {
    if (!key) LogError("Remote Config: Key cannot be nullptr.");
    return fail();
  }
  NSString *ns_key = @(key);
  if (!ns_key) {
    LogError("Remote Config: Key is not valid UTF-8.");
    return fail();
  }
  FIRRemoteConfigValue *value = [rc_ptr configValueForKey:ns_key];
  if (!value) return fail();
  if (info) {
    int source_index = static_cast<int>(value.source);
    if (source_index >= 0 &&
        static_cast<size_t>(source_index) <
            FIREBASE_ARRAYSIZE(kFirebaseRemoteConfigSourceToValueSourceMap)) {
      info->source = kFirebaseRemoteConfigSourceToValueSourceMap[source_index];
      info->conversion_successful = true;
    } else {
      LogWarning("Remote Config: Failed to find a valid source for key %s", key);
      return fail();
    }
  }
  return value;
}
static std::vector<unsigned char> ConvertData(FIRRemoteConfigValue *value) {
  if (!value || !value.dataValue) return {};
  NSData *data = value.dataValue;
  size_t size = static_cast<size_t>([data length]);
  const auto *bytes = static_cast<const unsigned char *>([data bytes]);
  return (size == 0 || !bytes) ? std::vector<unsigned char>{}
                               : std::vector<unsigned char>(bytes, bytes + size);
}
static void GetInfoFromFIRRemoteConfig(FIRRemoteConfig *rc_instance,
                                       ConfigInfo *out_info,
                                       int64_t throttled_end_time) {
  FIREBASE_ASSERT(out_info != nullptr);
  if (!out_info) return;
  *out_info = ConfigInfo{};
  if (!rc_instance) return;
  double fetch_seconds =
      rc_instance.lastFetchTime ? rc_instance.lastFetchTime.timeIntervalSince1970 : 0.0;
  out_info->fetch_time =
      (fetch_seconds <= 0.0 || std::isnan(fetch_seconds) || std::isinf(fetch_seconds))
          ? 0
          : static_cast<uint64_t>(std::llround(
                fetch_seconds * ::firebase::internal::kMillisecondsPerSecond));
  out_info->throttled_end_time =
      static_cast<uint64_t>(throttled_end_time < 0 ? 0 : throttled_end_time) *
      ::firebase::internal::kMillisecondsPerSecond;
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
               static_cast<int>(rc_instance.lastFetchStatus));
      out_info->last_fetch_status = kLastFetchStatusFailure;
      out_info->last_fetch_failure_reason = kFetchFailureReasonError;
      break;
  }
}
static RemoteConfigError ConvertFIRRemoteConfigUpdateError(NSError *error) {
  if (!error) return kRemoteConfigErrorNone;
  switch (error.code) {
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
  if (!keys) return config_update;
  for (NSString *key in keys) {
    if (key) config_update.updated_keys.push_back(util::NSStringToString(key));
  }
  return config_update;
}
namespace internal {
struct RemoteConfigIosState {
  RemoteConfigIosState()
      : future_impl(kRemoteConfigFnCount),
        serial_queue(dispatch_queue_create(
            "com.google.firebase.remote_config.ios", DISPATCH_QUEUE_SERIAL)) {}
  ~RemoteConfigIosState() {
#if !OS_OBJECT_USE_OBJC
    if (serial_queue) dispatch_release(serial_queue);
#endif
  }
  Mutex mutex;
  bool alive = true;
  ReferenceCountedFutureImpl future_impl;
  dispatch_queue_t serial_queue;
  std::vector<std::string> default_keys;
  int64_t throttled_end_time_in_sec = 0;
};
static void UpdateThrottledEndTimeFromError(NSError *error,
                                            RemoteConfigIosState *state) {
  if (!error || !error.userInfo || !state) return;
  id throttled_obj = error.userInfo[FIRRemoteConfigThrottledEndTimeInSecondsKey];
  if ([throttled_obj isKindOfClass:[NSNumber class]]) {
    int64_t throttled_end_time = [(NSNumber *)throttled_obj longLongValue];
    if (throttled_end_time > 0) {
      state->throttled_end_time_in_sec = throttled_end_time;
    }
  }
}
RemoteConfigInternal::RemoteConfigInternal(const firebase::App &app)
    : app_(app), state_(std::make_shared<RemoteConfigIosState>()) {
  impl_.reset(new FIRRemoteConfigPointer(
      [FIRRemoteConfig remoteConfigWithApp:app_.GetPlatformApp()]));
}
RemoteConfigInternal::~RemoteConfigInternal() {
  Cleanup();
  cleanup_notifier().CleanupAll();
}
bool RemoteConfigInternal::Initialized() const { return impl() != nil; }
void RemoteConfigInternal::Cleanup() {
  if (state_) {
    MutexLock lock(state_->mutex);
    state_->alive = false;
  }
}
Future<ConfigInfo> RemoteConfigInternal::EnsureInitialized() {
  const auto handle =
      state_->future_impl.SafeAlloc<ConfigInfo>(kRemoteConfigFnEnsureInitialized);
  FIRRemoteConfig *rc = impl();
  if (!rc) {
    state_->future_impl.CompleteWithResult(
        handle, kFutureStatusFailure, "Remote Config is not initialized.",
        ConfigInfo{});
    return MakeFuture<ConfigInfo>(&state_->future_impl, handle);
  }
  std::shared_ptr<RemoteConfigIosState> state = state_;
  [rc ensureInitializedWithCompletionHandler:^(NSError *error) {
    int64_t throttled_end_time = 0;
    {
      MutexLock lock(state->mutex);
      if (!state->alive) return;
      if (error) UpdateThrottledEndTimeFromError(error, state.get());
      throttled_end_time = state->throttled_end_time_in_sec;
    }
    ConfigInfo config_info;
    GetInfoFromFIRRemoteConfig(rc, &config_info, throttled_end_time);
    state->future_impl.CompleteWithResult(
        handle, error ? kFutureStatusFailure : kFutureStatusSuccess,
        error ? util::NSStringToString(error.localizedDescription).c_str() : "",
        config_info);
  }];
  return MakeFuture<ConfigInfo>(&state_->future_impl, handle);
}
Future<ConfigInfo> RemoteConfigInternal::EnsureInitializedLastResult() {
  return static_cast<const Future<ConfigInfo> &>(
      state_->future_impl.LastResult(kRemoteConfigFnEnsureInitialized));
}
Future<bool> RemoteConfigInternal::Activate() {
  const auto handle = state_->future_impl.SafeAlloc<bool>(kRemoteConfigFnActivate);
  FIRRemoteConfig *rc = impl();
  if (!rc) {
    state_->future_impl.CompleteWithResult(
        handle, kFutureStatusFailure, "Remote Config is not initialized.", false);
    return MakeFuture<bool>(&state_->future_impl, handle);
  }
  std::shared_ptr<RemoteConfigIosState> state = state_;
  [rc activateWithCompletion:^(BOOL changed, NSError *error) {
    {
      MutexLock lock(state->mutex);
      if (!state->alive) return;
      if (error) UpdateThrottledEndTimeFromError(error, state.get());
    }
    state->future_impl.CompleteWithResult(
        handle, error ? kFutureStatusFailure : kFutureStatusSuccess,
        error ? util::NSStringToString(error.localizedDescription).c_str() : "",
        !error && changed);
  }];
  return MakeFuture<bool>(&state_->future_impl, handle);
}
Future<bool> RemoteConfigInternal::ActivateLastResult() {
  return static_cast<const Future<bool> &>(
      state_->future_impl.LastResult(kRemoteConfigFnActivate));
}
Future<bool> RemoteConfigInternal::FetchAndActivate() {
  const auto handle =
      state_->future_impl.SafeAlloc<bool>(kRemoteConfigFnFetchAndActivate);
  FIRRemoteConfig *rc = impl();
  if (!rc) {
    state_->future_impl.CompleteWithResult(
        handle, kFutureStatusFailure, "Remote Config is not initialized.", false);
    return MakeFuture<bool>(&state_->future_impl, handle);
  }
  std::shared_ptr<RemoteConfigIosState> state = state_;
  [rc fetchAndActivateWithCompletionHandler:^(
          FIRRemoteConfigFetchAndActivateStatus status, NSError *error) {
    {
      MutexLock lock(state->mutex);
      if (!state->alive) return;
      if (error) {
        UpdateThrottledEndTimeFromError(error, state.get());
      } else if (status != FIRRemoteConfigFetchAndActivateStatusError) {
        state->throttled_end_time_in_sec = 0;
      }
    }
    if (error || status == FIRRemoteConfigFetchAndActivateStatusError) {
      state->future_impl.CompleteWithResult(
          handle, kFutureStatusFailure,
          error ? util::NSStringToString(error.localizedDescription).c_str()
                : "FetchAndActivate encountered an error.",
          false);
    } else {
      state->future_impl.CompleteWithResult(
          handle, kFutureStatusSuccess, "",
          status != FIRRemoteConfigFetchAndActivateStatusSuccessUsingPreFetchedData);
    }
  }];
  return MakeFuture<bool>(&state_->future_impl, handle);
}
Future<bool> RemoteConfigInternal::FetchAndActivateLastResult() {
  return static_cast<const Future<bool> &>(
      state_->future_impl.LastResult(kRemoteConfigFnFetchAndActivate));
}
Future<void> RemoteConfigInternal::Fetch(uint64_t cache_expiration_in_seconds) {
  const auto handle = state_->future_impl.SafeAlloc<void>(kRemoteConfigFnFetch);
  FIRRemoteConfig *rc = impl();
  if (!rc) {
    state_->future_impl.Complete(handle, kFutureStatusFailure,
                                 "Remote Config is not initialized.");
    return MakeFuture<void>(&state_->future_impl, handle);
  }
  std::shared_ptr<RemoteConfigIosState> state = state_;
  [rc fetchWithExpirationDuration:static_cast<NSTimeInterval>(
                                      cache_expiration_in_seconds)
                completionHandler:^(FIRRemoteConfigFetchStatus status,
                                    NSError *error) {
                  {
                    MutexLock lock(state->mutex);
                    if (!state->alive) return;
                    if (error) {
                      UpdateThrottledEndTimeFromError(error, state.get());
                    } else if (status == FIRRemoteConfigFetchStatusSuccess) {
                      state->throttled_end_time_in_sec = 0;
                    }
                  }
                  if (error || status != FIRRemoteConfigFetchStatusSuccess) {
                    state->future_impl.Complete(
                        handle, kFutureStatusFailure,
                        error ? util::NSStringToString(
                                    error.localizedDescription)
                                    .c_str()
                              : "Fetch encountered an error, either failed or throttled.");
                  } else {
                    state->future_impl.Complete(handle, kFutureStatusSuccess);
                  }
                }];
  return MakeFuture<void>(&state_->future_impl, handle);
}
Future<void> RemoteConfigInternal::FetchLastResult() {
  return static_cast<const Future<void> &>(
      state_->future_impl.LastResult(kRemoteConfigFnFetch));
}
Future<void> RemoteConfigInternal::SetDefaults(
    const ConfigKeyValueVariant *defaults, size_t number_of_defaults) {
  const auto handle =
      state_->future_impl.SafeAlloc<void>(kRemoteConfigFnSetDefaults);
  FIRRemoteConfig *rc = impl();
  if (!rc) {
    state_->future_impl.Complete(handle, kFutureStatusFailure,
                                 "Remote Config is not initialized.");
    return MakeFuture<void>(&state_->future_impl, handle);
  }
  if (number_of_defaults > 0 && defaults == nullptr) {
    LogError("Remote Config: SetDefaults() called with nullptr defaults.");
    state_->future_impl.Complete(
        handle, kFutureStatusFailure,
        "defaults cannot be nullptr when number_of_defaults > 0.");
    return MakeFuture<void>(&state_->future_impl, handle);
  }
  NSMutableDictionary *dict =
      [[NSMutableDictionary alloc] initWithCapacity:number_of_defaults];
  std::vector<std::string> new_default_keys;
  new_default_keys.reserve(number_of_defaults);
  for (size_t i = 0; i < number_of_defaults; ++i) {
    const char *key = defaults[i].key;
    NSString *ns_key = key ? @(key) : nil;
    if (!key || !ns_key || defaults[i].value.is_null()) {
      LogError("Remote Config: Invalid key or null Variant at index %zu.", i);
      continue;
    }
    id value = VariantToNSObject(defaults[i].value);
    if (value) {
      dict[ns_key] = value;
      new_default_keys.push_back(key);
    } else {
      LogError("Remote Config: Invalid Variant type for SetDefaults() key %s", key);
    }
  }
  {
    MutexLock lock(state_->mutex);
    state_->default_keys = std::move(new_default_keys);
  }
  std::shared_ptr<RemoteConfigIosState> state = state_;
  dispatch_async(state->serial_queue, ^{
    {
      MutexLock lock(state->mutex);
      if (!state->alive) return;
    }
    [rc setDefaults:dict];
    state->future_impl.Complete(handle, kFutureStatusSuccess);
  });
  return MakeFuture<void>(&state_->future_impl, handle);
}
Future<void> RemoteConfigInternal::SetDefaults(const ConfigKeyValue *defaults,
                                               size_t number_of_defaults) {
  const auto handle =
      state_->future_impl.SafeAlloc<void>(kRemoteConfigFnSetDefaults);
  FIRRemoteConfig *rc = impl();
  if (!rc) {
    state_->future_impl.Complete(handle, kFutureStatusFailure,
                                 "Remote Config is not initialized.");
    return MakeFuture<void>(&state_->future_impl, handle);
  }
  if (number_of_defaults > 0 && defaults == nullptr) {
    LogError("Remote Config: SetDefaults() called with nullptr defaults.");
    state_->future_impl.Complete(
        handle, kFutureStatusFailure,
        "defaults cannot be nullptr when number_of_defaults > 0.");
    return MakeFuture<void>(&state_->future_impl, handle);
  }
  NSMutableDictionary *dict =
      [[NSMutableDictionary alloc] initWithCapacity:number_of_defaults];
  std::vector<std::string> new_default_keys;
  new_default_keys.reserve(number_of_defaults);
  for (size_t i = 0; i < number_of_defaults; ++i) {
    const char *key = defaults[i].key;
    const char *value = defaults[i].value;
    NSString *ns_key = key ? @(key) : nil;
    NSString *ns_value = value ? @(value) : nil;
    if (!key || !value || !ns_key || !ns_value) {
      LogError("Remote Config: SetDefaults() invalid entry at index %zu.", i);
      continue;
    }
    dict[ns_key] = ns_value;
    new_default_keys.push_back(key);
  }
  {
    MutexLock lock(state_->mutex);
    state_->default_keys = std::move(new_default_keys);
  }
  std::shared_ptr<RemoteConfigIosState> state = state_;
  dispatch_async(state->serial_queue, ^{
    {
      MutexLock lock(state->mutex);
      if (!state->alive) return;
    }
    [rc setDefaults:dict];
    state->future_impl.Complete(handle, kFutureStatusSuccess);
  });
  return MakeFuture<void>(&state_->future_impl, handle);
}
Future<void> RemoteConfigInternal::SetDefaultsLastResult() {
  return static_cast<const Future<void> &>(
      state_->future_impl.LastResult(kRemoteConfigFnSetDefaults));
}
Future<void> RemoteConfigInternal::SetConfigSettings(ConfigSettings settings) {
  const auto handle =
      state_->future_impl.SafeAlloc<void>(kRemoteConfigFnSetConfigSettings);
  FIRRemoteConfig *rc = impl();
  if (!rc) {
    state_->future_impl.Complete(handle, kFutureStatusFailure,
                                 "Remote Config is not initialized.");
    return MakeFuture<void>(&state_->future_impl, handle);
  }
  FIRRemoteConfigSettings *newConfigSettings =
      [[FIRRemoteConfigSettings alloc] init];
  newConfigSettings.minimumFetchInterval =
      static_cast<NSTimeInterval>(settings.minimum_fetch_interval_in_milliseconds) /
      static_cast<NSTimeInterval>(::firebase::internal::kMillisecondsPerSecond);
  newConfigSettings.fetchTimeout =
      static_cast<NSTimeInterval>(settings.fetch_timeout_in_milliseconds) /
      static_cast<NSTimeInterval>(::firebase::internal::kMillisecondsPerSecond);
  std::shared_ptr<RemoteConfigIosState> state = state_;
  dispatch_async(state->serial_queue, ^{
    {
      MutexLock lock(state->mutex);
      if (!state->alive) return;
    }
    [rc setConfigSettings:newConfigSettings];
    state->future_impl.Complete(handle, kFutureStatusSuccess);
  });
  return MakeFuture<void>(&state_->future_impl, handle);
}
Future<void> RemoteConfigInternal::SetConfigSettingsLastResult() {
  return static_cast<const Future<void> &>(
      state_->future_impl.LastResult(kRemoteConfigFnSetConfigSettings));
}
Future<void> RemoteConfigInternal::SetCustomSignals(
    const std::map<std::string, Variant> &custom_signals) {
  const auto handle =
      state_->future_impl.SafeAlloc<void>(kRemoteConfigFnSetCustomSignals);
  FIRRemoteConfig *rc = impl();
  if (!rc) {
    state_->future_impl.Complete(handle, kFutureStatusFailure,
                                 "Remote Config is not initialized.");
    return MakeFuture<void>(&state_->future_impl, handle);
  }
  if (custom_signals.size() > kCustomSignalsMaxCount) {
    state_->future_impl.Complete(handle, kFutureStatusFailure,
                                 "Too many custom signals (maximum 100).");
    return MakeFuture<void>(&state_->future_impl, handle);
  }
  NSMutableDictionary *dict =
      [[NSMutableDictionary alloc] initWithCapacity:custom_signals.size()];
  for (const auto &[key, signal_val] : custom_signals) {
    if (key.empty() || key.size() > kCustomSignalsMaxKeyLength) {
      state_->future_impl.Complete(
          handle, kFutureStatusFailure,
          "Custom signal key must be non-empty and at most 256 characters.");
      return MakeFuture<void>(&state_->future_impl, handle);
    }
    NSString *ns_key = [[NSString alloc] initWithBytes:key.data()
                                                length:key.size()
                                              encoding:NSUTF8StringEncoding];
    if (!ns_key) {
      state_->future_impl.Complete(handle, kFutureStatusFailure,
                                   "Custom signal key is not valid UTF-8.");
      return MakeFuture<void>(&state_->future_impl, handle);
    }
    if (signal_val.is_null()) {
      dict[ns_key] = [NSNull null];
    } else if (signal_val.is_string() || signal_val.is_int64() ||
               signal_val.is_double()) {
      if (signal_val.is_string()) {
        size_t str_len =
            signal_val.is_mutable_string()
                ? signal_val.mutable_string().size()
                : (signal_val.string_value() ? strlen(signal_val.string_value())
                                             : 0);
        if (str_len > kCustomSignalsMaxStringValueLength) {
          state_->future_impl.Complete(
              handle, kFutureStatusFailure,
              "Custom signal string value exceeds 500 characters.");
          return MakeFuture<void>(&state_->future_impl, handle);
        }
      }
      id value = VariantToNSObject(signal_val);
      if (!value) {
        state_->future_impl.Complete(
            handle, kFutureStatusFailure,
            "Custom signal string value is not valid UTF-8.");
        return MakeFuture<void>(&state_->future_impl, handle);
      }
      dict[ns_key] = value;
    } else {
      state_->future_impl.Complete(
          handle, kFutureStatusFailure,
          "Invalid value type. Must be String, Int, Double, or Null.");
      return MakeFuture<void>(&state_->future_impl, handle);
    }
  }
  std::shared_ptr<RemoteConfigIosState> state = state_;
  [rc setCustomSignals:dict
        withCompletion:^(NSError *_Nullable error) {
          {
            MutexLock lock(state->mutex);
            if (!state->alive) return;
          }
          if (error) {
            state->future_impl.Complete(
                handle, kFutureStatusFailure,
                util::NSStringToString(error.localizedDescription).c_str());
          } else {
            state->future_impl.Complete(handle, kFutureStatusSuccess);
          }
        }];
  return MakeFuture<void>(&state_->future_impl, handle);
}
Future<void> RemoteConfigInternal::SetCustomSignalsLastResult() {
  return static_cast<const Future<void> &>(
      state_->future_impl.LastResult(kRemoteConfigFnSetCustomSignals));
}
ConfigSettings RemoteConfigInternal::GetConfigSettings() const {
  ConfigSettings settings;
  FIRRemoteConfig *rc = impl();
  if (!rc || !rc.configSettings) return settings;
  double min_interval = rc.configSettings.minimumFetchInterval;
  double timeout = rc.configSettings.fetchTimeout;
  settings.minimum_fetch_interval_in_milliseconds =
      (min_interval > 0.0 && !std::isnan(min_interval) && !std::isinf(min_interval))
          ? static_cast<uint64_t>(std::llround(
                min_interval * ::firebase::internal::kMillisecondsPerSecond))
          : 0;
  settings.fetch_timeout_in_milliseconds =
      (timeout > 0.0 && !std::isnan(timeout) && !std::isinf(timeout))
          ? static_cast<uint64_t>(std::llround(
                timeout * ::firebase::internal::kMillisecondsPerSecond))
          : 0;
  return settings;
}
bool RemoteConfigInternal::GetBoolean(const char *key, ValueInfo *info) {
  FIRRemoteConfigValue *value = GetValue(key, info, impl());
  if (!value) return false;
  if (value.source == FIRRemoteConfigSourceStatic) {
    if (info) info->conversion_successful = true;
    return false;
  }
  bool parsed = false;
  bool ok = ValueParser::ParseBoolean(util::NSStringToString(value.stringValue),
                                      &parsed);
  if (info && info->conversion_successful) info->conversion_successful = ok;
  return ok ? parsed : static_cast<bool>(value.boolValue);
}
int64_t RemoteConfigInternal::GetLong(const char *key, ValueInfo *info) {
  FIRRemoteConfigValue *value = GetValue(key, info, impl());
  if (!value) return 0;
  if (value.source == FIRRemoteConfigSourceStatic) {
    if (info) info->conversion_successful = true;
    return 0;
  }
  int64_t parsed = 0;
  bool ok = ValueParser::ParseInt64(util::NSStringToString(value.stringValue),
                                    &parsed);
  if (info && info->conversion_successful) info->conversion_successful = ok;
  return ok ? parsed : value.numberValue.longLongValue;
}
double RemoteConfigInternal::GetDouble(const char *key, ValueInfo *info) {
  FIRRemoteConfigValue *value = GetValue(key, info, impl());
  if (!value) return 0.0;
  if (value.source == FIRRemoteConfigSourceStatic) {
    if (info) info->conversion_successful = true;
    return 0.0;
  }
  double parsed = 0.0;
  bool ok = ValueParser::ParseDouble(util::NSStringToString(value.stringValue),
                                     &parsed);
  if (info && info->conversion_successful) info->conversion_successful = ok;
  return ok ? parsed : value.numberValue.doubleValue;
}
std::string RemoteConfigInternal::GetString(const char *key, ValueInfo *info) {
  FIRRemoteConfigValue *value = GetValue(key, info, impl());
  return value ? util::NSStringToString(value.stringValue) : "";
}
std::vector<unsigned char> RemoteConfigInternal::GetData(const char *key,
                                                         ValueInfo *info) {
  return ConvertData(GetValue(key, info, impl()));
}
std::vector<std::string> RemoteConfigInternal::GetKeysByPrefix(
    const char *prefix) {
  std::vector<std::string> keys;
  std::set<std::string> key_set;
  FIRRemoteConfig *rc = impl();
  if (rc) {
    NSString *prefix_string = prefix ? @(prefix) : nil;
    for (NSString *key in [rc keysWithPrefix:prefix_string]) {
      if (!key) continue;
      std::string key_str = util::NSStringToString(key);
      if (key_set.insert(key_str).second) keys.push_back(std::move(key_str));
    }
  }
  std::vector<std::string> default_keys_copy;
  if (state_) {
    MutexLock lock(state_->mutex);
    default_keys_copy = state_->default_keys;
  }
  std::string prefix_str = prefix ? prefix : "";
  for (const std::string &default_key : default_keys_copy) {
    if (key_set.find(default_key) == key_set.end() &&
        (prefix_str.empty() ||
         default_key.compare(0, prefix_str.size(), prefix_str) == 0)) {
      keys.push_back(default_key);
      key_set.insert(default_key);
    }
  }
  return keys;
}
std::vector<std::string> RemoteConfigInternal::GetKeys() {
  return GetKeysByPrefix(nullptr);
}
static Variant FIRValueToVariant(FIRRemoteConfigValue *in_value) {
  if (!in_value) return Variant::Null();
  if (in_value.stringValue != nil) {
    return ValueParser::CoerceToVariant(
        util::NSStringToString(in_value.stringValue));
  }
  NSData *data = in_value.dataValue;
  if (data != nil) {
    size_t size = static_cast<size_t>([data length]);
    const auto *bytes = static_cast<const unsigned char *>([data bytes]);
    return (size == 0 || !bytes) ? Variant::FromMutableBlob("", 0)
                                 : Variant::FromMutableBlob(bytes, size);
  }
  return Variant::Null();
}
std::map<std::string, Variant> RemoteConfigInternal::GetAll() {
  std::map<std::string, Variant> value;
  FIRRemoteConfig *rc = impl();
  if (!rc) return value;
  for (const std::string &key : GetKeys()) {
    NSString *ns_key = @(key.c_str());
    if (ns_key) {
      value.emplace(key, FIRValueToVariant([rc configValueForKey:ns_key]));
    }
  }
  return value;
}
ConfigInfo RemoteConfigInternal::GetInfo() const {
  ConfigInfo config_info;
  int64_t throttled_end_time = 0;
  if (state_) {
    MutexLock lock(state_->mutex);
    throttled_end_time = state_->throttled_end_time_in_sec;
  }
  GetInfoFromFIRRemoteConfig(impl(), &config_info, throttled_end_time);
  return config_info;
}
ConfigUpdateListenerRegistration
RemoteConfigInternal::AddOnConfigUpdateListener(
    std::function<void(ConfigUpdate &&, RemoteConfigError)>
        config_update_listener) {
  FIRRemoteConfig *rc = impl();
  if (!rc) return ConfigUpdateListenerRegistration();
  std::weak_ptr<RemoteConfigIosState> weak_state = state_;
  FIRConfigUpdateListenerRegistration *registration =
      [rc addOnConfigUpdateListener:^(FIRRemoteConfigUpdate *_Nullable update,
                                      NSError *_Nullable error) {
        auto state = weak_state.lock();
        if (!state) return;
        {
          MutexLock lock(state->mutex);
          if (!state->alive) return;
        }
        if (!config_update_listener) return;
        if (error) {
          config_update_listener({}, ConvertFIRRemoteConfigUpdateError(error));
          return;
        }
        config_update_listener(
            ConvertConfigUpdateKeys(update ? update.updatedKeys : nil),
            kRemoteConfigErrorNone);
      }];
  auto *registration_internal = new ConfigUpdateListenerRegistrationInternal(
      this, [registration]() { [registration remove]; });
  cleanup_notifier().RegisterObject(registration_internal, [](void *reg) {
    delete reinterpret_cast<ConfigUpdateListenerRegistrationInternal *>(reg);
  });
  return ConfigUpdateListenerRegistration(registration_internal);
}
}  // namespace internal
}  // namespace firebase::remote_config
