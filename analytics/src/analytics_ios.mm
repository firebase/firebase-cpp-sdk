#import <Foundation/Foundation.h>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <memory>
#import "FIRAnalytics+Consent.h"
#import "FIRAnalytics+OnDevice.h"
#import "FIRAnalytics.h"
#include "analytics/src/analytics_common.h"
#include "analytics/src/include/firebase/analytics.h"
#include "app/src/assert.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/include/firebase/version.h"
#include "app/src/log.h"
#include "app/src/thread.h"
#include "app/src/time.h"
#include "app/src/util.h"
#include "app/src/util_ios.h"
#include "firebase_analytics-Swift.h"
namespace firebase {
namespace analytics {
class AnalyticsDataResetter {
 public:
  AnalyticsDataResetter()
      : reset_state_(kResetStateNone),
        reset_timestamp_(std::chrono::steady_clock::now()) {}
  void Reset() {
    reset_timestamp_ = std::chrono::steady_clock::now();
    reset_state_ = kResetStateRequested;
    instance_id_ = util::StringFromNSString([FIRAnalytics appInstanceID]);
    [FIRAnalytics resetAnalyticsData];
  }
  std::string GetInstanceId() {
    std::string current_id =
        util::StringFromNSString([FIRAnalytics appInstanceID]);
    if (reset_state_ == kResetStateNone ||
        (!current_id.empty() && current_id != instance_id_)) {
      reset_state_ = kResetStateNone;
      instance_id_ = current_id;
      return current_id;
    }
    int64_t elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                          std::chrono::steady_clock::now() - reset_timestamp_)
                          .count();
    if (reset_state_ == kResetStateRequested &&
        elapsed >= kResetRetryIntervalMilliseconds) {
      reset_state_ = kResetStateRetry;
      [FIRAnalytics resetAnalyticsData];
      return std::string();
    }
    if (elapsed < kResetTimeoutMilliseconds) return std::string();
    reset_state_ = kResetStateNone;
    instance_id_ = current_id;
    return current_id;
  }
 private:
  enum ResetState { kResetStateNone = 0,
                    kResetStateRequested,
                    kResetStateRetry };
  ResetState reset_state_;
  std::chrono::steady_clock::time_point reset_timestamp_;
  std::string instance_id_;
  static constexpr int64_t kResetRetryIntervalMilliseconds = 1000;
  static constexpr int64_t kResetTimeoutMilliseconds = 5000;
};
DEFINE_FIREBASE_VERSION_STRING(FirebaseAnalytics);
static const double kMillisecondsPerSecond = 1000.0;
static Mutex g_mutex;
static bool g_initialized = false;
static AnalyticsDataResetter* g_resetter = nullptr;
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
bool IsInitialized() {
  MutexLock lock(g_mutex);
  return g_initialized;
}
}  // namespace internal
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
void SetAnalyticsCollectionEnabled(bool enabled) {
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  [FIRAnalytics setAnalyticsCollectionEnabled:enabled];
}
void SetConsent(const std::map<ConsentType, ConsentStatus>& consent_settings) {
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  NSMutableDictionary* dict =
      [NSMutableDictionary dictionaryWithCapacity:consent_settings.size()];
  for (const auto& [type, status] : consent_settings) {
    FIRConsentType fir_type;
    switch (type) {
      case kConsentTypeAdStorage:
        fir_type = FIRConsentTypeAdStorage;
        break;
      case kConsentTypeAnalyticsStorage:
        fir_type = FIRConsentTypeAnalyticsStorage;
        break;
      case kConsentTypeAdUserData:
        fir_type = FIRConsentTypeAdUserData;
        break;
      case kConsentTypeAdPersonalization:
        fir_type = FIRConsentTypeAdPersonalization;
        break;
      default:
        LogError("Unknown ConsentType value: %d", static_cast<int>(type));
        return;
    }
    FIRConsentStatus fir_status;
    switch (status) {
      case kConsentStatusGranted:
        fir_status = FIRConsentStatusGranted;
        break;
      case kConsentStatusDenied:
        fir_status = FIRConsentStatusDenied;
        break;
      default:
        LogError("Unknown ConsentStatus value: %d", static_cast<int>(status));
        return;
    }
    [dict setObject:fir_status forKey:fir_type];
  }
  [FIRAnalytics setConsent:dict];
}
NSString* SafeString(const char* s) { return @(s ? s : ""); }
void LogEvent(const char* name, const char* parameter_name,
              const char* parameter_value) {
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
                        SafeString(parameter_name) : SafeString(parameter_value)
                      }];
}
void LogEvent(const char* name, const char* parameter_name,
              const double parameter_value) {
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
            parameters:@{
              SafeString(parameter_name) : @(parameter_value)
            }];
}
void LogEvent(const char* name, const char* parameter_name,
              const int64_t parameter_value) {
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
            parameters:@{
              SafeString(parameter_name) : @(parameter_value)
            }];
}
void LogEvent(const char* name, const char* parameter_name,
              const int parameter_value) {
  LogEvent(name, parameter_name, static_cast<int64_t>(parameter_value));
}
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
  auto future_data = internal::FutureData::GetShared();
  if (!future_data) return Future<void>();
  auto* api = future_data->api();
  const auto handle =
      api->SafeAlloc<void>(internal::kAnalyticsFnLogAppleTransaction);
  if (!transaction_id) {
    api->Complete(handle, -1, "Transaction ID is null");
    return Future<void>(api, handle.get());
  }
  std::weak_ptr<internal::FutureData> weak_fd = future_data;
  [AppleTransactionVerifier
      verifyWithTransactionId:SafeString(transaction_id)
                   completion:^(BOOL isFound, NSString* _Nullable errorMessage) {
                     MutexLock lock(g_mutex);
                     auto locked = weak_fd.lock();
                     if (!g_initialized || !locked ||
                         locked != internal::FutureData::GetShared()) {
                       return;
                     }
                     auto* cur_api = locked->api();
                     if (isFound) {
                       cur_api->Complete(handle, 0, "");
                     } else if (errorMessage != nil) {
                       cur_api->Complete(
                           handle, -1,
                           util::NSStringToString(errorMessage).c_str());
                     } else {
                       cur_api->Complete(
                           handle, -1, "StoreKit 2 transaction not found.");
                     }
                   }];
  return Future<void>(api, handle.get());
}
Future<void> LogAppleTransactionLastResult() {
  MutexLock lock(g_mutex);
  FIREBASE_ASSERT_RETURN(Future<void>(), g_initialized);
  auto future_data = internal::FutureData::GetShared();
  if (!future_data) return Future<void>();
  return static_cast<const Future<void>&>(
      future_data->api()->LastResult(internal::kAnalyticsFnLogAppleTransaction));
}
NSDictionary* MapToDictionary(const std::map<Variant, Variant>& map);
NSArray* VectorOfMapsToArray(const std::vector<Variant>& vector) {
  NSMutableArray* array = [NSMutableArray arrayWithCapacity:vector.size()];
  for (const Variant& element : vector) {
    if (element.is_map()) {
      [array addObject:MapToDictionary(element.map())];
    } else {
      LogError("VectorOfMapsToArray: Unsupported type (%s) within vector.",
               Variant::TypeName(element.type()));
    }
  }
  return array;
}
bool AddVariantToDictionary(NSMutableDictionary* dict, NSString* key,
                            const Variant& value,
                            bool allow_null_clear = false) {
  if (value.is_int64()) {
    [dict setObject:@(value.int64_value()) forKey:key];
  } else if (value.is_double()) {
    [dict setObject:@(value.double_value()) forKey:key];
  } else if (value.is_string()) {
    [dict setObject:SafeString(value.string_value()) forKey:key];
  } else if (value.is_bool()) {
    [dict setObject:@(value.bool_value() ? 1LL : 0LL) forKey:key];
  } else if (value.is_null()) {
    [dict setObject:(allow_null_clear ? (id)[NSNull null] : (id) @(0LL))
             forKey:key];
  } else if (value.is_vector()) {
    [dict setObject:VectorOfMapsToArray(value.vector()) forKey:key];
  } else if (value.is_map()) {
    [dict setObject:MapToDictionary(value.map()) forKey:key];
  } else {
    return false;
  }
  return true;
}
NSDictionary* MapToDictionary(const std::map<Variant, Variant>& map) {
  NSMutableDictionary* dict =
      [NSMutableDictionary dictionaryWithCapacity:map.size()];
  for (const auto& [k, v] : map) {
    if (!k.is_string() || !k.string_value()) continue;
    NSString* key = SafeString(k.string_value());
    if (!AddVariantToDictionary(dict, key, v)) {
      LogError("MapToDictionary: Unsupported type (%s) within map with key %s.",
               Variant::TypeName(v.type()), key.UTF8String);
    }
  }
  return dict;
}
void LogEvent(const char* name, const Parameter* parameters,
              size_t number_of_parameters) {
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  if (!name || name[0] == '\0') {
    LogError("LogEvent: Event name cannot be null or empty.");
    return;
  }
  if (!parameters && number_of_parameters > 0) {
    LogError(
        "LogEvent(%s): parameters cannot be null when number_of_parameters > 0.",
        name);
    return;
  }
  NSMutableDictionary* dict =
      [NSMutableDictionary dictionaryWithCapacity:number_of_parameters];
  for (size_t i = 0; i < number_of_parameters; ++i) {
    const Parameter& p = parameters[i];
    if (!p.name) {
      LogError("LogEvent(%s): Parameter name cannot be null.", name);
      continue;
    }
    if (!AddVariantToDictionary(dict, SafeString(p.name), p.value)) {
      LogError(
          "LogEvent(%s): %s is not a valid parameter value type. "
          "Parameter was not added.",
          p.name, Variant::TypeName(p.value.type()));
    }
  }
  [FIRAnalytics logEventWithName:SafeString(name) parameters:dict];
}
void LogEvent(const char* name, const std::vector<Parameter>& parameters) {
  LogEvent(name, parameters.data(), parameters.size());
}
void SetDefaultEventParameters(const Parameter* parameters,
                               size_t number_of_parameters) {
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  if (!parameters && number_of_parameters > 0) {
    LogError(
        "SetDefaultEventParameters: parameters cannot be null when "
        "number_of_parameters > 0.");
    return;
  }
  if (!parameters || number_of_parameters == 0) {
    [FIRAnalytics setDefaultEventParameters:nil];
    return;
  }
  NSMutableDictionary* dict =
      [NSMutableDictionary dictionaryWithCapacity:number_of_parameters];
  for (size_t i = 0; i < number_of_parameters; ++i) {
    const Parameter& p = parameters[i];
    if (!p.name) {
      LogError("SetDefaultEventParameters: Parameter name cannot be null.");
      continue;
    }
    if (!AddVariantToDictionary(dict, SafeString(p.name), p.value,
                                true)) {
      LogError(
          "SetDefaultEventParameters(%s): %s is not a valid parameter value "
          "type. The parameter was not added.",
          p.name, Variant::TypeName(p.value.type()));
    }
  }
  [FIRAnalytics setDefaultEventParameters:dict];
}
void SetDefaultEventParameters(const std::vector<Parameter>& parameters) {
  SetDefaultEventParameters(parameters.data(), parameters.size());
}
void InitiateOnDeviceConversionMeasurementWithEmailAddress(
    const char* email_address) {
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  if (!email_address) {
    LogError(
        "InitiateOnDeviceConversionMeasurementWithEmailAddress: "
        "email_address cannot be null.");
    return;
  }
  [FIRAnalytics
      initiateOnDeviceConversionMeasurementWithEmailAddress:SafeString(
                                                                email_address)];
}
void InitiateOnDeviceConversionMeasurementWithPhoneNumber(
    const char* phone_number) {
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  if (!phone_number) {
    LogError(
        "InitiateOnDeviceConversionMeasurementWithPhoneNumber: "
        "phone_number cannot be null.");
    return;
  }
  [FIRAnalytics
      initiateOnDeviceConversionMeasurementWithPhoneNumber:SafeString(
                                                               phone_number)];
}
void InitiateOnDeviceConversionMeasurementWithHashedEmailAddress(
    std::vector<unsigned char> hashed_email) {
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  [FIRAnalytics
      initiateOnDeviceConversionMeasurementWithHashedEmailAddress:
          firebase::util::BytesToNSData(hashed_email.data(),
                                        hashed_email.size())];
}
void InitiateOnDeviceConversionMeasurementWithHashedPhoneNumber(
    std::vector<unsigned char> hashed_phone) {
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  [FIRAnalytics
      initiateOnDeviceConversionMeasurementWithHashedPhoneNumber:
          firebase::util::BytesToNSData(hashed_phone.data(),
                                        hashed_phone.size())];
}
void SetUserProperty(const char* name, const char* value) {
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  if (!name || name[0] == '\0') {
    LogError("SetUserProperty: Property name cannot be null or empty.");
    return;
  }
  [FIRAnalytics setUserPropertyString:(value ? SafeString(value) : nil)
                              forName:SafeString(name)];
}
void SetUserId(const char* user_id) {
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  [FIRAnalytics setUserID:(user_id ? SafeString(user_id) : nil)];
}
void SetSessionTimeoutDuration(int64_t milliseconds) {
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  [FIRAnalytics setSessionTimeoutInterval:static_cast<NSTimeInterval>(
                                              milliseconds) /
                                          kMillisecondsPerSecond];
}
void ResetAnalyticsData() {
  MutexLock lock(g_mutex);
  FIREBASE_ASSERT_RETURN_VOID(g_initialized);
  g_resetter->Reset();
}
void SetLogCallback(const LogCallback&) {}
void NotifyAppLifecycleChange(AppLifecycleState) {}
bool IsDesktopInitialized() { return false; }
void SetDesktopDebugMode(bool) {}
Future<std::string> GetAnalyticsInstanceId() {
  MutexLock lock(g_mutex);
  FIREBASE_ASSERT_RETURN(Future<std::string>(), g_initialized);
  auto future_data = internal::FutureData::GetShared();
  if (!future_data) return Future<std::string>();
  auto* api = future_data->api();
  const auto handle =
      api->SafeAlloc<std::string>(internal::kAnalyticsFnGetAnalyticsInstanceId);
  static const int kPollTimeMs = 100;
  static const int64_t kPollTimeoutMs = 6500;
  std::weak_ptr<internal::FutureData> weak_fd = future_data;
  Thread([weak_fd, handle]() {
    @autoreleasepool {
      const auto start_time = std::chrono::steady_clock::now();
      for (;;) {
        {
          MutexLock lock(g_mutex);
          auto locked = weak_fd.lock();
          if (!g_initialized || !locked ||
              locked != internal::FutureData::GetShared()) {
            break;
          }
          std::string instance_id = g_resetter->GetInstanceId();
          if (!instance_id.empty()) {
            locked->api()->CompleteWithResult(handle, 0, "", instance_id);
            break;
          }
          auto elapsed_ms =
              std::chrono::duration_cast<std::chrono::milliseconds>(
                  std::chrono::steady_clock::now() - start_time)
                  .count();
          if (elapsed_ms >= kPollTimeoutMs) {
            locked->api()->CompleteWithResult(
                handle, -1,
                "Failed to retrieve Analytics Instance ID: timed out "
                "(Analytics collection may be disabled).",
                std::string(""));
            break;
          }
        }
        firebase::internal::Sleep(kPollTimeMs);
      }
    }
  }).Detach();
  return Future<std::string>(api, handle.get());
}
Future<std::string> GetAnalyticsInstanceIdLastResult() {
  MutexLock lock(g_mutex);
  FIREBASE_ASSERT_RETURN(Future<std::string>(), g_initialized);
  auto future_data = internal::FutureData::GetShared();
  if (!future_data) return Future<std::string>();
  return static_cast<const Future<std::string>&>(
      future_data->api()->LastResult(
          internal::kAnalyticsFnGetAnalyticsInstanceId));
}
Future<int64_t> GetSessionId() {
  MutexLock lock(g_mutex);
  FIREBASE_ASSERT_RETURN(Future<int64_t>(), g_initialized);
  auto future_data = internal::FutureData::GetShared();
  if (!future_data) return Future<int64_t>();
  auto* api = future_data->api();
  const auto handle =
      api->SafeAlloc<int64_t>(internal::kAnalyticsFnGetSessionId);
  std::weak_ptr<internal::FutureData> weak_fd = future_data;
  [FIRAnalytics
      sessionIDWithCompletion:^(int64_t session_id, NSError* _Nullable error) {
        MutexLock lock(g_mutex);
        auto locked = weak_fd.lock();
        if (!g_initialized || !locked ||
            locked != internal::FutureData::GetShared()) {
          return;
        }
        if (error) {
          locked->api()->CompleteWithResult(
              handle, -1,
              util::NSStringToString(error.localizedDescription).c_str(),
              int64_t(0));
        } else {
          locked->api()->CompleteWithResult(handle, 0, "", session_id);
        }
      }];
  return MakeFuture<int64_t>(api, handle);
}
Future<int64_t> GetSessionIdLastResult() {
  MutexLock lock(g_mutex);
  FIREBASE_ASSERT_RETURN(Future<int64_t>(), g_initialized);
  auto future_data = internal::FutureData::GetShared();
  if (!future_data) return Future<int64_t>();
  return static_cast<const Future<int64_t>&>(
      future_data->api()->LastResult(internal::kAnalyticsFnGetSessionId));
}
}  // namespace analytics
}  // namespace firebase
