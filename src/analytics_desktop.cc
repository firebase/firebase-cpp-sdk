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
#include "firebase/app.h"
#include "firebase/analytics.h"
#include "firebase/variant.h"
#include "firebase/future.h"
#include "firebase/log.h" // <-- New include

#include <vector>
#include <string>
#include <map>

// Error code for Analytics features not supported on this platform.
const int kAnalyticsErrorNotSupportedOnPlatform = 1; // Or a more specific range

namespace firebase {
namespace analytics {

// Initializes the Analytics desktop API.
// This function must be called before any other Analytics methods.
void Initialize(const App& app) {
  // The 'app' parameter is not directly used by the underlying Google Analytics C API
  // for Windows for global initialization. It's included for API consistency
  // with other Firebase platforms.
  // (void)app; // Mark as unused if applicable by style guides.

  // The underlying Google Analytics C API for Windows does not have an explicit
  // global initialization function.
  // This function is provided for API consistency with other Firebase platforms
  // and for any future global initialization needs for the desktop wrapper.
}

// Terminates the Analytics desktop API.
// Call this function when Analytics is no longer needed to free up resources.
void Terminate() {
  // The underlying Google Analytics C API for Windows does not have an explicit
  // global termination or shutdown function. Resources like event parameter maps
  // are managed at the point of their use (e.g., destroyed after logging).
  // This function is provided for API consistency with other Firebase platforms
  // and for any future global cleanup needs for the desktop wrapper.
}

static void ConvertParametersToGAParams(
    const Parameter* parameters, // firebase::analytics::Parameter
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
      // This block handles parameters that are vectors of items (e.g., kParameterItems).
      // The 'param.value' is expected to be a firebase::Variant of type kTypeVector.
      // Each element in this outer vector (item_variants) is itself a firebase::Variant,
      // which must be of type kTypeMap, representing a single item's properties.
      const std::vector<firebase::Variant>& item_variants =
          param.value.vector_value();

      GoogleAnalytics_ItemVector* c_item_vector =
          GoogleAnalytics_ItemVector_Create();
      if (!c_item_vector) {
        LogError("Analytics: Failed to create ItemVector for parameter '%s'.", param.name);
        continue; // Skip this parameter
      }

      bool item_vector_populated = false;
      for (const firebase::Variant& item_variant : item_variants) {
        if (item_variant.is_map()) {
          const std::map<std::string, firebase::Variant>& item_map =
              item_variant.map_value();

          GoogleAnalytics_Item* c_item = GoogleAnalytics_Item_Create();
          if (!c_item) {
            LogError("Analytics: Failed to create Item for an item in vector parameter '%s'.", param.name);
            continue;
          }

          bool item_populated = false;
          for (const auto& entry : item_map) {
            const std::string& item_key = entry.first;
            const firebase::Variant& item_val = entry.second;

            if (item_val.is_int64()) {
              GoogleAnalytics_Item_InsertInt(c_item, item_key.c_str(),
                                             item_val.int64_value());
              item_populated = true;
            } else if (item_val.is_double()) {
              GoogleAnalytics_Item_InsertDouble(c_item, item_key.c_str(),
                                                item_val.double_value());
              item_populated = true;
            } else if (item_val.is_string()) {
              GoogleAnalytics_Item_InsertString(c_item, item_key.c_str(),
                                                item_val.string_value());
              item_populated = true;
            } else {
                LogWarning("Analytics: Unsupported variant type in Item map for key '%s' in vector parameter '%s'.", item_key.c_str(), param.name);
            }
          }

          if (item_populated) {
            GoogleAnalytics_ItemVector_InsertItem(c_item_vector, c_item);
            item_vector_populated = true;
          } else {
            GoogleAnalytics_Item_Destroy(c_item);
          }
        } else {
          LogWarning("Analytics: Expected a map (Item) in vector parameter '%s', but found a different Variant type.", param.name);
        }
      }

      if (item_vector_populated) {
        GoogleAnalytics_EventParameters_InsertItemVector(
            c_event_params, param.name, c_item_vector);
      } else {
        GoogleAnalytics_ItemVector_Destroy(c_item_vector);
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
  // c_event_params is consumed by GoogleAnalytics_LogEvent, so no explicit destroy if passed.
  // However, if we created it but didn't pass it (e.g. error), it should be destroyed.
  // The C API doc says: "Automatically destroyed when it is logged."
  // "The caller is responsible for destroying the event parameter map using the
  // GoogleAnalytics_EventParameters_Destroy() function, unless it has been
  // logged, in which case it will be destroyed automatically when it is logged."
  // So, if GoogleAnalytics_LogEvent is called with c_event_params, it's handled.
  // If there was an error before that, and c_event_params was allocated, it would leak.
  // For robustness, a unique_ptr or similar RAII wrapper would be better for c_event_params
  // if not for the C API's ownership transfer.
  // Given the current structure, if c_event_params is created, it's always passed or should be.
  // If `name` is invalid, we return early, c_event_params is not created.
  // If `c_event_params` creation fails, we return, nothing to destroy.
  // If a parameter is bad, we `continue`, `c_event_params` is still valid and eventually logged.
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
  LogWarning("Analytics: SetConsent() is not supported and has no effect on Windows.");
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
  LogWarning("Analytics: InitiateOnDeviceConversionMeasurementWithEmailAddress() is not supported and has no effect on Windows.");
}

void InitiateOnDeviceConversionMeasurementWithPhoneNumber(
    const char* phone_number) {
  (void)phone_number;
  LogWarning("Analytics: InitiateOnDeviceConversionMeasurementWithPhoneNumber() is not supported and has no effect on Windows.");
}

void InitiateOnDeviceConversionMeasurementWithHashedEmailAddress(
    std::vector<unsigned char> hashed_email_address) {
  (void)hashed_email_address;
  LogWarning("Analytics: InitiateOnDeviceConversionMeasurementWithHashedEmailAddress() is not supported and has no effect on Windows.");
}

void InitiateOnDeviceConversionMeasurementWithHashedPhoneNumber(
    std::vector<unsigned char> hashed_phone_number) {
  (void)hashed_phone_number;
  LogWarning("Analytics: InitiateOnDeviceConversionMeasurementWithHashedPhoneNumber() is not supported and has no effect on Windows.");
}

void SetSessionTimeoutDuration(int64_t milliseconds) {
  (void)milliseconds;
  LogWarning("Analytics: SetSessionTimeoutDuration() is not supported and has no effect on Windows.");
}

Future<std::string> GetAnalyticsInstanceId() {
  // Not supported by the Windows C API.
  // Return a Future that is already completed with an error.
  firebase::FutureHandle handle; // Dummy handle for error
  // TODO(jules): Ensure g_future_api_table is appropriate or replace with direct Future creation.
  auto future = MakeFuture<std::string>(&firebase::g_future_api_table, handle);
  future.Complete(handle, kAnalyticsErrorNotSupportedOnPlatform, "GetAnalyticsInstanceId is not supported on Windows.");
  return future;
}

Future<std::string> GetAnalyticsInstanceIdLastResult() {
  // This typically returns the last result of the async call.
  // Since GetAnalyticsInstanceId is not supported, this also returns a failed future.
  firebase::FutureHandle handle;
  auto future = MakeFuture<std::string>(&firebase::g_future_api_table, handle);
  future.Complete(handle, kAnalyticsErrorNotSupportedOnPlatform, "GetAnalyticsInstanceId is not supported on Windows.");
  return future;
}

Future<int64_t> GetSessionId() {
  // Not supported by the Windows C API.
  firebase::FutureHandle handle;
  auto future = MakeFuture<int64_t>(&firebase::g_future_api_table, handle);
  future.Complete(handle, kAnalyticsErrorNotSupportedOnPlatform, "GetSessionId is not supported on Windows.");
  return future;
}

Future<int64_t> GetSessionIdLastResult() {
  firebase::FutureHandle handle;
  auto future = MakeFuture<int64_t>(&firebase::g_future_api_table, handle);
  future.Complete(handle, kAnalyticsErrorNotSupportedOnPlatform, "GetSessionId is not supported on Windows.");
  return future;
}

}  // namespace analytics
}  // namespace firebase
