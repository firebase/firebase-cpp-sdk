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

Future<void> RemoteConfigInternal::SetConfigSettings(ConfigSettings settings) {
  const auto handle = future_impl_.SafeAlloc<void>(kRemoteConfigFnSetConfigSettings);
  FIRRemoteConfigSettings *config_settings = impl().configSettings;
  config_settings.minimumFetchInterval =
      static_cast<NSTimeInterval>(settings.minimum_fetch_interval_in_milliseconds /
                                  ::firebase::internal::kMillisecondsPerSecond);
  config_settings.fetchTimeout = static_cast<NSTimeInterval>(
      settings.fetch_timeout_in_milliseconds / ::firebase::internal::kMillisecondsPerSecond);
  future_impl_.Complete(handle, kFutureStatusSuccess);
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
}  // namespace internal
}  // namespace remote_config
}  // namespace firebase
