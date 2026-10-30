// Copyright 2026 Google LLC
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

// See analytics_desktop_apple.h for an overview.
//
// Implementation notes:
//  * Every Apple SDK type is reached through NSClassFromString() and the
//    "shape" protocols below. No FIR* symbol is referenced at link time, so
//    this file only depends on Foundation and the Objective-C runtime.
//  * Parameter conversion mirrors analytics_ios.mm so that macOS desktop and
//    iOS log identical payloads for the same C++ calls.

#import <Foundation/Foundation.h>

#include <cstdlib>
#include <cstring>
#include <string>

#include "analytics/src/analytics_common.h"
#include "analytics/src/analytics_desktop_apple.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/include/firebase/variant.h"
#include "app/src/log.h"
#include "app/src/time.h"
#include "app/src/util.h"

#if !__has_feature(objc_arc)
#error "analytics_desktop_apple.mm must be compiled with -fobjc-arc"
#endif

// ---------------------------------------------------------------------------
// Shapes of the Firebase Apple SDK classes used by this backend. Only the
// selectors are declared here; the classes are looked up at runtime.
// ---------------------------------------------------------------------------

@protocol FIRCppDesktopOptionsShape <NSObject>
- (instancetype)initWithGoogleAppID:(NSString*)googleAppID GCMSenderID:(NSString*)GCMSenderID;
@property(nonatomic, copy) NSString* googleAppID;
@property(nonatomic, copy) NSString* GCMSenderID;
@property(nonatomic, copy) NSString* bundleID;
@property(nonatomic, copy, nullable) NSString* APIKey;
@property(nonatomic, copy, nullable) NSString* clientID;
@property(nonatomic, copy, nullable) NSString* projectID;
@property(nonatomic, copy, nullable) NSString* databaseURL;
@property(nonatomic, copy, nullable) NSString* storageBucket;
@end

@protocol FIRCppDesktopAppShape <NSObject>
@property(nonatomic, copy, readonly) id<FIRCppDesktopOptionsShape> options;
@end

@protocol FIRCppDesktopAppClassShape <NSObject>
+ (void)configureWithOptions:(id)options;
+ (nullable id<FIRCppDesktopAppShape>)defaultApp;
@end

@protocol FIRCppDesktopAnalyticsClassShape <NSObject>
+ (void)logEventWithName:(NSString*)name
              parameters:(nullable NSDictionary<NSString*, id>*)parameters;
+ (void)setUserPropertyString:(nullable NSString*)value forName:(NSString*)name;
+ (void)setUserID:(nullable NSString*)userID;
+ (void)setAnalyticsCollectionEnabled:(BOOL)analyticsCollectionEnabled;
+ (void)setSessionTimeoutInterval:(NSTimeInterval)sessionTimeoutInterval;
+ (nullable NSString*)appInstanceID;
+ (void)resetAnalyticsData;
+ (void)setDefaultEventParameters:(nullable NSDictionary<NSString*, id>*)parameters;
+ (void)sessionIDWithCompletion:(void (^)(int64_t sessionID, NSError* _Nullable error))completion;
@optional
+ (void)setConsent:(NSDictionary<NSString*, NSString*>*)consentSettings;
+ (void)initiateOnDeviceConversionMeasurementWithEmailAddress:(NSString*)emailAddress;
+ (void)initiateOnDeviceConversionMeasurementWithPhoneNumber:(NSString*)phoneNumber;
+ (void)initiateOnDeviceConversionMeasurementWithHashedEmailAddress:(NSData*)hashedEmailAddress;
+ (void)initiateOnDeviceConversionMeasurementWithHashedPhoneNumber:(NSData*)hashedPhoneNumber;
@end

namespace firebase {
namespace analytics {
namespace internal {
namespace apple {

const char kBackendModeEnvironmentVariable[] = "FIREBASE_ANALYTICS_APPLE_SDK";

namespace {

// Values of FIRConsentType / FIRConsentStatus (FIRAnalytics+Consent.h). The
// literals are used instead of the extern constants so that this file has no
// link-time dependency on FirebaseAnalytics.
NSString* const kFIRConsentTypeAdStorage = @"ad_storage";
NSString* const kFIRConsentTypeAnalyticsStorage = @"analytics_storage";
NSString* const kFIRConsentTypeAdUserData = @"ad_user_data";
NSString* const kFIRConsentTypeAdPersonalization = @"ad_personalization";
NSString* const kFIRConsentStatusGranted = @"granted";
NSString* const kFIRConsentStatusDenied = @"denied";

// NSUserDefaults key GoogleAppMeasurement uses to persist debug mode
// (APMPersistedConfigDebugModeKey). It is read when the SDK starts.
NSString* const kDebugModeUserDefaultsKey = @"/google/measurement/debug_mode";

// Bundle ID prefix of the Unity Editor application
// (com.unity3d.UnityEditor5.x).
NSString* const kUnityEditorBundleIdPrefix = @"com.unity3d.UnityEditor";

const double kMillisecondsPerSecond = 1000.0;
const int64_t kInstanceIdPollIntervalMs = 100;
const uint64_t kInstanceIdTimeoutMs = 30000;

Mutex g_mutex;  // NOLINT
bool g_active = false;
BackendMode g_mode_override = kBackendModeAuto;
// Google App ID the process-wide FIRApp was configured with by this backend.
std::string* g_configured_app_id = nullptr;

Class<FIRCppDesktopAnalyticsClassShape> AnalyticsClass() {
  return (Class<FIRCppDesktopAnalyticsClassShape>)NSClassFromString(@"FIRAnalytics");
}

Class<FIRCppDesktopAppClassShape> AppClass() {
  return (Class<FIRCppDesktopAppClassShape>)NSClassFromString(@"FIRApp");
}

Class OptionsClass() { return NSClassFromString(@"FIROptions"); }

bool IsAppleSdkEmbedded() {
  return AnalyticsClass() != nil && AppClass() != nil && OptionsClass() != nil;
}

bool IsRunningInUnityEditor() {
  NSString* bundle_id = [[NSBundle mainBundle] bundleIdentifier];
  return bundle_id != nil && [bundle_id hasPrefix:kUnityEditorBundleIdPrefix];
}

// Due to overloads of LogEvent(), it's possible for users to call variants
// that require a string with a nullptr. Mirror analytics_ios.mm and use an
// empty string instead.
NSString* SafeString(const char* value) { return @(value ? value : ""); }

std::string StdString(NSString* value) {
  return value ? std::string([value UTF8String]) : std::string();
}

// Variant conversion, mirrors analytics_ios.mm.
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

bool AddVariantToDictionary(NSMutableDictionary* dict, NSString* key, const Variant& value) {
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
    // Just use integer 0 for null.
    [dict setObject:[NSNumber numberWithLongLong:0] forKey:key];
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
  NSMutableDictionary* dict = [NSMutableDictionary dictionaryWithCapacity:map.size()];
  for (const auto& pair : map) {
    // Only add elements that use a string key.
    if (!pair.first.is_string()) continue;
    NSString* key = SafeString(pair.first.string_value());
    if (!AddVariantToDictionary(dict, key, pair.second)) {
      LogError("MapToDictionary: Unsupported type (%s) within map with key %s.",
               Variant::TypeName(pair.second.type()), [key UTF8String]);
    }
  }
  return dict;
}

NSDictionary* ParametersToDictionary(const char* context, const Parameter* parameters,
                                     size_t number_of_parameters) {
  NSMutableDictionary* dict = [NSMutableDictionary dictionaryWithCapacity:number_of_parameters];
  for (size_t i = 0; i < number_of_parameters; ++i) {
    const Parameter& parameter = parameters[i];
    if (!AddVariantToDictionary(dict, SafeString(parameter.name), parameter.value)) {
      LogError("%s(%s): %s is not a valid parameter value type. The parameter was not added.",
               context, parameter.name ? parameter.name : "",
               Variant::TypeName(parameter.value.type()));
    }
  }
  return dict;
}

NSData* BytesToData(const std::vector<unsigned char>& bytes) {
  return [NSData dataWithBytes:bytes.data() length:bytes.size()];
}

// Works around the instance ID lagging behind a reset on Apple platforms
// (b/143656277, b/110166640). Mirrors AnalyticsDataResetter in
// analytics_ios.mm. All methods must be called with g_mutex held.
class AnalyticsDataResetter {
 public:
  void Reset() {
    reset_timestamp_ = firebase::internal::GetTimestampEpoch();
    reset_state_ = kResetStateRequested;
    instance_id_ = StdString([AnalyticsClass() appInstanceID]);
    [AnalyticsClass() resetAnalyticsData];
  }

  // Returns a non-empty instance ID if valid, or an empty string while a
  // reset is still in progress.
  std::string GetInstanceId() {
    std::string current_instance_id = StdString([AnalyticsClass() appInstanceID]);
    uint64_t elapsed_ms = firebase::internal::GetTimestampEpoch() - reset_timestamp_;
    switch (reset_state_) {
      case kResetStateNone:
        break;
      case kResetStateRequested:
        if (elapsed_ms >= kResetRetryIntervalMilliseconds) {
          // The Apple SDK can take a while to initialize; reset again if the
          // instance ID hasn't changed for a while.
          reset_state_ = kResetStateRetry;
          reset_timestamp_ = firebase::internal::GetTimestampEpoch();
          [AnalyticsClass() resetAnalyticsData];
          return std::string();
        }
        FIREBASE_CASE_FALLTHROUGH;
      case kResetStateRetry:
        if ((current_instance_id.empty() || current_instance_id == instance_id_) &&
            elapsed_ms < kResetTimeoutMilliseconds) {
          return std::string();
        }
        break;
    }
    reset_state_ = kResetStateNone;
    instance_id_ = current_instance_id;
    return current_instance_id;
  }

 private:
  enum ResetState { kResetStateNone = 0, kResetStateRequested, kResetStateRetry };
  static constexpr uint64_t kResetRetryIntervalMilliseconds = 1000;
  static constexpr uint64_t kResetTimeoutMilliseconds = 5000;

  ResetState reset_state_ = kResetStateNone;
  uint64_t reset_timestamp_ = 0;
  std::string instance_id_;
};

AnalyticsDataResetter* g_resetter = nullptr;

BackendMode ResolveMode() {
  if (g_mode_override != kBackendModeAuto) return g_mode_override;
  return ParseBackendMode(std::getenv(kBackendModeEnvironmentVariable));
}

void WriteDebugModeFlag(bool enabled) {
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  if (enabled) {
    [defaults setBool:YES forKey:kDebugModeUserDefaultsKey];
  } else {
    [defaults removeObjectForKey:kDebugModeUserDefaultsKey];
  }
}

id<FIRCppDesktopOptionsShape> CreateOptions(const AppOptions& app_options) {
  id<FIRCppDesktopOptionsShape> options =
      [[OptionsClass() alloc] initWithGoogleAppID:SafeString(app_options.app_id())
                                      GCMSenderID:SafeString(app_options.messaging_sender_id())];
  if (std::strlen(app_options.api_key())) options.APIKey = @(app_options.api_key());
  if (std::strlen(app_options.project_id())) options.projectID = @(app_options.project_id());
  if (std::strlen(app_options.database_url())) options.databaseURL = @(app_options.database_url());
  if (std::strlen(app_options.storage_bucket())) {
    options.storageBucket = @(app_options.storage_bucket());
  }
  if (std::strlen(app_options.client_id())) options.clientID = @(app_options.client_id());
  // Mirrors app_ios.mm. Note that only FirebaseCore reads FIROptions.bundleID
  // (to log I-COR000008 when it differs from the main bundle);
  // GoogleAppMeasurement always reports [NSBundle mainBundle].bundleIdentifier,
  // so inside the Unity Editor events carry the Editor's bundle ID regardless.
  if (std::strlen(app_options.package_name())) {
    options.bundleID = @(app_options.package_name());
  } else if ([[NSBundle mainBundle] bundleIdentifier]) {
    options.bundleID = [[NSBundle mainBundle] bundleIdentifier];
  }
  return options;
}

// Configures the process-wide default FIRApp if nobody has yet. Returns false
// if the Apple SDK could not be configured. Must be called with g_mutex held.
bool EnsureDefaultAppConfigured(const AppOptions& app_options) {
  id<FIRCppDesktopAppShape> default_app = [AppClass() defaultApp];
  if (default_app) {
    std::string existing_app_id = StdString(default_app.options.googleAppID);
    if (g_configured_app_id == nullptr) {
      // The host configured FIRApp itself (e.g. a native macOS app that also
      // uses the Firebase Apple SDK directly). Reuse it.
      LogInfo("Analytics: Using the FIRApp already configured by the host (App ID %s).",
              existing_app_id.c_str());
    } else if (existing_app_id != app_options.app_id()) {
      // The Apple SDK cannot be re-configured within a process. In the Unity
      // Editor this happens when switching Firebase projects without
      // restarting the Editor.
      LogWarning("Analytics: The Firebase Apple SDK is already configured with App ID %s in "
                 "this process; ignoring App ID %s. Restart the process (or the Unity Editor) "
                 "to switch projects.",
                 existing_app_id.c_str(), app_options.app_id());
    }
    return true;
  }

  if (!IsAppleGoogleAppId(app_options.app_id())) {
    LogWarning("Analytics: App ID '%s' is not an Apple platform App ID (1:<sender>:ios:<id>). "
               "Register an Apple app in the Firebase console and configure the desktop app "
               "with it (e.g. from GoogleService-Info.plist). Falling back to the Analytics "
               "desktop stubs.",
               app_options.app_id());
    return false;
  }

  @try {
    [AppClass() configureWithOptions:CreateOptions(app_options)];
  } @catch (NSException* exception) {
    LogError("Analytics: Failed to configure the Firebase Apple SDK: %s",
             [[exception reason] UTF8String]);
    return false;
  }
  if (g_configured_app_id == nullptr) g_configured_app_id = new std::string();
  *g_configured_app_id = app_options.app_id();
  return true;
}

void PollInstanceId(ReferenceCountedFutureImpl* api, SafeFutureHandle<std::string> handle,
                    uint64_t start_ms) {
  dispatch_after(
      dispatch_time(DISPATCH_TIME_NOW, kInstanceIdPollIntervalMs * NSEC_PER_MSEC),
      dispatch_get_global_queue(QOS_CLASS_UTILITY, 0), ^{
        MutexLock lock(g_mutex);
        // Terminate() deactivates the backend (under g_mutex) before the
        // future API is destroyed, so `api` is valid while g_active is set.
        if (!g_active) return;
        std::string instance_id = g_resetter->GetInstanceId();
        if (!instance_id.empty()) {
          api->CompleteWithResult(handle, 0, "", instance_id);
        } else if (firebase::internal::GetTimestampEpoch() - start_ms >= kInstanceIdTimeoutMs) {
          api->CompleteWithResult(handle, -1, "Timed out waiting for the app instance ID.",
                                  std::string());
        } else {
          PollInstanceId(api, handle, start_ms);
        }
      });
}

}  // namespace

BackendMode ParseBackendMode(const char* value) {
  if (value == nullptr || value[0] == '\0') return kBackendModeAuto;
  NSString* mode = [@(value) lowercaseString];
  if ([mode isEqualToString:@"debug"]) return kBackendModeDebug;
  for (NSString* off in @[ @"disabled", @"disable", @"0", @"off", @"false", @"no" ]) {
    if ([mode isEqualToString:off]) return kBackendModeDisabled;
  }
  for (NSString* on in @[ @"enabled", @"enable", @"1", @"on", @"true", @"yes" ]) {
    if ([mode isEqualToString:on]) return kBackendModeEnabled;
  }
  LogWarning("Analytics: Ignoring unrecognized %s value '%s'.", kBackendModeEnvironmentVariable,
             value);
  return kBackendModeAuto;
}

bool IsAppleGoogleAppId(const char* app_id) {
  return app_id != nullptr && std::strstr(app_id, ":ios:") != nullptr;
}

void SetBackendMode(BackendMode mode) {
  MutexLock lock(g_mutex);
  g_mode_override = mode;
}

bool Initialize(const App& app, bool collection_enabled, bool collection_enabled_set) {
  MutexLock lock(g_mutex);
  if (g_active) return true;

  if (!IsAppleSdkEmbedded()) {
    LogDebug("Analytics: The Firebase Apple SDK is not linked into this binary; using the "
             "Analytics desktop stubs.");
    return false;
  }

  @autoreleasepool {
    const bool in_unity_editor = IsRunningInUnityEditor();
    const BackendMode requested_mode = ResolveMode();
    // On by default everywhere, including the Unity Editor. Debug mode is the
    // app developer's choice (SetDesktopDebugMode() or
    // FIREBASE_ANALYTICS_APPLE_SDK=debug); the SDK doesn't force it.
    const BackendMode mode =
        requested_mode == kBackendModeAuto ? kBackendModeEnabled : requested_mode;
    if (mode == kBackendModeDisabled) return false;

    // The debug-mode flag must be written before FIRApp is configured because
    // GoogleAppMeasurement reads it at startup. Only explicit modes touch it;
    // otherwise the value persisted by SetDesktopDebugMode() is respected.
    const bool explicit_mode = requested_mode != kBackendModeAuto;
    const bool debug_mode = mode == kBackendModeDebug;
    if (debug_mode) {
      WriteDebugModeFlag(true);
    } else if (explicit_mode) {
      WriteDebugModeFlag(false);
    }
    if (in_unity_editor) {
      LogInfo("Analytics: Running in the Unity Editor. GoogleAppMeasurement reports the Editor's "
              "bundle ID and app version (from the Editor's Info.plist), and its local storage "
              "is shared by every Unity project on this machine. Set %s=disabled to turn "
              "Analytics off in the Editor.",
              kBackendModeEnvironmentVariable);
    }

    if (!EnsureDefaultAppConfigured(app.options())) return false;

    if (collection_enabled_set) {
      [AnalyticsClass() setAnalyticsCollectionEnabled:collection_enabled];
    }
    if (g_resetter == nullptr) g_resetter = new AnalyticsDataResetter();
    g_active = true;
    LogInfo("Analytics: Using the Firebase Apple SDK%s.", debug_mode ? " (debug mode)" : "");
  }
  return true;
}

void Terminate() {
  MutexLock lock(g_mutex);
  // FIRApp / FIRAnalytics stay configured on purpose (see header).
  g_active = false;
  delete g_resetter;
  g_resetter = nullptr;
}

bool IsActive() {
  MutexLock lock(g_mutex);
  return g_active;
}

void LogEvent(const char* name, const Parameter* parameters, size_t number_of_parameters) {
  @autoreleasepool {
    NSDictionary* dict = ParametersToDictionary("LogEvent", parameters, number_of_parameters);
    [AnalyticsClass() logEventWithName:SafeString(name) parameters:dict];
  }
}

void SetUserProperty(const char* name, const char* value) {
  @autoreleasepool {
    [AnalyticsClass() setUserPropertyString:(value ? @(value) : nil) forName:SafeString(name)];
  }
}

void SetUserId(const char* user_id) {
  @autoreleasepool {
    [AnalyticsClass() setUserID:(user_id ? @(user_id) : nil)];
  }
}

void SetAnalyticsCollectionEnabled(bool enabled) {
  [AnalyticsClass() setAnalyticsCollectionEnabled:enabled];
}

void SetConsent(const std::map<ConsentType, ConsentStatus>& consent_settings) {
  Class<FIRCppDesktopAnalyticsClassShape> analytics = AnalyticsClass();
  if (![(id)analytics respondsToSelector:@selector(setConsent:)]) {
    LogWarning("Analytics: SetConsent() is not supported by the embedded Firebase Apple SDK.");
    return;
  }
  @autoreleasepool {
    NSMutableDictionary* dict =
        [NSMutableDictionary dictionaryWithCapacity:consent_settings.size()];
    for (const auto& setting : consent_settings) {
      NSString* type = nil;
      switch (setting.first) {
        case kConsentTypeAdStorage:
          type = kFIRConsentTypeAdStorage;
          break;
        case kConsentTypeAnalyticsStorage:
          type = kFIRConsentTypeAnalyticsStorage;
          break;
        case kConsentTypeAdUserData:
          type = kFIRConsentTypeAdUserData;
          break;
        case kConsentTypeAdPersonalization:
          type = kFIRConsentTypeAdPersonalization;
          break;
        default:
          LogError("Unknown ConsentType value: %d", setting.first);
          return;
      }
      NSString* status = nil;
      switch (setting.second) {
        case kConsentStatusGranted:
          status = kFIRConsentStatusGranted;
          break;
        case kConsentStatusDenied:
          status = kFIRConsentStatusDenied;
          break;
        default:
          LogError("Unknown ConsentStatus value: %d", setting.second);
          return;
      }
      [dict setObject:status forKey:type];
    }
    [analytics setConsent:dict];
  }
}

void SetDefaultEventParameters(const Parameter* parameters, size_t number_of_parameters) {
  @autoreleasepool {
    if (parameters == nullptr || number_of_parameters == 0) {
      [AnalyticsClass() setDefaultEventParameters:nil];
      return;
    }
    [AnalyticsClass()
        setDefaultEventParameters:ParametersToDictionary("SetDefaultEventParameters", parameters,
                                                         number_of_parameters)];
  }
}

void SetSessionTimeoutDuration(int64_t milliseconds) {
  [AnalyticsClass()
      setSessionTimeoutInterval:static_cast<NSTimeInterval>(milliseconds) / kMillisecondsPerSecond];
}

void ResetAnalyticsData() {
  MutexLock lock(g_mutex);
  if (!g_active) return;
  g_resetter->Reset();
}

// On-device conversion measurement is only available on some Apple platforms
// and requires GoogleAdsOnDeviceConversion; it's a logged no-op otherwise.
#define FIREBASE_ANALYTICS_APPLE_ODM(selector_name, argument)                                   \
  do {                                                                                          \
    Class<FIRCppDesktopAnalyticsClassShape> analytics = AnalyticsClass();                       \
    if ([(id)analytics respondsToSelector:@selector(selector_name:)]) {                         \
      [analytics selector_name:argument];                                                       \
    } else {                                                                                    \
      LogWarning("Analytics: %s is not supported on macOS and has no effect.", #selector_name); \
    }                                                                                           \
  } while (0)

void InitiateOnDeviceConversionMeasurementWithEmailAddress(const char* email_address) {
  @autoreleasepool {
    FIREBASE_ANALYTICS_APPLE_ODM(initiateOnDeviceConversionMeasurementWithEmailAddress,
                                 SafeString(email_address));
  }
}

void InitiateOnDeviceConversionMeasurementWithPhoneNumber(const char* phone_number) {
  @autoreleasepool {
    FIREBASE_ANALYTICS_APPLE_ODM(initiateOnDeviceConversionMeasurementWithPhoneNumber,
                                 SafeString(phone_number));
  }
}

void InitiateOnDeviceConversionMeasurementWithHashedEmailAddress(
    const std::vector<unsigned char>& hashed_email_address) {
  @autoreleasepool {
    FIREBASE_ANALYTICS_APPLE_ODM(initiateOnDeviceConversionMeasurementWithHashedEmailAddress,
                                 BytesToData(hashed_email_address));
  }
}

void InitiateOnDeviceConversionMeasurementWithHashedPhoneNumber(
    const std::vector<unsigned char>& hashed_phone_number) {
  @autoreleasepool {
    FIREBASE_ANALYTICS_APPLE_ODM(initiateOnDeviceConversionMeasurementWithHashedPhoneNumber,
                                 BytesToData(hashed_phone_number));
  }
}

#undef FIREBASE_ANALYTICS_APPLE_ODM

void SetDebugMode(bool enabled) {
  if (!IsAppleSdkEmbedded()) return;
  WriteDebugModeFlag(enabled);
  LogInfo("Analytics: Debug mode %s; this takes effect the next time the app starts.",
          enabled ? "enabled" : "disabled");
}

void GetAnalyticsInstanceId(ReferenceCountedFutureImpl* api,
                            const SafeFutureHandle<std::string>& handle) {
  MutexLock lock(g_mutex);
  if (!g_active) {
    api->CompleteWithResult(handle, -1, "Analytics is not active.", std::string());
    return;
  }
  std::string instance_id = g_resetter->GetInstanceId();
  if (!instance_id.empty()) {
    api->CompleteWithResult(handle, 0, "", instance_id);
    return;
  }
  PollInstanceId(api, handle, firebase::internal::GetTimestampEpoch());
}

void GetSessionId(ReferenceCountedFutureImpl* api, const SafeFutureHandle<int64_t>& handle) {
  SafeFutureHandle<int64_t> handle_copy = handle;
  [AnalyticsClass() sessionIDWithCompletion:^(int64_t session_id, NSError* _Nullable error) {
    MutexLock lock(g_mutex);
    if (!g_active) return;
    if (error) {
      api->CompleteWithResult(handle_copy, -1, [[error localizedDescription] UTF8String],
                              static_cast<int64_t>(0));
    } else {
      api->CompleteWithResult(handle_copy, 0, "", session_id);
    }
  }];
}

}  // namespace apple
}  // namespace internal
}  // namespace analytics
}  // namespace firebase
