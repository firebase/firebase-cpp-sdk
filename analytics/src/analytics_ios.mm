/*
 * Copyright 2016 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#import <Foundation/Foundation.h>

#import "FIRAnalytics+Consent.h"
#import "FIRAnalytics+OnDevice.h"
#import "FIRAnalytics.h"

#include <cassert>
#include <chrono>
#include <cstdint>
#include <memory>
#include "analytics/src/analytics_common.h"
#include "analytics/src/include/firebase/analytics.h"

// Include the generated Swift header for the C++ bridge.
#include "firebase_analytics-Swift.h"

#include "app/src/assert.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/include/firebase/version.h"
#include "app/src/log.h"
#include "app/src/thread.h"
#include "app/src/time.h"
#include "app/src/util.h"
#include "app/src/util_ios.h"

namespace firebase {
namespace analytics {

// Used to workaround b/143656277 and b/110166640
class AnalyticsDataResetter {
 private:
  enum ResetState {
    kResetStateNone = 0,
    kResetStateRequested,
    kResetStateRetry,
  };

 public:
  // Initialize the class.
  AnalyticsDataResetter()
      : reset_state_(kResetStateNone),
        reset_timestamp_(std::chrono::steady_clock::now()) {}

  // Reset analytics data.
  void Reset() {
    reset_timestamp_ = std::chrono::steady_clock::now();
    reset_state_ = kResetStateRequested;
    instance_id_ = util::StringFromNSString([FIRAnalytics appInstanceID]);
    [FIRAnalytics resetAnalyticsData];
  }

  // Get the instance ID, returning a non-empty string if it's valid or an empty string if it's
  // still being reset.
  std::string GetInstanceId() {
    std::string current_instance_id = util::StringFromNSString([FIRAnalytics appInstanceID]);
    if (reset_state_ == kResetStateNone) {
      instance_id_ = current_instance_id;
      return current_instance_id;
    }

    if (!current_instance_id.empty() && current_instance_id != instance_id_) {
      // Observed a new instance ID after reset; transition back to idle so
      // subsequent calls do not trigger another reset.
      reset_state_ = kResetStateNone;
      instance_id_ = current_instance_id;
      return current_instance_id;
    }

    int64_t reset_time_elapsed_milliseconds = GetResetTimeElapsedMilliseconds();
    if (reset_state_ == kResetStateRequested &&
        reset_time_elapsed_milliseconds >= kResetRetryIntervalMilliseconds) {
      // Firebase Analytics on iOS can take a while to initialize, in this case we try to reset
      // again if the instance ID hasn't changed for a while. Keep reset_timestamp_ anchored to
      // the original Reset() call so total elapsed time reaches kResetTimeoutMilliseconds within
      // GetAnalyticsInstanceId()'s poll window.
      reset_state_ = kResetStateRetry;
      [FIRAnalytics resetAnalyticsData];
      return std::string();
    }

    if (reset_time_elapsed_milliseconds < kResetTimeoutMilliseconds) {
      return std::string();
    }

    reset_state_ = kResetStateNone;
    instance_id_ = current_instance_id;
    return current_instance_id;
  }

 private:
  // Get the monotonic time elapsed in milliseconds since reset was requested.
  int64_t GetResetTimeElapsedMilliseconds() const {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now() - reset_timestamp_)
        .count();
  }

 private:
  // Reset attempt.
  ResetState reset_state_;
  // When a reset was first requested (monotonic clock).
  std::chrono::steady_clock::time_point reset_timestamp_;
  // Instance ID before it was reset.
  std::string instance_id_;

  // Time to wait before trying to reset again.
  static const int64_t kResetRetryIntervalMilliseconds;
  // Time to wait before giving up on resetting the ID.
  static const int64_t kResetTimeoutMilliseconds;
};

DEFINE_FIREBASE_VERSION_STRING(FirebaseAnalytics);

const int64_t AnalyticsDataResetter::kResetRetryIntervalMilliseconds = 1000;
const int64_t AnalyticsDataResetter::kResetTimeoutMilliseconds = 5000;

static const double kMillisecondsPerSecond = 1000.0;
static Mutex g_mutex;  // NOLINT
static bool g_initialized = false;
static AnalyticsDataResetter* g_resetter = nullptr;

// Initialize the API.
void Initialize(const ::firebase::App& app) {
  {
    MutexLock lock(g_mutex);
    if (g_initialized) {
      LogWarning("%s API already initialized", internal::kAnalyticsModuleName);
      return;
    }
    g_initialized = true;
    g_resetter = new AnalyticsDataResetter();
    internal::FutureData::Create();
  }
  internal::RegisterTerminateOnAppDestroy(&app);
}

namespace internal {

// Determine whether the analytics module is initialized.
bool IsInitialized() {
  MutexLock lock(g_mutex);
  return g_initialized;
}

}  // namespace internal

// Terminate the API.
void Terminate() {
  {
    MutexLock lock(g_mutex);
    if (!g_initialized) {
      LogWarning("%s API already shut down", internal::kAnalyticsModuleName);
      return;
    }
    internal::FutureData::Destroy();
    delete g_resetter;
    g_resetter = nullptr;
    g_initialized = false;
  }
  internal::UnregisterTerminateOnAppDestroy();
}

// Enable / disable measurement and reporting.
void SetAnalyticsCollectionEnabled(bool enabled) {
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  [FIRAnalytics setAnalyticsCollectionEnabled:enabled];
}

void SetConsent(const std::map<ConsentType, ConsentStatus>& consent_settings) {
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  NSMutableDictionary* consent_settings_dict =
      [NSMutableDictionary dictionaryWithCapacity:consent_settings.size()];
  for (auto it = consent_settings.begin(); it != consent_settings.end(); ++it) {
    FIRConsentType consent_type;
    switch (it->first) {
      case kConsentTypeAdStorage:
        consent_type = FIRConsentTypeAdStorage;
        break;
      case kConsentTypeAnalyticsStorage:
        consent_type = FIRConsentTypeAnalyticsStorage;
        break;
      case kConsentTypeAdUserData:
        consent_type = FIRConsentTypeAdUserData;
        break;
      case kConsentTypeAdPersonalization:
        consent_type = FIRConsentTypeAdPersonalization;
        break;
      default:
        LogError("Unknown ConsentType value: %d", static_cast<int>(it->first));
        return;
    };
    FIRConsentStatus consent_status;
    switch (it->second) {
      case kConsentStatusGranted:
        consent_status = FIRConsentStatusGranted;
        break;
      case kConsentStatusDenied:
        consent_status = FIRConsentStatusDenied;
        break;
      default:
        LogError("Unknown ConsentStatus value: %d", static_cast<int>(it->second));
        return;
    };
    [consent_settings_dict setObject:consent_status forKey:consent_type];
  }
  [FIRAnalytics setConsent:consent_settings_dict];
}

// Due to overloads of LogEvent(), it's possible for users to call variants that require a
// string with a nullptr.  To prevent a crash and instead yield a warning, pass an empty string
// to logEventWithName: methods instead.  See b/30061553 for the background.
NSString* SafeString(const char* event_name) { return @(event_name ? event_name : ""); }

// Log an event with one string parameter.
void LogEvent(const char* name, const char* parameter_name, const char* parameter_value) {
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  if (!name || name[0] == '\0') {
    LogError("LogEvent: Event name cannot be null or empty.");
    return;
  }
  if (!parameter_name) {
    LogError("LogEvent(%s): Parameter name cannot be null.", name);
    return;
  }
  [FIRAnalytics logEventWithName:SafeString(name)
                      parameters:@{SafeString(parameter_name) : SafeString(parameter_value)}];
}

// Log an event with one double parameter.
void LogEvent(const char* name, const char* parameter_name, const double parameter_value) {
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  if (!name || name[0] == '\0') {
    LogError("LogEvent: Event name cannot be null or empty.");
    return;
  }
  if (!parameter_name) {
    LogError("LogEvent(%s): Parameter name cannot be null.", name);
    return;
  }
  [FIRAnalytics
      logEventWithName:SafeString(name)
            parameters:@{SafeString(parameter_name) : [NSNumber numberWithDouble:parameter_value]}];
}

// Log an event with one 64-bit integer parameter.
void LogEvent(const char* name, const char* parameter_name, const int64_t parameter_value) {
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  if (!name || name[0] == '\0') {
    LogError("LogEvent: Event name cannot be null or empty.");
    return;
  }
  if (!parameter_name) {
    LogError("LogEvent(%s): Parameter name cannot be null.", name);
    return;
  }
  [FIRAnalytics logEventWithName:SafeString(name)
                      parameters:@{
                        SafeString(parameter_name) : [NSNumber numberWithLongLong:parameter_value]
                      }];
}

/// Log an event with one integer parameter (stored as a 64-bit integer).
void LogEvent(const char* name, const char* parameter_name, const int parameter_value) {
  LogEvent(name, parameter_name, static_cast<int64_t>(parameter_value));
}

/// Log an event with no parameters.
void LogEvent(const char* name) {
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  if (!name || name[0] == '\0') {
    LogError("LogEvent: Event name cannot be null or empty.");
    return;
  }
  [FIRAnalytics logEventWithName:SafeString(name) parameters:@{}];
}

Future<void> LogAppleTransaction(const char* transaction_id) {
  MutexLock lock(g_mutex);
  FIREBASE_ASSERT_RETURN(Future<void>(), g_initialized);

  std::shared_ptr<internal::FutureData> future_data = internal::FutureData::GetShared();
  if (!future_data) {
    return Future<void>();
  }
  auto* api = future_data->api();
  const auto future_handle = api->SafeAlloc<void>(internal::kAnalyticsFnLogAppleTransaction);

  if (!transaction_id) {
    api->Complete(future_handle, -1, "Transaction ID is null");
    return Future<void>(api, future_handle.get());
  }

  std::weak_ptr<internal::FutureData> weak_future_data = future_data;
  [AppleTransactionVerifier
      verifyWithTransactionId:SafeString(transaction_id)
                   completion:^(BOOL isFound, NSString* _Nullable errorMessage) {
                     MutexLock lock(g_mutex);
                     auto locked_future_data = weak_future_data.lock();
                     if (!g_initialized || !locked_future_data ||
                         locked_future_data != internal::FutureData::GetShared()) {
                       return;
                     }

                     auto* current_api = locked_future_data->api();
                     if (isFound) {
                       current_api->Complete(future_handle, 0, "");
                     } else if (errorMessage != nil) {
                       current_api->Complete(
                           future_handle, -1,
                           util::NSStringToString(errorMessage).c_str());
                     } else {
                       current_api->Complete(future_handle, -1,
                                             "StoreKit 2 transaction not found.");
                     }
                   }];

  return Future<void>(api, future_handle.get());
}

Future<void> LogAppleTransactionLastResult() {
  MutexLock lock(g_mutex);
  FIREBASE_ASSERT_RETURN(Future<void>(), g_initialized);
  std::shared_ptr<internal::FutureData> future_data = internal::FutureData::GetShared();
  if (!future_data) {
    return Future<void>();
  }
  return static_cast<const Future<void>&>(
      future_data->api()->LastResult(internal::kAnalyticsFnLogAppleTransaction));
}

// Declared here so that it can be used, defined below.
NSDictionary* MapToDictionary(const std::map<Variant, Variant>& map);

// Converts the given vector of Maps into an ObjC NSArray of ObjC NSDictionaries.
NSArray* VectorOfMapsToArray(const std::vector<Variant>& vector) {
  NSMutableArray* array = [NSMutableArray arrayWithCapacity:vector.size()];
  for (const Variant& element : vector) {
    if (element.is_map()) {
      NSDictionary* dict = MapToDictionary(element.map());
      [array addObject:dict];
    } else {
      LogError("VectorOfMapsToArray: Unsupported type (%s) within vector.",
               Variant::TypeName(element.type()));
    }
  }
  return array;
}

// Converts and adds the Variant to the given Dictionary.
bool AddVariantToDictionary(NSMutableDictionary* dict, NSString* key, const Variant& value,
                            bool allow_null_clear = false) {
  if (value.is_int64()) {
    [dict setObject:[NSNumber numberWithLongLong:value.int64_value()] forKey:key];
  } else if (value.is_double()) {
    [dict setObject:[NSNumber numberWithDouble:value.double_value()] forKey:key];
  } else if (value.is_string()) {
    [dict setObject:SafeString(value.string_value()) forKey:key];
  } else if (value.is_bool()) {
    // Just use integer 0 or 1.
    [dict setObject:[NSNumber numberWithLongLong:value.bool_value() ? 1 : 0] forKey:key];
  } else if (value.is_null()) {
    if (allow_null_clear) {
      [dict setObject:[NSNull null] forKey:key];
    } else {
      // Just use integer 0 for null.
      [dict setObject:[NSNumber numberWithLongLong:0] forKey:key];
    }
  } else if (value.is_vector()) {
    NSArray* array = VectorOfMapsToArray(value.vector());
    [dict setObject:array forKey:key];
  } else if (value.is_map()) {
    NSDictionary* inner_dict = MapToDictionary(value.map());
    [dict setObject:inner_dict forKey:key];
  } else {
    // A Variant type that couldn't be handled was passed in.
    return false;
  }
  return true;
}

// Converts the given map into an ObjC dictionary of ObjC objects.
NSDictionary* MapToDictionary(const std::map<Variant, Variant>& map) {
  NSMutableDictionary* dict = [NSMutableDictionary dictionaryWithCapacity:map.size()];
  for (const auto& pair : map) {
    // Only add elements that use a string key
    if (!pair.first.is_string() || !pair.first.string_value()) {
      continue;
    }
    NSString* key = SafeString(pair.first.string_value());
    const Variant& value = pair.second;
    if (!AddVariantToDictionary(dict, key, value)) {
      LogError("MapToDictionary: Unsupported type (%s) within map with key %s.",
               Variant::TypeName(value.type()), key.UTF8String);
    }
  }
  return dict;
}

// Log an event with associated parameters.
void LogEvent(const char* name, const Parameter* parameters, size_t number_of_parameters) {
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  if (!name || name[0] == '\0') {
    LogError("LogEvent: Event name cannot be null or empty.");
    return;
  }
  if (!parameters && number_of_parameters > 0) {
    LogError("LogEvent(%s): parameters cannot be null when number_of_parameters > 0.", name);
    return;
  }
  NSMutableDictionary* parameters_dict =
      [NSMutableDictionary dictionaryWithCapacity:number_of_parameters];
  for (size_t i = 0; i < number_of_parameters; ++i) {
    const Parameter& parameter = parameters[i];
    if (!parameter.name) {
      LogError("LogEvent(%s): Parameter name cannot be null.", name);
      continue;
    }
    NSString* parameter_name = SafeString(parameter.name);
    if (!AddVariantToDictionary(parameters_dict, parameter_name, parameter.value)) {
      // A Variant type that couldn't be handled was passed in.
      LogError("LogEvent(%s): %s is not a valid parameter value type. "
               "Parameter was not added.",
               parameter.name,
               Variant::TypeName(parameter.value.type()));
    }
  }
  [FIRAnalytics logEventWithName:SafeString(name) parameters:parameters_dict];
}

// log an event and the associated parameters via a vector.
void LogEvent(const char* name, const std::vector<Parameter>& parameters) {
  LogEvent(name, parameters.data(), parameters.size());
}

// Sets the default event parameter
void SetDefaultEventParameters(const Parameter* parameters, size_t number_of_parameters) {
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  if (!parameters && number_of_parameters > 0) {
    LogError("SetDefaultEventParameters: parameters cannot be null when "
             "number_of_parameters > 0.");
    return;
  }

  if (!parameters || number_of_parameters == 0) {
    [FIRAnalytics setDefaultEventParameters:nil];
    return;
  }

  NSMutableDictionary* parameters_dict =
      [NSMutableDictionary dictionaryWithCapacity:number_of_parameters];
  for (size_t i = 0; i < number_of_parameters; ++i) {
    const Parameter& parameter = parameters[i];
    if (!parameter.name) {
      LogError("SetDefaultEventParameters: Parameter name cannot be null.");
      continue;
    }
    NSString* parameter_name = SafeString(parameter.name);
    if (!AddVariantToDictionary(parameters_dict, parameter_name, parameter.value,
                                /*allow_null_clear=*/true)) {
      // A Variant type that couldn't be handled was passed in.
      LogError("SetDefaultEventParameters(%s): %s is not a valid parameter value type. "
               "The parameter was not added.",
               parameter.name,
               Variant::TypeName(parameter.value.type()));
    }
  }
  [FIRAnalytics setDefaultEventParameters:parameters_dict];
}

// Sets the default event parameters via a vector
void SetDefaultEventParameters(const std::vector<Parameter>& parameters) {
  SetDefaultEventParameters(parameters.data(), parameters.size());
}

/// Initiates on-device conversion measurement given a user email address on iOS (no-op on
/// Android). On iOS, requires dependency GoogleAppMeasurementOnDeviceConversion to be linked
/// in, otherwise it is a no-op.
void InitiateOnDeviceConversionMeasurementWithEmailAddress(const char* email_address) {
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  if (!email_address) {
    LogError("InitiateOnDeviceConversionMeasurementWithEmailAddress: "
             "email_address cannot be null.");
    return;
  }
  [FIRAnalytics
      initiateOnDeviceConversionMeasurementWithEmailAddress:SafeString(email_address)];
}

/// Initiates on-device conversion measurement given a phone number on iOS (no-op on
/// Android). On iOS, requires dependency GoogleAppMeasurementOnDeviceConversion to be linked
/// in, otherwise it is a no-op.
void InitiateOnDeviceConversionMeasurementWithPhoneNumber(const char* phone_number) {
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  if (!phone_number) {
    LogError("InitiateOnDeviceConversionMeasurementWithPhoneNumber: "
             "phone_number cannot be null.");
    return;
  }
  [FIRAnalytics
      initiateOnDeviceConversionMeasurementWithPhoneNumber:SafeString(phone_number)];
}

/// Initiates on-device conversion measurement given a hashed user email address
/// on iOS (no-op on Android). On iOS, requires dependency
/// GoogleAppMeasurementOnDeviceConversion to be linked in, otherwise it is a
/// no-op.
void InitiateOnDeviceConversionMeasurementWithHashedEmailAddress(
    std::vector<unsigned char> hashed_email) {
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  NSData* hashed_email_data =
      firebase::util::BytesToNSData(hashed_email.data(), hashed_email.size());
  [FIRAnalytics initiateOnDeviceConversionMeasurementWithHashedEmailAddress:hashed_email_data];
}

/// Initiates on-device conversion measurement given a hashed phone number on
/// iOS (no-op on Android). On iOS, requires dependency
/// GoogleAppMeasurementOnDeviceConversion to be linked in, otherwise it is a
/// no-op.
void InitiateOnDeviceConversionMeasurementWithHashedPhoneNumber(
    std::vector<unsigned char> hashed_phone) {
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  NSData* hashed_phone_data =
      firebase::util::BytesToNSData(hashed_phone.data(), hashed_phone.size());
  [FIRAnalytics initiateOnDeviceConversionMeasurementWithHashedPhoneNumber:hashed_phone_data];
}

// Set a user property to the given value.
void SetUserProperty(const char* name, const char* value) {
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  if (!name || name[0] == '\0') {
    LogError("SetUserProperty: Property name cannot be null or empty.");
    return;
  }
  [FIRAnalytics setUserPropertyString:(value ? SafeString(value) : nil)
                              forName:SafeString(name)];
}

// Sets the user ID property. This feature must be used in accordance with
// <a href="https://www.google.com/policies/privacy">
// Google's Privacy Policy</a>
void SetUserId(const char* user_id) {
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  [FIRAnalytics setUserID:(user_id ? SafeString(user_id) : nil)];
}

// Sets the duration of inactivity that terminates the current session.
void SetSessionTimeoutDuration(int64_t milliseconds) {
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  [FIRAnalytics
      setSessionTimeoutInterval:static_cast<NSTimeInterval>(milliseconds) / kMillisecondsPerSecond];
}

void ResetAnalyticsData() {
  MutexLock lock(g_mutex);
  FIREBASE_ASSERT_RETURN_VOID(g_initialized);
  g_resetter->Reset();
}

// No-Op on iOS and Android. Only used in Windows desktop apps.
void SetLogCallback(const LogCallback&) {}

// No-Op on iOS and Android. Only used in Windows desktop apps.
void NotifyAppLifecycleChange(AppLifecycleState) {}

// NO-OP in Android and iOS. Only used in Windows.
bool IsDesktopInitialized() { return false; }

// NO-OP in Android and iOS. Only used in Windows.
void SetDesktopDebugMode(bool) {}

Future<std::string> GetAnalyticsInstanceId() {
  MutexLock lock(g_mutex);
  FIREBASE_ASSERT_RETURN(Future<std::string>(), g_initialized);
  std::shared_ptr<internal::FutureData> future_data = internal::FutureData::GetShared();
  if (!future_data) {
    return Future<std::string>();
  }
  auto* api = future_data->api();
  const auto future_handle =
      api->SafeAlloc<std::string>(internal::kAnalyticsFnGetAnalyticsInstanceId);
  static const int kPollTimeMs = 100;
  static const int64_t kPollTimeoutMs = 6500;
  std::weak_ptr<internal::FutureData> weak_future_data = future_data;
  Thread get_id_thread([weak_future_data, future_handle]() {
    @autoreleasepool {
      const auto start_time = std::chrono::steady_clock::now();
      for (;;) {
      {
        MutexLock lock(g_mutex);
        auto locked_future_data = weak_future_data.lock();
        if (!g_initialized || !locked_future_data ||
            locked_future_data != internal::FutureData::GetShared()) {
          break;
        }
        std::string instance_id = g_resetter->GetInstanceId();
        if (!instance_id.empty()) {
          locked_future_data->api()->CompleteWithResult(future_handle, 0, "",
                                                        instance_id);
          break;
        }
        const auto elapsed_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start_time)
                .count();
        if (elapsed_ms >= kPollTimeoutMs) {
          locked_future_data->api()->CompleteWithResult(
              future_handle, -1,
              "Failed to retrieve Analytics Instance ID: timed out "
              "(Analytics collection may be disabled).",
              std::string(""));
          break;
        }
      }
      firebase::internal::Sleep(kPollTimeMs);
      }
    }
  });
  get_id_thread.Detach();
  return Future<std::string>(api, future_handle.get());
}

Future<std::string> GetAnalyticsInstanceIdLastResult() {
  MutexLock lock(g_mutex);
  FIREBASE_ASSERT_RETURN(Future<std::string>(), g_initialized);
  std::shared_ptr<internal::FutureData> future_data = internal::FutureData::GetShared();
  if (!future_data) {
    return Future<std::string>();
  }
  return static_cast<const Future<std::string>&>(
      future_data->api()->LastResult(internal::kAnalyticsFnGetAnalyticsInstanceId));
}

Future<int64_t> GetSessionId() {
  MutexLock lock(g_mutex);
  FIREBASE_ASSERT_RETURN(Future<int64_t>(), g_initialized);
  std::shared_ptr<internal::FutureData> future_data = internal::FutureData::GetShared();
  if (!future_data) {
    return Future<int64_t>();
  }
  auto* api = future_data->api();
  const auto future_handle = api->SafeAlloc<int64_t>(internal::kAnalyticsFnGetSessionId);
  std::weak_ptr<internal::FutureData> weak_future_data = future_data;
  [FIRAnalytics sessionIDWithCompletion:^(int64_t session_id, NSError* _Nullable error) {
    MutexLock lock(g_mutex);
    auto locked_future_data = weak_future_data.lock();
    if (!g_initialized || !locked_future_data ||
        locked_future_data != internal::FutureData::GetShared()) {
      return;
    }
    auto* current_api = locked_future_data->api();
    if (error) {
      current_api->CompleteWithResult(
          future_handle, -1,
          util::NSStringToString(error.localizedDescription).c_str(),
          int64_t(0));
    } else {
      current_api->CompleteWithResult(future_handle, 0, "", session_id);
    }
  }];
  return MakeFuture<int64_t>(api, future_handle);
}

Future<int64_t> GetSessionIdLastResult() {
  MutexLock lock(g_mutex);
  FIREBASE_ASSERT_RETURN(Future<int64_t>(), g_initialized);
  std::shared_ptr<internal::FutureData> future_data = internal::FutureData::GetShared();
  if (!future_data) {
    return Future<int64_t>();
  }
  return static_cast<const Future<int64_t>&>(
      future_data->api()->LastResult(internal::kAnalyticsFnGetSessionId));
}

}  // namespace analytics
}  // namespace firebase
