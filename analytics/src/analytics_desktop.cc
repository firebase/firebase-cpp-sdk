// Copyright 2025 Google LLC
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
#endif  // defined(_WIN32)

namespace firebase {
namespace analytics {

#if defined(_WIN32)
#define ANALYTICS_DLL_FILENAME L"google_analytics.dll"

static HMODULE g_analytics_module = 0;
#endif  // defined(_WIN32)

// Protected by internal::FutureData::mutex().
static bool g_initialized = false;
static int g_fake_instance_id = 0;
static bool g_analytics_collection_enabled = true;

namespace {

LogCallback& GetLogCallback() {
  static auto* s_log_callback = new LogCallback();
  return *s_log_callback;
}

std::mutex& GetLogCallbackMutex() {
  static auto* s_log_callback_mutex = new std::mutex();
  return *s_log_callback_mutex;
}

#if defined(_WIN32)
std::string& GetAppId() {
  static auto* s_app_id = new std::string();
  return *s_app_id;
}

std::string& GetPackageName() {
  static auto* s_package_name = new std::string();
  return *s_package_name;
}
#endif  // defined(_WIN32)

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
    std::lock_guard<std::mutex> lock(GetLogCallbackMutex());
    callback_to_call = GetLogCallback();
  }

  if (callback_to_call) {
    LogLevel firebase_log_level = ConvertAnalyticsLogLevelToFirebaseLogLevel(
        static_cast<GoogleAnalytics_LogLevel>(log_level));
    callback_to_call(firebase_log_level, message ? message : "");
  }
}

// Initializes the Analytics desktop API.
// This function must be called before any other Analytics methods.
void Initialize(const App& app) {
  {
    std::lock_guard<std::recursive_mutex> lock(internal::FutureData::mutex());
    if (g_initialized) {
      LogWarning("Analytics already initialized.");
      return;
    }
    g_initialized = true;
    internal::FutureData::Create();
    g_fake_instance_id = 0;

    bool has_log_callback = false;
    {
      std::lock_guard<std::mutex> callback_lock(GetLogCallbackMutex());
      has_log_callback = static_cast<bool>(GetLogCallback());
    }

#if defined(_WIN32)
    if (!g_analytics_module) {
      std::vector<std::string> allowed_hashes;
      for (int i = 0; i < FirebaseAnalytics_KnownWindowsDllHashCount; i++) {
        allowed_hashes.push_back(
            std::string(FirebaseAnalytics_KnownWindowsDllHashes[i]));
      }

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
          // Do not proceed with C API initialization if functions didn't load
        } else {
          LogInfo("Analytics: Loaded Google Analytics module.");

          // Register the log callback before GoogleAnalytics_Initialize so
          // startup logs are captured.
          GoogleAnalytics_SetLogCallback(GoogleAnalyticsWrapperLogCallback);

          // Initialize Google Analytics C API
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
              // GoogleAnalytics_Initialize destroys c_options automatically if
              // created by _Create
            } else {
              LogInfo(
                  "Analytics: Google Analytics C API initialized "
                  "successfully.");
            }
          }
        }
      } else {
        // LogWarning for g_analytics_module load failure is handled by
        // VerifyAndLoadAnalyticsLibrary
        g_analytics_module = 0;  // Ensure it's null if loading failed
      }
    }
#endif  // defined(_WIN32)

    if (has_log_callback) {
      GoogleAnalytics_SetLogCallback(GoogleAnalyticsWrapperLogCallback);
    }
  }
  internal::RegisterTerminateOnAppDestroy(&app);
}

namespace internal {

// Determine whether the analytics module is initialized.
bool IsInitialized() {
  std::lock_guard<std::recursive_mutex> lock(FutureData::mutex());
  return g_initialized;
}

}  // namespace internal

// Terminates the Analytics desktop API.
// Call this function when Analytics is no longer needed to free up resources.
void Terminate() {
  {
    std::lock_guard<std::recursive_mutex> lock(internal::FutureData::mutex());
    if (!g_initialized) {
      return;
    }
#if defined(_WIN32)
    if (g_analytics_module) {
      // Make sure to notify the SDK that the analytics is being terminated to
      // upload any pending data.
      NotifyAppLifecycleChange(AppLifecycleState::kTermination);
      GoogleAnalytics_SetLogCallback(nullptr);

      FirebaseAnalytics_UnloadDynamicFunctions();
      FreeLibrary(g_analytics_module);
      g_analytics_module = 0;
    }
#endif  // defined(_WIN32)

    GoogleAnalytics_SetLogCallback(nullptr);
    {
      std::lock_guard<std::mutex> callback_lock(GetLogCallbackMutex());
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
  } else if (value.is_bool()) {
    GoogleAnalytics_Item_InsertInt(c_item, key, value.bool_value() ? 1 : 0);
    return true;
  } else if (value.is_double()) {
    GoogleAnalytics_Item_InsertDouble(c_item, key, value.double_value());
    return true;
  } else if (value.is_string()) {
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
  if (!parameters || number_of_parameters == 0 || !c_event_params) {
    return;
  }

  for (size_t i = 0; i < number_of_parameters; ++i) {
    const Parameter& param = parameters[i];
    if (param.name == nullptr || param.name[0] == '\0') {
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
      // A Vector of Maps represents an item list (e.g. kParameterItems), where
      // each Map in the Vector is converted into a single GoogleAnalytics_Item
      // with all of its key-value properties.
      const std::vector<firebase::Variant>& item_vector = param.value.vector();
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
        const std::map<firebase::Variant, firebase::Variant>& item_map =
            element.map();
        if (item_map.empty()) {
          continue;
        }

        GoogleAnalytics_Item* c_item = GoogleAnalytics_Item_Create();
        if (!c_item) {
          LogError(
              "Analytics: Failed to create Item for element in vector "
              "parameter '%s'.",
              param.name);
          continue;
        }

        bool item_populated = false;
        for (const auto& entry : item_map) {
          const firebase::Variant& key_variant = entry.first;
          if (!key_variant.is_string()) {
            LogError("Analytics: Non-string map key found. Skipping.");
            continue;
          }
          const char* key_from_map = key_variant.string_value();
          if (key_from_map == nullptr || key_from_map[0] == '\0') {
            LogError("Analytics: Map key cannot be null or empty. Skipping.");
            continue;
          }
          if (InsertVariantIntoGAItem(c_item, key_from_map, entry.second,
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
      // This block handles parameters that are maps (legacy single-map
      // fallback). Each key-value pair from the input map is converted into a
      // distinct GoogleAnalytics_Item.
      const std::map<firebase::Variant, firebase::Variant>& user_map =
          param.value.map();
      if (user_map.empty()) {
        LogWarning("Analytics: Parameter '%s' is an empty map. Skipping.",
                   param.name);
        continue;  // Skip this parameter
      }

      GoogleAnalytics_ItemVector* c_item_vector =
          GoogleAnalytics_ItemVector_Create();
      if (!c_item_vector) {
        LogError(
            "Analytics: Failed to create ItemVector for map parameter '%s'.",
            param.name);
        continue;  // Skip this parameter
      }

      bool item_vector_populated = false;
      for (const auto& entry : user_map) {
        const firebase::Variant& key_variant = entry.first;
        if (!key_variant.is_string()) {
          LogError("Analytics: Non-string map key found. Skipping.");
          continue;
        }
        const char* key_from_map = key_variant.string_value();
        if (key_from_map == nullptr || key_from_map[0] == '\0') {
          LogError("Analytics: Map key cannot be null or empty. Skipping.");
          continue;
        }

        GoogleAnalytics_Item* c_item = GoogleAnalytics_Item_Create();
        if (!c_item) {
          LogError(
              "Analytics: Failed to create Item for key '%s' in map parameter "
              "'%s'.",
              key_from_map, param.name);
          continue;  // Skip this key-value pair, try next one in map
        }

        if (InsertVariantIntoGAItem(c_item, key_from_map, entry.second,
                                    param.name, "map")) {
          GoogleAnalytics_ItemVector_InsertItem(c_item_vector, c_item);
          // c_item is now owned by c_item_vector
          item_vector_populated = true;
        } else {
          // If no property was set (e.g., value type was unsupported), destroy
          // the created c_item.
          GoogleAnalytics_Item_Destroy(c_item);
        }
      }

      if (item_vector_populated) {
        GoogleAnalytics_EventParameters_InsertItemVector(
            c_event_params, param.name, c_item_vector);
        // c_item_vector is now owned by c_event_params
      } else {
        // If no items were successfully created and added (e.g., all values in
        // map were unsupported types)
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

// Logs an event with the given name and parameters.
void LogEvent(const char* name, const Parameter* parameters,
              size_t number_of_parameters) {
  std::lock_guard<std::recursive_mutex> lock(internal::FutureData::mutex());
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());

  if (name == nullptr || name[0] == '\0') {
    LogError("Analytics: Event name cannot be null or empty.");
    return;
  }
  if (parameters == nullptr && number_of_parameters > 0) {
    LogError(
        "Analytics: Event parameters cannot be null when "
        "number_of_parameters > 0.");
    return;
  }

  GoogleAnalytics_EventParameters* c_event_params = nullptr;
  if (parameters != nullptr && number_of_parameters > 0) {
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
  // GoogleAnalytics_LogEvent is expected to handle the lifecycle of
  // c_event_params if non-null.
}

// log an event and the associated parameters via a vector.
void LogEvent(const char* name, const std::vector<Parameter>& parameters) {
  LogEvent(name, parameters.data(), parameters.size());
}

// Sets a user property to the given value.
//
// Up to 25 user property names are supported. Once set, user property values
// persist throughout the app lifecycle and across sessions.
//
// @param[in] name The name of the user property to set. Should contain 1 to 24
// alphanumeric characters or underscores, and must start with an alphabetic
// character. The "firebase_", "google_", and "ga_" prefixes are reserved and
// should not be used for user property names. Must be UTF-8 encoded.
// @param[in] value The value of the user property. Values can be up to 36
// characters long. Setting the value to `nullptr` or an empty string will
// clear the user property. Must be UTF-8 encoded if not nullptr.
void SetUserProperty(const char* name, const char* property) {
  std::lock_guard<std::recursive_mutex> lock(internal::FutureData::mutex());
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());

  if (name == nullptr || name[0] == '\0') {
    LogError("Analytics: User property name cannot be null or empty.");
    return;
  }
  // The C API GoogleAnalytics_SetUserProperty allows value to be nullptr to
  // remove the property. If value is an empty string, it might also be treated
  // as clearing by some backends, or it might be an invalid value. The C API
  // doc says: "Setting the value to `nullptr` remove the user property." For
  // consistency, we pass it as is.
  GoogleAnalytics_SetUserProperty(name, property);
}

// Sets the user ID property.
// This feature must be used in accordance with Google's Privacy Policy.
//
// @param[in] user_id The user ID associated with the user of this app on this
// device. The user ID must be non-empty if not nullptr, and no more than 256
// characters long, and UTF-8 encoded. Setting user_id to `nullptr` removes
// the user ID.
void SetUserId(const char* user_id) {
  std::lock_guard<std::recursive_mutex> lock(internal::FutureData::mutex());
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  GoogleAnalytics_SetUserId(user_id);
}

// Sets whether analytics collection is enabled for this app on this device.
// This setting is persisted across app sessions. By default it is enabled.
//
// @param[in] enabled A flag that enables or disables Analytics collection.
void SetAnalyticsCollectionEnabled(bool enabled) {
  std::lock_guard<std::recursive_mutex> lock(internal::FutureData::mutex());
  g_analytics_collection_enabled = enabled;

  if (g_initialized) {
    GoogleAnalytics_SetAnalyticsCollectionEnabled(enabled);
  }
}

// Clears all analytics data for this app from the device and resets the app
// instance ID.
void ResetAnalyticsData() {
  std::lock_guard<std::recursive_mutex> lock(internal::FutureData::mutex());
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());

  GoogleAnalytics_ResetAnalyticsData();
  g_fake_instance_id++;
}

// Allows the passing of a callback to be used when the SDK logs any
// messages regarding its behavior. The callback must be thread-safe.
void SetLogCallback(const LogCallback& callback) {
  std::lock_guard<std::recursive_mutex> lock(internal::FutureData::mutex());
  // The C API does not support user data, so we must use a global variable.
  {
    std::lock_guard<std::mutex> callback_lock(GetLogCallbackMutex());
    GetLogCallback() = callback;
  }
  if (g_initialized) {
    if (callback) {
      GoogleAnalytics_SetLogCallback(GoogleAnalyticsWrapperLogCallback);
    } else {
      GoogleAnalytics_SetLogCallback(nullptr);
    }
  }
}

// Notify the Analytics SDK about the current state of the app's lifecycle.
void NotifyAppLifecycleChange(AppLifecycleState state) {
  std::lock_guard<std::recursive_mutex> lock(internal::FutureData::mutex());
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  GoogleAnalytics_NotifyAppLifecycleChange(
      static_cast<GoogleAnalytics_AppLifecycleState>(state));
}

// Returns true if the desktop Analytics dll is initialized.
bool IsDesktopInitialized() {
  std::lock_guard<std::recursive_mutex> lock(internal::FutureData::mutex());
  FIREBASE_ASSERT_RETURN(false, internal::IsInitialized());
  return GoogleAnalytics_IsInitialized();
}

// Sends the enabled Flag to the desktop Analytics dll.
void SetDesktopDebugMode(bool enabled) {
  std::lock_guard<std::recursive_mutex> lock(internal::FutureData::mutex());
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  GoogleAnalytics_SetDebugMode(enabled);
}

// Overloaded versions of LogEvent for convenience.

void LogEvent(const char* name) {
  LogEvent(name, static_cast<const Parameter*>(nullptr), 0);
}

/// Log an Apple StoreKit 2 transaction. This is a no-op on Desktop and returns
/// success.
Future<void> LogAppleTransaction(const char* transaction_id) {
  (void)transaction_id;
  std::lock_guard<std::recursive_mutex> lock(internal::FutureData::mutex());
  FIREBASE_ASSERT_RETURN(Future<void>(), internal::IsInitialized());
  std::shared_ptr<internal::FutureData> future_data =
      internal::FutureData::GetShared();
  if (!future_data) {
    return Future<void>();
  }
  auto* api = future_data->api();
  const auto future_handle =
      api->SafeAlloc<void>(internal::kAnalyticsFnLogAppleTransaction);
  api->Complete(future_handle, kErrorNone, "");
  return Future<void>(api, future_handle.get());
}

Future<void> LogAppleTransactionLastResult() {
  std::lock_guard<std::recursive_mutex> lock(internal::FutureData::mutex());
  FIREBASE_ASSERT_RETURN(Future<void>(), internal::IsInitialized());
  std::shared_ptr<internal::FutureData> future_data =
      internal::FutureData::GetShared();
  if (!future_data) {
    return Future<void>();
  }
  return static_cast<const Future<void>&>(
      future_data->api()->LastResult(internal::kAnalyticsFnLogAppleTransaction));
}

void LogEvent(const char* name, const char* parameter_name,
              const char* parameter_value) {
  if (parameter_name == nullptr) {
    LogError("Analytics: Parameter name cannot be null.");
    return;
  }
  Parameter param(parameter_name, parameter_value);
  LogEvent(name, &param, 1);
}

void LogEvent(const char* name, const char* parameter_name,
              const double parameter_value) {
  if (parameter_name == nullptr) {
    LogError("Analytics: Parameter name cannot be null.");
    return;
  }
  Parameter param(parameter_name, parameter_value);
  LogEvent(name, &param, 1);
}

void LogEvent(const char* name, const char* parameter_name,
              const int64_t parameter_value) {
  if (parameter_name == nullptr) {
    LogError("Analytics: Parameter name cannot be null.");
    return;
  }
  Parameter param(parameter_name, parameter_value);
  LogEvent(name, &param, 1);
}

void LogEvent(const char* name, const char* parameter_name,
              const int parameter_value) {
  if (parameter_name == nullptr) {
    LogError("Analytics: Parameter name cannot be null.");
    return;
  }
  Parameter param(parameter_name, static_cast<int64_t>(parameter_value));
  LogEvent(name, &param, 1);
}

// --- Stub Implementations for Unsupported Features ---

void SetDefaultEventParameters(const Parameter* parameters,
                               size_t number_of_parameters) {
  std::lock_guard<std::recursive_mutex> lock(internal::FutureData::mutex());
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  if (parameters == nullptr && number_of_parameters > 0) {
    LogError(
        "Analytics: Default event parameters cannot be null when "
        "number_of_parameters > 0.");
    return;
  }
#if defined(_WIN32)
  if (g_analytics_module) {
    // Only log this if we are not in stub mode.
    LogWarning(
        "Analytics: SetDefaultEventParameters() is not supported and has no "
        "effect on Desktop.");
  }
#endif  // defined(_WIN32)
}

void SetDefaultEventParameters(const std::vector<Parameter>& parameters) {
  SetDefaultEventParameters(parameters.data(), parameters.size());
}

void SetConsent(const std::map<ConsentType, ConsentStatus>& consent_settings) {
  std::lock_guard<std::recursive_mutex> lock(internal::FutureData::mutex());
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());

  // Not supported by the Windows C API.
  (void)consent_settings;  // Mark as unused
#if defined(_WIN32)
  if (g_analytics_module) {
    // Only log this if we are not in stub mode.
    LogWarning(
        "Analytics: SetConsent() is not supported and has no effect on "
        "Desktop.");
  }
#endif  // defined(_WIN32)
}

void InitiateOnDeviceConversionMeasurementWithEmailAddress(
    const char* email_address) {
  std::lock_guard<std::recursive_mutex> lock(internal::FutureData::mutex());
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  (void)email_address;
#if defined(_WIN32)
  if (g_analytics_module) {
    // Only log this if we are not in stub mode.
    LogWarning(
        "Analytics: InitiateOnDeviceConversionMeasurementWithEmailAddress() is "
        "not supported and has no effect on Desktop.");
  }
#endif  // defined(_WIN32)
}

void InitiateOnDeviceConversionMeasurementWithPhoneNumber(
    const char* phone_number) {
  std::lock_guard<std::recursive_mutex> lock(internal::FutureData::mutex());
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  (void)phone_number;
#if defined(_WIN32)
  if (g_analytics_module) {
    // Only log this if we are not in stub mode.
    LogWarning(
        "Analytics: InitiateOnDeviceConversionMeasurementWithPhoneNumber() is "
        "not supported and has no effect on Desktop.");
  }
#endif  // defined(_WIN32)
}

void InitiateOnDeviceConversionMeasurementWithHashedEmailAddress(
    std::vector<unsigned char> hashed_email_address) {
  std::lock_guard<std::recursive_mutex> lock(internal::FutureData::mutex());
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  (void)hashed_email_address;
#if defined(_WIN32)
  if (g_analytics_module) {
    // Only log this if we are not in stub mode.
    LogWarning(
        "Analytics: "
        "InitiateOnDeviceConversionMeasurementWithHashedEmailAddress() is not "
        "supported and has no effect on Desktop.");
  }
#endif  // defined(_WIN32)
}

void InitiateOnDeviceConversionMeasurementWithHashedPhoneNumber(
    std::vector<unsigned char> hashed_phone_number) {
  std::lock_guard<std::recursive_mutex> lock(internal::FutureData::mutex());
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  (void)hashed_phone_number;
#if defined(_WIN32)
  if (g_analytics_module) {
    // Only log this if we are not in stub mode.
    LogWarning(
        "Analytics: "
        "InitiateOnDeviceConversionMeasurementWithHashedPhoneNumber() "
        "is not supported and has no effect on Desktop.");
  }
#endif  // defined(_WIN32)
}

void SetSessionTimeoutDuration(int64_t milliseconds) {
  std::lock_guard<std::recursive_mutex> lock(internal::FutureData::mutex());
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  if (milliseconds < 0) {
    GoogleAnalytics_SetSessionTimeoutInterval(-1);
    return;
  }
  int64_t seconds = milliseconds / 1000;
  if (milliseconds > 0 && seconds == 0) {
    seconds = 1;
  }
  GoogleAnalytics_SetSessionTimeoutInterval(seconds);
}

Future<std::string> GetAnalyticsInstanceId() {
  std::lock_guard<std::recursive_mutex> lock(internal::FutureData::mutex());
  FIREBASE_ASSERT_RETURN(Future<std::string>(), internal::IsInitialized());
  std::shared_ptr<internal::FutureData> future_data =
      internal::FutureData::GetShared();
  if (!future_data) {
    return Future<std::string>();
  }
  auto* api = future_data->api();
  const auto future_handle =
      api->SafeAlloc<std::string>(internal::kAnalyticsFnGetAnalyticsInstanceId);
  std::string instance_id =
      "FakeAnalyticsInstanceId" + std::to_string(g_fake_instance_id);
  api->CompleteWithResult(future_handle, kErrorNone, "", instance_id);

#if defined(_WIN32)
  if (g_analytics_module) {
    // Only log this if we are not in stub mode.
    LogWarning(
        "Analytics: GetAnalyticsInstanceId() is not supported on Desktop.");
  }
#endif  // defined(_WIN32)
  return Future<std::string>(api, future_handle.get());
}

Future<std::string> GetAnalyticsInstanceIdLastResult() {
  std::lock_guard<std::recursive_mutex> lock(internal::FutureData::mutex());
  FIREBASE_ASSERT_RETURN(Future<std::string>(), internal::IsInitialized());
  std::shared_ptr<internal::FutureData> future_data =
      internal::FutureData::GetShared();
  if (!future_data) {
    return Future<std::string>();
  }
#if defined(_WIN32)
  if (g_analytics_module) {
    // Only log this if we are not in stub mode.
    LogWarning(
        "Analytics: GetAnalyticsInstanceIdLastResult() is not supported on "
        "Desktop.");
  }
#endif  // defined(_WIN32)
  return static_cast<const Future<std::string>&>(
      future_data->api()->LastResult(
          internal::kAnalyticsFnGetAnalyticsInstanceId));
}

Future<int64_t> GetSessionId() {
  std::lock_guard<std::recursive_mutex> lock(internal::FutureData::mutex());
  FIREBASE_ASSERT_RETURN(Future<int64_t>(), internal::IsInitialized());
  std::shared_ptr<internal::FutureData> future_data =
      internal::FutureData::GetShared();
  if (!future_data) {
    return Future<int64_t>();
  }
  auto* api = future_data->api();
  const auto future_handle =
      api->SafeAlloc<int64_t>(internal::kAnalyticsFnGetSessionId);
  int64_t session_id = 0x5E5510171D570BL;  // "SESSIONIDSTUB", kinda
  api->CompleteWithResult(future_handle, kErrorNone, "", session_id);
#if defined(_WIN32)
  if (g_analytics_module) {
    // Only log this if we are not in stub mode.
    LogWarning("Analytics: GetSessionId() is not supported on Desktop.");
  }
#endif  // defined(_WIN32)
  return Future<int64_t>(api, future_handle.get());
}

Future<int64_t> GetSessionIdLastResult() {
  std::lock_guard<std::recursive_mutex> lock(internal::FutureData::mutex());
  FIREBASE_ASSERT_RETURN(Future<int64_t>(), internal::IsInitialized());
  std::shared_ptr<internal::FutureData> future_data =
      internal::FutureData::GetShared();
  if (!future_data) {
    return Future<int64_t>();
  }
#if defined(_WIN32)
  if (g_analytics_module) {
    // Only log this if we are not in stub mode.
    LogWarning(
        "Analytics: GetSessionIdLastResult() is not supported on Desktop.");
  }
#endif  // defined(_WIN32)
  return static_cast<const Future<int64_t>&>(
      future_data->api()->LastResult(
          internal::kAnalyticsFnGetSessionId));
}

}  // namespace analytics
}  // namespace firebase
