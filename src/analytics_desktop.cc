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

#include "analytics/src/windows/analytics_windows.h"
#include "app/src/include/firebase/app.h"
#include "analytics/src/include/firebase/analytics.h" // Path confirmed to remain as is
#include "analytics/src/common/analytics_common.h"
#include "common/src/include/firebase/variant.h"
#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/log.h"
#include "app/src/future_manager.h" // For FutureData

#include <vector>
#include <string>
#include <map>

namespace firebase {
namespace analytics {

// Future data for analytics.
// This is initialized in `Initialize()` and cleaned up in `Terminate()`.
static FutureData* g_future_data = nullptr;

// Initializes the Analytics desktop API.
// This function must be called before any other Analytics methods.
void Initialize(const App& app) {
  // The 'app' parameter is not directly used by the underlying Google Analytics C API
  // for Windows for global initialization. It's included for API consistency
  // with other Firebase platforms.
  (void)app;

  if (g_future_data) {
    LogWarning("Analytics: Initialize() called when already initialized.");
  } else {
    g_future_data = new FutureData(internal::kAnalyticsFnCount);
  }
}

// Terminates the Analytics desktop API.
// Call this function when Analytics is no longer needed to free up resources.
void Terminate() {
  // The underlying Google Analytics C API for Windows does not have an explicit
  // global termination or shutdown function. Resources like event parameter maps
  // are managed at the point of their use (e.g., destroyed after logging).
  // This function is provided for API consistency with other Firebase platforms
  // and for any future global cleanup needs for the desktop wrapper.
  if (g_future_data) {
    delete g_future_data;
    g_future_data = nullptr;
  } else {
    LogWarning("Analytics: Terminate() called when not initialized or already terminated.");
  }
}

static void ConvertParametersToGAParams(
    const Parameter* parameters,
    size_t number_of_parameters,
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
      GoogleAnalytics_EventParameters_InsertString(
          c_event_params, param.name, param.value.string_value());
    } else if (param.value.is_vector()) {
      // Vector types for top-level event parameters are not supported on Desktop.
      // Only specific complex types (like a map processed into an ItemVector) are handled.
      LogError("Analytics: Parameter '%s' has type Vector, which is unsupported for event parameters on Desktop. Skipping.", param.name);
      continue; // Skip this parameter
    } else if (param.value.is_map()) {
      // This block handles parameters that are maps.
      // Each key-value pair in the map is converted into a GoogleAnalytics_Item,
      // and all such items are bundled into a GoogleAnalytics_ItemVector,
      // which is then inserted into the event parameters.
      // The original map's key becomes the "name" property of the GA_Item,
      // and the map's value becomes one of "int_value", "double_value", or "string_value".
      const std::map<std::string, firebase::Variant>& user_map =
          param.value.map_value();
      if (user_map.empty()) {
        LogWarning("Analytics: Parameter '%s' is an empty map. Skipping.", param.name);
        continue; // Skip this parameter
      }

      GoogleAnalytics_ItemVector* c_item_vector =
          GoogleAnalytics_ItemVector_Create();
      if (!c_item_vector) {
        LogError("Analytics: Failed to create ItemVector for map parameter '%s'.", param.name);
        continue; // Skip this parameter
      }

      bool item_vector_populated = false;
      for (const auto& entry : user_map) {
        const std::string& key_from_map = entry.first;
        const firebase::Variant& value_from_map = entry.second;

        GoogleAnalytics_Item* c_item = GoogleAnalytics_Item_Create();
        if (!c_item) {
          LogError("Analytics: Failed to create Item for key '%s' in map parameter '%s'.", key_from_map.c_str(), param.name);
          continue; // Skip this key-value pair, try next one in map
        }

        // Removed: GoogleAnalytics_Item_InsertString(c_item, "name", key_from_map.c_str());

        bool successfully_set_property = false;
        if (value_from_map.is_int64()) {
          GoogleAnalytics_Item_InsertInt(c_item, key_from_map.c_str(), value_from_map.int64_value());
          successfully_set_property = true;
        } else if (value_from_map.is_double()) {
          GoogleAnalytics_Item_InsertDouble(c_item, key_from_map.c_str(), value_from_map.double_value());
          successfully_set_property = true;
        } else if (value_from_map.is_string()) {
          GoogleAnalytics_Item_InsertString(c_item, key_from_map.c_str(), value_from_map.string_value());
          successfully_set_property = true;
        } else {
          LogWarning("Analytics: Value for key '%s' in map parameter '%s' has an unsupported Variant type. This key-value pair will be skipped.", key_from_map.c_str(), param.name);
          // successfully_set_property remains false
        }

        if (successfully_set_property) {
          GoogleAnalytics_ItemVector_InsertItem(c_item_vector, c_item);
          // c_item is now owned by c_item_vector
          item_vector_populated = true; // Mark that the vector has at least one item
        } else {
          // If no property was set (e.g., value type was unsupported), destroy the created c_item.
          GoogleAnalytics_Item_Destroy(c_item);
        }
      }

      if (item_vector_populated) {
        GoogleAnalytics_EventParameters_InsertItemVector(
            c_event_params, param.name, c_item_vector);
        // c_item_vector is now owned by c_event_params
      } else {
        // If no items were successfully created and added (e.g., all values in map were unsupported types)
        GoogleAnalytics_ItemVector_Destroy(c_item_vector);
        LogWarning("Analytics: Map parameter '%s' resulted in an empty ItemVector; no valid key-value pairs found or all values had unsupported types. This map parameter was skipped.", param.name);
      }
    } else {
      LogWarning("Analytics: Unsupported variant type for parameter '%s'.", param.name);
    }
  }
}

// Logs an event with the given name and parameters.
void LogEvent(const char* name,
              const Parameter* parameters, // firebase::analytics::Parameter
              size_t number_of_parameters) {
  if (name == nullptr || name[0] == '\0') {
    LogError("Analytics: Event name cannot be null or empty.");
    return;
  }

  GoogleAnalytics_EventParameters* c_event_params = nullptr;
  if (parameters != nullptr && number_of_parameters > 0) {
    c_event_params = GoogleAnalytics_EventParameters_Create();
    if (!c_event_params) {
      LogError("Analytics: Failed to create event parameters map for event '%s'.", name);
      return;
    }
    ConvertParametersToGAParams(parameters, number_of_parameters, c_event_params);
  }

  GoogleAnalytics_LogEvent(name, c_event_params);
  // GoogleAnalytics_LogEvent is expected to handle the lifecycle of c_event_params if non-null.
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
  if (name == nullptr || name[0] == '\0') {
    LogError("Analytics: User property name cannot be null or empty.");
    return;
  }
  // The C API GoogleAnalytics_SetUserProperty allows value to be nullptr to remove the property.
  // If value is an empty string, it might also be treated as clearing by some backends,
  // or it might be an invalid value. The C API doc says:
  // "Setting the value to `nullptr` remove the user property."
  // For consistency, we pass it as is.
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
  // The C API GoogleAnalytics_SetUserId allows user_id to be nullptr to clear the user ID.
  // The C API documentation also mentions: "The user ID must be non-empty and
  // no more than 256 characters long".
  // We'll pass nullptr as is. If user_id is an empty string "", this might be
  // an issue for the underlying C API or backend if it expects non-empty.
  // However, the Firebase API typically allows passing "" to clear some fields,
  // or it's treated as an invalid value. For SetUserId, `nullptr` is the standard
  // clear mechanism. An empty string might be an invalid ID.
  // For now, we are not adding extra validation for empty string beyond what C API does.
  // Consider adding a check for empty string if Firebase spec requires it.
  // e.g., if (user_id != nullptr && user_id[0] == '\0') { /* log error */ return; }
  GoogleAnalytics_SetUserId(user_id);
}

// Sets whether analytics collection is enabled for this app on this device.
// This setting is persisted across app sessions. By default it is enabled.
//
// @param[in] enabled A flag that enables or disables Analytics collection.
void SetAnalyticsCollectionEnabled(bool enabled) {
  GoogleAnalytics_SetAnalyticsCollectionEnabled(enabled);
}

// Clears all analytics data for this app from the device and resets the app
// instance ID.
void ResetAnalyticsData() {
  GoogleAnalytics_ResetAnalyticsData();
}

// --- Stub Implementations for Unsupported Features ---

void SetConsent(const std::map<ConsentType, ConsentStatus>& consent_settings) {
  // Not supported by the Windows C API.
  (void)consent_settings; // Mark as unused
  LogWarning("Analytics: SetConsent() is not supported and has no effect on Desktop.");
}

void LogEvent(const char* name) {
  LogEvent(name, nullptr, 0);
}

void LogEvent(const char* name, const char* parameter_name,
              const char* parameter_value) {
  if (parameter_name == nullptr) {
    LogEvent(name, nullptr, 0);
    return;
  }
  Parameter param(parameter_name, parameter_value);
  LogEvent(name, &param, 1);
}

void LogEvent(const char* name, const char* parameter_name,
              const double parameter_value) {
  if (parameter_name == nullptr) {
    LogEvent(name, nullptr, 0);
    return;
  }
  Parameter param(parameter_name, parameter_value);
  LogEvent(name, &param, 1);
}

void LogEvent(const char* name, const char* parameter_name,
              const int64_t parameter_value) {
  if (parameter_name == nullptr) {
    LogEvent(name, nullptr, 0);
    return;
  }
  Parameter param(parameter_name, parameter_value);
  LogEvent(name, &param, 1);
}

void LogEvent(const char* name, const char* parameter_name,
              const int parameter_value) {
  if (parameter_name == nullptr) {
    LogEvent(name, nullptr, 0);
    return;
  }
  Parameter param(parameter_name, static_cast<int64_t>(parameter_value));
  LogEvent(name, &param, 1);
}

void InitiateOnDeviceConversionMeasurementWithEmailAddress(
    const char* email_address) {
  (void)email_address;
  LogWarning("Analytics: InitiateOnDeviceConversionMeasurementWithEmailAddress() is not supported and has no effect on Desktop.");
}

void InitiateOnDeviceConversionMeasurementWithPhoneNumber(
    const char* phone_number) {
  (void)phone_number;
  LogWarning("Analytics: InitiateOnDeviceConversionMeasurementWithPhoneNumber() is not supported and has no effect on Desktop.");
}

void InitiateOnDeviceConversionMeasurementWithHashedEmailAddress(
    std::vector<unsigned char> hashed_email_address) {
  (void)hashed_email_address;
  LogWarning("Analytics: InitiateOnDeviceConversionMeasurementWithHashedEmailAddress() is not supported and has no effect on Desktop.");
}

void InitiateOnDeviceConversionMeasurementWithHashedPhoneNumber(
    std::vector<unsigned char> hashed_phone_number) {
  (void)hashed_phone_number;
  LogWarning("Analytics: InitiateOnDeviceConversionMeasurementWithHashedPhoneNumber() is not supported and has no effect on Desktop.");
}

void SetSessionTimeoutDuration(int64_t milliseconds) {
  (void)milliseconds;
  LogWarning("Analytics: SetSessionTimeoutDuration() is not supported and has no effect on Desktop.");
}

Future<std::string> GetAnalyticsInstanceId() {
  LogWarning("Analytics: GetAnalyticsInstanceId() is not supported on Desktop.");
  if (!g_future_data) {
    LogError("Analytics: API not initialized; call Initialize() first.");
    static firebase::Future<std::string> invalid_future; // Default invalid
    if (!g_future_data) return invalid_future; // Or some other error future
  }
  const auto handle =
      g_future_data->CreateFuture(internal::kAnalyticsFn_GetAnalyticsInstanceId, nullptr);
  g_future_data->CompleteFuture(handle, 0 /* error_code */, nullptr /* error_message_string */);
  return g_future_data->GetFuture<std::string>(handle);
}

Future<std::string> GetAnalyticsInstanceIdLastResult() {
  if (!g_future_data) {
    LogError("Analytics: API not initialized; call Initialize() first.");
    static firebase::Future<std::string> invalid_future;
    return invalid_future;
  }
  return g_future_data->LastResult<std::string>(internal::kAnalyticsFn_GetAnalyticsInstanceId);
}

Future<int64_t> GetSessionId() {
  LogWarning("Analytics: GetSessionId() is not supported on Desktop.");
  if (!g_future_data) {
    LogError("Analytics: API not initialized; call Initialize() first.");
    static firebase::Future<int64_t> invalid_future;
    return invalid_future;
  }
  const auto handle =
      g_future_data->CreateFuture(internal::kAnalyticsFn_GetSessionId, nullptr);
  g_future_data->CompleteFuture(handle, 0 /* error_code */, nullptr /* error_message_string */);
  return g_future_data->GetFuture<int64_t>(handle);
}

Future<int64_t> GetSessionIdLastResult() {
  if (!g_future_data) {
    LogError("Analytics: API not initialized; call Initialize() first.");
    static firebase::Future<int64_t> invalid_future;
    return invalid_future;
  }
  return g_future_data->LastResult<int64_t>(internal::kAnalyticsFn_GetSessionId);
}

}  // namespace analytics
}  // namespace firebase
