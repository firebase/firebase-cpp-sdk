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

#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "analytics/src/analytics_common.h"
#include "analytics/src/analytics_desktop_dynamic.h"
#include "analytics/src/analytics_internal.h"
#include "analytics/src/include/firebase/analytics.h"
#include "app/src/future_manager.h"  // For FutureData
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/variant.h"
#include "app/src/log.h"

#if defined(_WIN32)
#include <windows.h>

#include "analytics_windows.h"
#endif  // defined(_WIN32)

namespace firebase {
namespace analytics {

#if defined(_WIN32)
#define ANALYTICS_DLL_FILENAME L"analytics_win.dll"

static HMODULE g_analytics_module = 0;
#endif  // defined(_WIN32)

// Future data for analytics.
// This is initialized in `Initialize()` and cleaned up in `Terminate()`.
static bool g_initialized = false;
static int g_fake_instance_id = 0;
static bool g_analytics_collection_enabled = true;
static std::string g_app_id;
static std::string g_package_name;

// Initializes the Analytics desktop API.
// This function must be called before any other Analytics methods.
void Initialize(const App& app) {
  g_initialized = true;
  internal::RegisterTerminateOnDefaultAppDestroy();
  internal::FutureData::Create();
  g_fake_instance_id = 0;

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

        // Initialize Google Analytics C API
        g_app_id = app.options().app_id();
        g_package_name = app.options().package_name();

        GoogleAnalytics_Options* c_options = GoogleAnalytics_Options_Create();
        if (!c_options) {
          LogError("Analytics: Failed to create GoogleAnalytics_Options.");
        } else {
          c_options->app_id = g_app_id.c_str();
          c_options->package_name = g_package_name.c_str();
          c_options->analytics_collection_enabled_at_first_launch =
              g_analytics_collection_enabled;

          LogInfo(
              "Analytics: Initializing Google Analytics C API with App ID: %s, "
              "Package Name: %s",
              c_options->app_id ? c_options->app_id : "null",
              c_options->package_name ? c_options->package_name : "null");

          if (!GoogleAnalytics_Initialize(c_options)) {
            LogError("Analytics: Failed to initialize Google Analytics C API.");
            // GoogleAnalytics_Initialize destroys c_options automatically if
            // created by _Create
          } else {
            LogInfo(
                "Analytics: Google Analytics C API initialized successfully.");
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
}

namespace internal {

bool IsAnalyticsDllLoaded() {
#if defined(_WIN32)
  return g_analytics_module != 0;
#else
  return false;
#endif  // defined(_WIN32)
}

// Determine whether the analytics module is initialized.
bool IsInitialized() { return g_initialized; }

}  // namespace internal

// Terminates the Analytics desktop API.
// Call this function when Analytics is no longer needed to free up resources.
void Terminate() {
#if defined(_WIN32)
  if (g_analytics_module) {
    FirebaseAnalytics_UnloadDynamicFunctions();
    FreeLibrary(g_analytics_module);
    g_analytics_module = 0;
  }
#endif  // defined(_WIN32)

  internal::FutureData::Destroy();
  internal::UnregisterTerminateOnDefaultAppDestroy();
  g_initialized = false;
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
    } else if (param.value.is_double()) {
      GoogleAnalytics_EventParameters_InsertDouble(c_event_params, param.name,
                                                   param.value.double_value());
    } else if (param.value.is_string()) {
      GoogleAnalytics_EventParameters_InsertString(c_event_params, param.name,
                                                   param.value.string_value());
    } else if (param.value.is_vector()) {
      // Vector types for top-level event parameters are not supported on
      // Desktop. Only specific complex types (like a map processed into an
      // ItemVector) are handled.
#if defined(_WIN32)
      if (g_analytics_module) {
        // Only log this if we are not in stub mode.
        LogError(
            "Analytics: Parameter '%s' has type Vector, which is unsupported "
            "for "
            "event parameters on Desktop. Skipping.",
            param.name);
      }
#endif           // defined(_WIN32)
      continue;  // Skip this parameter
    } else if (param.value.is_map()) {
      // This block handles parameters that are maps.
      // Each key-value pair from the input map is converted into a distinct
      // GoogleAnalytics_Item. In each such GoogleAnalytics_Item, the original
      // key from the map is used directly as the property key, and the original
      // value (which must be a primitive) is set as the property's value. All
      // these GoogleAnalytics_Items are then bundled into a single
      // GoogleAnalytics_ItemVector, which is associated with the original
      // parameter's name.
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
        const std::string& key_from_map = key_variant.mutable_string();
        const firebase::Variant& value_from_map = entry.second;

        GoogleAnalytics_Item* c_item = GoogleAnalytics_Item_Create();
        if (!c_item) {
          LogError(
              "Analytics: Failed to create Item for key '%s' in map parameter "
              "'%s'.",
              key_from_map.c_str(), param.name);
          continue;  // Skip this key-value pair, try next one in map
        }

        bool successfully_set_property = false;
        if (value_from_map.is_int64()) {
          GoogleAnalytics_Item_InsertInt(c_item, key_from_map.c_str(),
                                         value_from_map.int64_value());
          successfully_set_property = true;
        } else if (value_from_map.is_double()) {
          GoogleAnalytics_Item_InsertDouble(c_item, key_from_map.c_str(),
                                            value_from_map.double_value());
          successfully_set_property = true;
        } else if (value_from_map.is_string()) {
          GoogleAnalytics_Item_InsertString(c_item, key_from_map.c_str(),
                                            value_from_map.string_value());
          successfully_set_property = true;
        } else {
          LogWarning(
              "Analytics: Value for key '%s' in map parameter '%s' has an "
              "unsupported Variant type. This key-value pair will be skipped.",
              key_from_map.c_str(), param.name);
          // successfully_set_property remains false
        }

        if (successfully_set_property) {
          GoogleAnalytics_ItemVector_InsertItem(c_item_vector, c_item);
          // c_item is now owned by c_item_vector
          item_vector_populated =
              true;  // Mark that the vector has at least one item
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
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());

  if (name == nullptr || name[0] == '\0') {
    LogError("Analytics: Event name cannot be null or empty.");
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
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  // The C API GoogleAnalytics_SetUserId allows user_id to be nullptr to clear
  // the user ID. The C API documentation also mentions: "The user ID must be
  // non-empty and no more than 256 characters long". We'll pass nullptr as is.
  // If user_id is an empty string "", this might be an issue for the underlying
  // C API or backend if it expects non-empty. However, the Firebase API
  // typically allows passing "" to clear some fields, or it's treated as an
  // invalid value. For SetUserId, `nullptr` is the standard clear mechanism. An
  // empty string might be an invalid ID. For now, we are not adding extra
  // validation for empty string beyond what C API does. Consider adding a check
  // for empty string if Firebase spec requires it. e.g., if (user_id != nullptr
  // && user_id[0] == '\0') { /* log error */ return; }
  GoogleAnalytics_SetUserId(user_id);
}

// Sets whether analytics collection is enabled for this app on this device.
// This setting is persisted across app sessions. By default it is enabled.
//
// @param[in] enabled A flag that enables or disables Analytics collection.
void SetAnalyticsCollectionEnabled(bool enabled) {
  g_analytics_collection_enabled = enabled;

  if (internal::IsInitialized()) {
    GoogleAnalytics_SetAnalyticsCollectionEnabled(enabled);
  }
}

// Clears all analytics data for this app from the device and resets the app
// instance ID.
void ResetAnalyticsData() {
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());

  GoogleAnalytics_ResetAnalyticsData();
  g_fake_instance_id++;
}

// Overloaded versions of LogEvent for convenience.

void LogEvent(const char* name) {
  LogEvent(name, static_cast<const Parameter*>(nullptr), 0);
}

void LogEvent(const char* name, const char* parameter_name,
              const char* parameter_value) {
  if (parameter_name == nullptr) {
    LogEvent(name, static_cast<const Parameter*>(nullptr), 0);
    return;
  }
  Parameter param(parameter_name, parameter_value);
  LogEvent(name, &param, 1);
}

void LogEvent(const char* name, const char* parameter_name,
              const double parameter_value) {
  if (parameter_name == nullptr) {
    LogEvent(name, static_cast<const Parameter*>(nullptr), 0);
    return;
  }
  Parameter param(parameter_name, parameter_value);
  LogEvent(name, &param, 1);
}

void LogEvent(const char* name, const char* parameter_name,
              const int64_t parameter_value) {
  if (parameter_name == nullptr) {
    LogEvent(name, static_cast<const Parameter*>(nullptr), 0);
    return;
  }
  Parameter param(parameter_name, parameter_value);
  LogEvent(name, &param, 1);
}

void LogEvent(const char* name, const char* parameter_name,
              const int parameter_value) {
  if (parameter_name == nullptr) {
    LogEvent(name, static_cast<const Parameter*>(nullptr), 0);
    return;
  }
  Parameter param(parameter_name, static_cast<int64_t>(parameter_value));
  LogEvent(name, &param, 1);
}

// --- Stub Implementations for Unsupported Features ---

void SetDefaultEventParameters(const Parameter* parameters,
                               size_t number_of_parameters) {
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
}

void SetDefaultEventParameters(const std::vector<Parameter>& parameters) {
  SetDefaultEventParameters(parameters.data(), parameters.size());
}

void SetConsent(const std::map<ConsentType, ConsentStatus>& consent_settings) {
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
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  (void)milliseconds;
#if defined(_WIN32)
  if (g_analytics_module) {
    // Only log this if we are not in stub mode.
    LogWarning(
        "Analytics: SetSessionTimeoutDuration() is not supported and has no "
        "effect on Desktop.");
  }
#endif  // defined(_WIN32)
}

Future<std::string> GetAnalyticsInstanceId() {
  FIREBASE_ASSERT_RETURN(Future<std::string>(), internal::IsInitialized());
  auto* api = internal::FutureData::Get()->api();
  const auto future_handle =
      api->SafeAlloc<std::string>(internal::kAnalyticsFnGetAnalyticsInstanceId);
  std::string instance_id = std::string("FakeAnalyticsInstanceId");
  {
    std::stringstream ss;
    ss << g_fake_instance_id;
    instance_id += ss.str();
  }
  api->CompleteWithResult(future_handle, 0, "", instance_id);

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
  FIREBASE_ASSERT_RETURN(Future<std::string>(), internal::IsInitialized());
  LogWarning(
      "Analytics: GetAnalyticsInstanceIdLastResult() is not supported on "
      "Desktop.");
  return static_cast<const Future<std::string>&>(
      internal::FutureData::Get()->api()->LastResult(
          internal::kAnalyticsFnGetAnalyticsInstanceId));
}

Future<int64_t> GetSessionId() {
  FIREBASE_ASSERT_RETURN(Future<int64_t>(), internal::IsInitialized());
  auto* api = internal::FutureData::Get()->api();
  const auto future_handle =
      api->SafeAlloc<int64_t>(internal::kAnalyticsFnGetSessionId);
  int64_t session_id = 0x5E5510171D570BL;  // "SESSIONIDSTUB", kinda
  api->CompleteWithResult(future_handle, 0, "", session_id);
#if defined(_WIN32)
  if (g_analytics_module) {
    // Only log this if we are not in stub mode.
    LogWarning("Analytics: GetSessionId() is not supported on Desktop.");
  }
#endif  // defined(_WIN32)
  return Future<int64_t>(api, future_handle.get());
}

Future<int64_t> GetSessionIdLastResult() {
  FIREBASE_ASSERT_RETURN(Future<int64_t>(), internal::IsInitialized());
#if defined(_WIN32)
  if (g_analytics_module) {
    // Only log this if we are not in stub mode.
    LogWarning(
        "Analytics: GetSessionIdLastResult() is not supported on Desktop.");
  }
#endif  // defined(_WIN32)
  return static_cast<const Future<int64_t>&>(
      internal::FutureData::Get()->api()->LastResult(
          internal::kAnalyticsFnGetSessionId));
}

}  // namespace analytics
}  // namespace firebase
