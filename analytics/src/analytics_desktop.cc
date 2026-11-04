#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include "analytics/src/analytics_common.h"
#include "analytics/src/analytics_desktop_dynamic.h"
#include "analytics/src/include/firebase/analytics.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/variant.h"
#include "app/src/log.h"
#include "firebase/log.h"
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "analytics_windows.h"
#endif
namespace firebase {
namespace analytics {
#if defined(_WIN32)
#define ANALYTICS_DLL_FILENAME L"google_analytics.dll"
static HMODULE g_analytics_module = 0;
#endif
static bool g_initialized = false;
static int g_fake_instance_id = 0;
static bool g_analytics_collection_enabled = true;
namespace {
LogCallback& GetLogCallback() {
  static LogCallback s_log_callback;
  return s_log_callback;
}
std::mutex& GetLogCallbackMutex() {
  static std::mutex s_log_callback_mutex;
  return s_log_callback_mutex;
}
#if defined(_WIN32)
std::string& GetAppId() {
  static std::string s_app_id;
  return s_app_id;
}
std::string& GetPackageName() {
  static std::string s_package_name;
  return s_package_name;
}
#endif
}  // namespace
LogLevel ConvertAnalyticsLogLevelToFirebaseLogLevel(
    GoogleAnalytics_LogLevel log_level) {
  switch (log_level) {
    case GoogleAnalytics_LogLevel_kDebug:
      return kLogLevelDebug;
    case GoogleAnalytics_LogLevel_kInfo:
      return kLogLevelInfo;
    case GoogleAnalytics_LogLevel_kWarning:
      return kLogLevelWarning;
    case GoogleAnalytics_LogLevel_kError:
      return kLogLevelError;
    default:
      return kLogLevelInfo;
  }
}
void GoogleAnalyticsWrapperLogCallback(int32_t log_level, const char* message) {
  LogCallback callback_to_call;
  {
    std::lock_guard lock(GetLogCallbackMutex());
    callback_to_call = GetLogCallback();
  }
  if (callback_to_call) {
    callback_to_call(ConvertAnalyticsLogLevelToFirebaseLogLevel(
                         static_cast<GoogleAnalytics_LogLevel>(log_level)),
                     message ? message : "");
  }
}
void Initialize(const App& app) {
  {
    std::lock_guard lock(internal::FutureData::mutex());
    if (g_initialized) {
      LogWarning("Analytics already initialized.");
      return;
    }
    g_initialized = true;
    internal::FutureData::Create();
    g_fake_instance_id = 0;
    bool has_log_callback = false;
    {
      std::lock_guard callback_lock(GetLogCallbackMutex());
      has_log_callback = static_cast<bool>(GetLogCallback());
    }
#if defined(_WIN32)
    if (!g_analytics_module) {
      std::vector<std::string> allowed_hashes(
          FirebaseAnalytics_KnownWindowsDllHashes,
          FirebaseAnalytics_KnownWindowsDllHashes +
              FirebaseAnalytics_KnownWindowsDllHashCount);
      g_analytics_module =
          firebase::analytics::internal::VerifyAndLoadAnalyticsLibrary(
              ANALYTICS_DLL_FILENAME, allowed_hashes);
      if (g_analytics_module) {
        int num_loaded =
            FirebaseAnalytics_LoadDynamicFunctions(g_analytics_module);
        if (num_loaded < FirebaseAnalytics_DynamicFunctionCount) {
          LogWarning(
              "Analytics: Failed to load functions from Google Analytics "
              "module (%d out of %d loaded), reverting to stubs.",
              num_loaded, FirebaseAnalytics_DynamicFunctionCount);
          FirebaseAnalytics_UnloadDynamicFunctions();
          FreeLibrary(g_analytics_module);
          g_analytics_module = 0;
        } else {
          LogInfo("Analytics: Loaded Google Analytics module.");
          GoogleAnalytics_SetLogCallback(GoogleAnalyticsWrapperLogCallback);
          std::string& app_id = GetAppId();
          std::string& package_name = GetPackageName();
          app_id = app.options().app_id();
          package_name = app.options().package_name();
          GoogleAnalytics_Options* c_options = GoogleAnalytics_Options_Create();
          if (!c_options) {
            LogError("Analytics: Failed to create GoogleAnalytics_Options.");
          } else {
            c_options->app_id = app_id.c_str();
            c_options->package_name = package_name.c_str();
            c_options->analytics_collection_enabled_at_first_launch =
                g_analytics_collection_enabled;
            LogInfo(
                "Analytics: Initializing Google Analytics C API with App ID: "
                "%s, Package Name: %s",
                c_options->app_id, c_options->package_name);
            if (!GoogleAnalytics_Initialize(c_options)) {
              LogError(
                  "Analytics: Failed to initialize Google Analytics C API.");
            } else {
              LogInfo(
                  "Analytics: Google Analytics C API initialized "
                  "successfully.");
            }
          }
        }
      } else {
        g_analytics_module = 0;
      }
    }
#endif
    if (has_log_callback) {
      GoogleAnalytics_SetLogCallback(GoogleAnalyticsWrapperLogCallback);
    }
  }
  internal::RegisterTerminateOnAppDestroy(&app);
}
namespace internal {
bool IsInitialized() {
  std::lock_guard lock(FutureData::mutex());
  return g_initialized;
}
}  // namespace internal
void Terminate() {
  {
    std::lock_guard lock(internal::FutureData::mutex());
    if (!g_initialized) return;
#if defined(_WIN32)
    if (g_analytics_module) {
      NotifyAppLifecycleChange(AppLifecycleState::kTermination);
      GoogleAnalytics_SetLogCallback(nullptr);
      FirebaseAnalytics_UnloadDynamicFunctions();
      FreeLibrary(g_analytics_module);
      g_analytics_module = 0;
    }
#endif
    GoogleAnalytics_SetLogCallback(nullptr);
    {
      std::lock_guard callback_lock(GetLogCallbackMutex());
      GetLogCallback() = nullptr;
    }
    internal::FutureData::Destroy();
    g_initialized = false;
  }
  internal::UnregisterTerminateOnAppDestroy();
}
static bool InsertVariantIntoGAItem(GoogleAnalytics_Item* c_item,
                                    const char* key,
                                    const firebase::Variant& value,
                                    const char* param_name,
                                    const char* container_kind) {
  if (value.is_int64()) {
    GoogleAnalytics_Item_InsertInt(c_item, key, value.int64_value());
    return true;
  }
  if (value.is_bool()) {
    GoogleAnalytics_Item_InsertInt(c_item, key, value.bool_value() ? 1 : 0);
    return true;
  }
  if (value.is_double()) {
    GoogleAnalytics_Item_InsertDouble(c_item, key, value.double_value());
    return true;
  }
  if (value.is_string()) {
    GoogleAnalytics_Item_InsertString(c_item, key, value.string_value());
    return true;
  }
  LogWarning(
      "Analytics: Value for key '%s' in %s parameter '%s' has an unsupported "
      "Variant type. Skipping.",
      key, container_kind, param_name);
  return false;
}
static void ConvertParametersToGAParams(
    const Parameter* parameters, size_t number_of_parameters,
    GoogleAnalytics_EventParameters* c_event_params) {
  if (!parameters || number_of_parameters == 0 || !c_event_params) return;
  for (size_t i = 0; i < number_of_parameters; ++i) {
    const Parameter& param = parameters[i];
    if (!param.name || param.name[0] == '\0') {
      LogError("Analytics: Parameter name cannot be null or empty.");
      continue;
    }
    if (param.value.is_int64()) {
      GoogleAnalytics_EventParameters_InsertInt(c_event_params, param.name,
                                                param.value.int64_value());
    } else if (param.value.is_bool()) {
      GoogleAnalytics_EventParameters_InsertInt(
          c_event_params, param.name, param.value.bool_value() ? 1 : 0);
    } else if (param.value.is_double()) {
      GoogleAnalytics_EventParameters_InsertDouble(c_event_params, param.name,
                                                   param.value.double_value());
    } else if (param.value.is_string()) {
      GoogleAnalytics_EventParameters_InsertString(c_event_params, param.name,
                                                   param.value.string_value());
    } else if (param.value.is_vector()) {
      const auto& item_vector = param.value.vector();
      if (item_vector.empty()) {
        LogWarning("Analytics: Parameter '%s' is an empty vector. Skipping.",
                   param.name);
        continue;
      }
      GoogleAnalytics_ItemVector* c_item_vector =
          GoogleAnalytics_ItemVector_Create();
      if (!c_item_vector) {
        LogError(
            "Analytics: Failed to create ItemVector for vector parameter '%s'.",
            param.name);
        continue;
      }
      bool item_vector_populated = false;
      for (const firebase::Variant& element : item_vector) {
        if (!element.is_map()) {
          LogWarning(
              "Analytics: Element in vector parameter '%s' is not a map. "
              "Skipping element.",
              param.name);
          continue;
        }
        const auto& item_map = element.map();
        if (item_map.empty()) continue;
        GoogleAnalytics_Item* c_item = GoogleAnalytics_Item_Create();
        if (!c_item) {
          LogError(
              "Analytics: Failed to create Item for element in vector "
              "parameter '%s'.",
              param.name);
          continue;
        }
        bool item_populated = false;
        for (const auto& [key_variant, value_variant] : item_map) {
          if (!key_variant.is_string()) {
            LogError("Analytics: Non-string map key found. Skipping.");
            continue;
          }
          const char* key_from_map = key_variant.string_value();
          if (!key_from_map || key_from_map[0] == '\0') {
            LogError("Analytics: Map key cannot be null or empty. Skipping.");
            continue;
          }
          if (InsertVariantIntoGAItem(c_item, key_from_map, value_variant,
                                      param.name, "vector item")) {
            item_populated = true;
          }
        }
        if (item_populated) {
          GoogleAnalytics_ItemVector_InsertItem(c_item_vector, c_item);
          item_vector_populated = true;
        } else {
          GoogleAnalytics_Item_Destroy(c_item);
        }
      }
      if (item_vector_populated) {
        GoogleAnalytics_EventParameters_InsertItemVector(
            c_event_params, param.name, c_item_vector);
      } else {
        GoogleAnalytics_ItemVector_Destroy(c_item_vector);
        LogWarning(
            "Analytics: Vector parameter '%s' resulted in an empty ItemVector; "
            "skipping.",
            param.name);
      }
    } else if (param.value.is_map()) {
      const auto& user_map = param.value.map();
      if (user_map.empty()) {
        LogWarning("Analytics: Parameter '%s' is an empty map. Skipping.",
                   param.name);
        continue;
      }
      GoogleAnalytics_ItemVector* c_item_vector =
          GoogleAnalytics_ItemVector_Create();
      if (!c_item_vector) {
        LogError(
            "Analytics: Failed to create ItemVector for map parameter '%s'.",
            param.name);
        continue;
      }
      bool item_vector_populated = false;
      for (const auto& [key_variant, value_variant] : user_map) {
        if (!key_variant.is_string()) {
          LogError("Analytics: Non-string map key found. Skipping.");
          continue;
        }
        const char* key_from_map = key_variant.string_value();
        if (!key_from_map || key_from_map[0] == '\0') {
          LogError("Analytics: Map key cannot be null or empty. Skipping.");
          continue;
        }
        GoogleAnalytics_Item* c_item = GoogleAnalytics_Item_Create();
        if (!c_item) {
          LogError(
              "Analytics: Failed to create Item for key '%s' in map parameter "
              "'%s'.",
              key_from_map, param.name);
          continue;
        }
        if (InsertVariantIntoGAItem(c_item, key_from_map, value_variant,
                                    param.name, "map")) {
          GoogleAnalytics_ItemVector_InsertItem(c_item_vector, c_item);
          item_vector_populated = true;
        } else {
          GoogleAnalytics_Item_Destroy(c_item);
        }
      }
      if (item_vector_populated) {
        GoogleAnalytics_EventParameters_InsertItemVector(
            c_event_params, param.name, c_item_vector);
      } else {
        GoogleAnalytics_ItemVector_Destroy(c_item_vector);
        LogWarning(
            "Analytics: Map parameter '%s' resulted in an empty ItemVector; no "
            "valid key-value pairs found or all values had unsupported types. "
            "This map parameter was skipped.",
            param.name);
      }
    } else {
      LogWarning("Analytics: Unsupported variant type for parameter '%s'.",
                 param.name);
    }
  }
}
void LogEvent(const char* name, const Parameter* parameters,
              size_t number_of_parameters) {
  std::lock_guard lock(internal::FutureData::mutex());
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  if (!name || name[0] == '\0') {
    LogError("Analytics: Event name cannot be null or empty.");
    return;
  }
  if (!parameters && number_of_parameters > 0) {
    LogError(
        "Analytics: Event parameters cannot be null when "
        "number_of_parameters > 0.");
    return;
  }
  GoogleAnalytics_EventParameters* c_event_params = nullptr;
  if (parameters && number_of_parameters > 0) {
    c_event_params = GoogleAnalytics_EventParameters_Create();
    if (!c_event_params) {
      LogError(
          "Analytics: Failed to create event parameters map for event '%s'.",
          name);
      return;
    }
    ConvertParametersToGAParams(parameters, number_of_parameters,
                                c_event_params);
  }
  GoogleAnalytics_LogEvent(name, c_event_params);
}
void LogEvent(const char* name, const std::vector<Parameter>& parameters) {
  LogEvent(name, parameters.data(), parameters.size());
}
void SetUserProperty(const char* name, const char* property) {
  std::lock_guard lock(internal::FutureData::mutex());
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  if (!name || name[0] == '\0') {
    LogError("Analytics: User property name cannot be null or empty.");
    return;
  }
  GoogleAnalytics_SetUserProperty(name, property);
}
void SetUserId(const char* user_id) {
  std::lock_guard lock(internal::FutureData::mutex());
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  GoogleAnalytics_SetUserId(user_id);
}
void SetAnalyticsCollectionEnabled(bool enabled) {
  std::lock_guard lock(internal::FutureData::mutex());
  g_analytics_collection_enabled = enabled;
  if (g_initialized) {
    GoogleAnalytics_SetAnalyticsCollectionEnabled(enabled);
  }
}
void ResetAnalyticsData() {
  std::lock_guard lock(internal::FutureData::mutex());
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  GoogleAnalytics_ResetAnalyticsData();
  g_fake_instance_id++;
}
void SetLogCallback(const LogCallback& callback) {
  std::lock_guard lock(internal::FutureData::mutex());
  {
    std::lock_guard callback_lock(GetLogCallbackMutex());
    GetLogCallback() = callback;
  }
  if (g_initialized) {
    GoogleAnalytics_SetLogCallback(callback ? GoogleAnalyticsWrapperLogCallback
                                            : nullptr);
  }
}
void NotifyAppLifecycleChange(AppLifecycleState state) {
  std::lock_guard lock(internal::FutureData::mutex());
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  GoogleAnalytics_NotifyAppLifecycleChange(
      static_cast<GoogleAnalytics_AppLifecycleState>(state));
}
bool IsDesktopInitialized() {
  std::lock_guard lock(internal::FutureData::mutex());
  FIREBASE_ASSERT_RETURN(false, internal::IsInitialized());
  return GoogleAnalytics_IsInitialized();
}
void SetDesktopDebugMode(bool enabled) {
  std::lock_guard lock(internal::FutureData::mutex());
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  GoogleAnalytics_SetDebugMode(enabled);
}
void LogEvent(const char* name) {
  LogEvent(name, static_cast<const Parameter*>(nullptr), 0);
}
Future<void> LogAppleTransaction(const char* transaction_id) {
  (void)transaction_id;
  std::lock_guard lock(internal::FutureData::mutex());
  FIREBASE_ASSERT_RETURN(Future<void>(), internal::IsInitialized());
  auto future_data = internal::FutureData::GetShared();
  if (!future_data) return Future<void>();
  auto* api = future_data->api();
  const auto handle =
      api->SafeAlloc<void>(internal::kAnalyticsFnLogAppleTransaction);
  api->Complete(handle, kErrorNone, "");
  return Future<void>(api, handle.get());
}
Future<void> LogAppleTransactionLastResult() {
  std::lock_guard lock(internal::FutureData::mutex());
  FIREBASE_ASSERT_RETURN(Future<void>(), internal::IsInitialized());
  auto future_data = internal::FutureData::GetShared();
  if (!future_data) return Future<void>();
  return static_cast<const Future<void>&>(
      future_data->api()->LastResult(internal::kAnalyticsFnLogAppleTransaction));
}
template <typename T>
static void LogSingleParamEvent(const char* name, const char* parameter_name,
                                T parameter_value) {
  if (!parameter_name) {
    LogError("Analytics: Parameter name cannot be null.");
    return;
  }
  Parameter param(parameter_name, parameter_value);
  LogEvent(name, &param, 1);
}
void LogEvent(const char* name, const char* parameter_name,
              const char* parameter_value) {
  LogSingleParamEvent(name, parameter_name, parameter_value);
}
void LogEvent(const char* name, const char* parameter_name,
              const double parameter_value) {
  LogSingleParamEvent(name, parameter_name, parameter_value);
}
void LogEvent(const char* name, const char* parameter_name,
              const int64_t parameter_value) {
  LogSingleParamEvent(name, parameter_name, parameter_value);
}
void LogEvent(const char* name, const char* parameter_name,
              const int parameter_value) {
  LogSingleParamEvent(name, parameter_name,
                      static_cast<int64_t>(parameter_value));
}
static void WarnDesktopUnsupported(const char* fn_name) {
  (void)fn_name;
#if defined(_WIN32)
  if (g_analytics_module) {
    LogWarning("Analytics: %s is not supported and has no effect on Desktop.",
               fn_name);
  }
#endif
}
void SetDefaultEventParameters(const Parameter* parameters,
                               size_t number_of_parameters) {
  std::lock_guard lock(internal::FutureData::mutex());
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  if (!parameters && number_of_parameters > 0) {
    LogError(
        "Analytics: Default event parameters cannot be null when "
        "number_of_parameters > 0.");
    return;
  }
  WarnDesktopUnsupported("SetDefaultEventParameters()");
}
void SetDefaultEventParameters(const std::vector<Parameter>& parameters) {
  SetDefaultEventParameters(parameters.data(), parameters.size());
}
void SetConsent(const std::map<ConsentType, ConsentStatus>& consent_settings) {
  std::lock_guard lock(internal::FutureData::mutex());
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  (void)consent_settings;
  WarnDesktopUnsupported("SetConsent()");
}
void InitiateOnDeviceConversionMeasurementWithEmailAddress(
    const char* email_address) {
  std::lock_guard lock(internal::FutureData::mutex());
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  (void)email_address;
  WarnDesktopUnsupported(
      "InitiateOnDeviceConversionMeasurementWithEmailAddress()");
}
void InitiateOnDeviceConversionMeasurementWithPhoneNumber(
    const char* phone_number) {
  std::lock_guard lock(internal::FutureData::mutex());
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  (void)phone_number;
  WarnDesktopUnsupported(
      "InitiateOnDeviceConversionMeasurementWithPhoneNumber()");
}
void InitiateOnDeviceConversionMeasurementWithHashedEmailAddress(
    std::vector<unsigned char> hashed_email_address) {
  std::lock_guard lock(internal::FutureData::mutex());
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  (void)hashed_email_address;
  WarnDesktopUnsupported(
      "InitiateOnDeviceConversionMeasurementWithHashedEmailAddress()");
}
void InitiateOnDeviceConversionMeasurementWithHashedPhoneNumber(
    std::vector<unsigned char> hashed_phone_number) {
  std::lock_guard lock(internal::FutureData::mutex());
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  (void)hashed_phone_number;
  WarnDesktopUnsupported(
      "InitiateOnDeviceConversionMeasurementWithHashedPhoneNumber()");
}
void SetSessionTimeoutDuration(int64_t milliseconds) {
  std::lock_guard lock(internal::FutureData::mutex());
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  if (milliseconds < 0) {
    GoogleAnalytics_SetSessionTimeoutInterval(-1);
    return;
  }
  int64_t seconds = milliseconds / 1000;
  if (milliseconds > 0 && seconds == 0) seconds = 1;
  GoogleAnalytics_SetSessionTimeoutInterval(seconds);
}
Future<std::string> GetAnalyticsInstanceId() {
  std::lock_guard lock(internal::FutureData::mutex());
  FIREBASE_ASSERT_RETURN(Future<std::string>(), internal::IsInitialized());
  auto future_data = internal::FutureData::GetShared();
  if (!future_data) return Future<std::string>();
  auto* api = future_data->api();
  const auto handle =
      api->SafeAlloc<std::string>(internal::kAnalyticsFnGetAnalyticsInstanceId);
  api->CompleteWithResult(
      handle, kErrorNone, "",
      "FakeAnalyticsInstanceId" + std::to_string(g_fake_instance_id));
#if defined(_WIN32)
  if (g_analytics_module) {
    LogWarning("Analytics: GetAnalyticsInstanceId() is not supported on Desktop.");
  }
#endif
  return Future<std::string>(api, handle.get());
}
Future<std::string> GetAnalyticsInstanceIdLastResult() {
  std::lock_guard lock(internal::FutureData::mutex());
  FIREBASE_ASSERT_RETURN(Future<std::string>(), internal::IsInitialized());
  auto future_data = internal::FutureData::GetShared();
  if (!future_data) return Future<std::string>();
#if defined(_WIN32)
  if (g_analytics_module) {
    LogWarning(
        "Analytics: GetAnalyticsInstanceIdLastResult() is not supported on "
        "Desktop.");
  }
#endif
  return static_cast<const Future<std::string>&>(
      future_data->api()->LastResult(
          internal::kAnalyticsFnGetAnalyticsInstanceId));
}
Future<int64_t> GetSessionId() {
  std::lock_guard lock(internal::FutureData::mutex());
  FIREBASE_ASSERT_RETURN(Future<int64_t>(), internal::IsInitialized());
  auto future_data = internal::FutureData::GetShared();
  if (!future_data) return Future<int64_t>();
  auto* api = future_data->api();
  const auto handle =
      api->SafeAlloc<int64_t>(internal::kAnalyticsFnGetSessionId);
  api->CompleteWithResult(handle, kErrorNone, "", int64_t(0x5E5510171D570BL));
#if defined(_WIN32)
  if (g_analytics_module) {
    LogWarning("Analytics: GetSessionId() is not supported on Desktop.");
  }
#endif
  return Future<int64_t>(api, handle.get());
}
Future<int64_t> GetSessionIdLastResult() {
  std::lock_guard lock(internal::FutureData::mutex());
  FIREBASE_ASSERT_RETURN(Future<int64_t>(), internal::IsInitialized());
  auto future_data = internal::FutureData::GetShared();
  if (!future_data) return Future<int64_t>();
#if defined(_WIN32)
  if (g_analytics_module) {
    LogWarning(
        "Analytics: GetSessionIdLastResult() is not supported on Desktop.");
  }
#endif
  return static_cast<const Future<int64_t>&>(
      future_data->api()->LastResult(internal::kAnalyticsFnGetSessionId));
}
}  // namespace analytics
}  // namespace firebase
