/*
 * Copyright 2025 Google LLC
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

#ifndef ANALYTICS_MOBILE_CONSOLE_MEASUREMENT_PRERELEASE_C_ANALYTICS_H_
#define ANALYTICS_MOBILE_CONSOLE_MEASUREMENT_PRERELEASE_C_ANALYTICS_H_

#include <stdbool.h>
#include <stdint.h>

#if defined(ANALYTICS_DLL) && defined(_WIN32)
#define ANALYTICS_API __declspec(dllexport)
#else
#define ANALYTICS_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct GoogleAnalytics_Reserved_Opaque GoogleAnalytics_Reserved;


/**
 * @brief GoogleAnalytics_Options for initializing the Analytics SDK.
 * GoogleAnalytics_Options_Create() must be used to create an instance of this
 * struct with default values. If these options are created manually instead of
 * using GoogleAnalytics_Options_Create(), initialization will fail, and the
 * caller will be responsible for destroying the options.
 */
typedef struct ANALYTICS_API GoogleAnalytics_Options {
  /**
   * @brief The unique identifier for the Firebase app across all of Firebase
   * with a platform-specific format. This is a required field, can not be null
   * or empty, and must be UTF-8 encoded.
   *
   * The caller is responsible for allocating this memory, and deallocating it
   * once the options instance has been destroyed.
   *
   * Example: 1:1234567890:android:321abc456def7890
   */
  const char* app_id;

  /**
   * @brief Unique identifier for the application implementing the SDK. The
   * format typically follows a reversed domain name convention. This is a
   * required field, can not be null or empty, and must be UTF-8 encoded.
   *
   * The caller is responsible for allocating this memory, and deallocating it
   * once the options instance has been destroyed.
   *
   * Example: com.google.analytics.AnalyticsApp
   */
  const char* package_name;

  /**
   * @brief Whether Analytics is enabled at the very first launch.
   * This value is then persisted across app sessions, and from then on, takes
   * precedence over the value of this field.
   * GoogleAnalytics_SetAnalyticsCollectionEnabled() can be used to
   * enable/disable after that point.
   */
  bool analytics_collection_enabled_at_first_launch;

  /**
   * @brief An optional path to a folder where the SDK can store its data.
   * If not provided, the SDK will store its data in the same folder as the
   * executable.
   *
   * The path must pre-exist and the app has read and write access to it.
   */
  const char* app_data_directory;

  /**
   * @brief The duration of inactivity in seconds after which a session
   * terminates.
   *
   * If a user interacts with the app after this timeout period,
   * a new session is initiated. If set to a negative value, the SDK's
   * default timeout duration is used.
   */
  int64_t session_timeout_duration_seconds;

  /**
   * @brief Reserved for internal use by the SDK.
   */
  GoogleAnalytics_Reserved* reserved;
} GoogleAnalytics_Options;

/**
 * @brief The state of an app in its lifecycle.
 */
typedef enum GoogleAnalytics_AppLifecycleState {
  /**
   * @brief This is an invalid state that is used to capture unininitialized
   * values.
   */
  GoogleAnalytics_AppLifecycleState_kUnknown = 0,
  /**
   * @brief The app is about to be terminated.
   */
  GoogleAnalytics_AppLifecycleState_kTermination = 1,
  /**
   * @brief The application has user focus (e.g., is in the foreground).
   */
  GoogleAnalytics_AppLifecycleState_kFocused = 2,
  /**
   * @brief The application does not have user focus (e.g., is in the
   * background).
   */
  GoogleAnalytics_AppLifecycleState_kUnfocused = 3,
} GoogleAnalytics_AppLifecycleState;

/**
 * @brief The log level of a log message.
 */
typedef enum GoogleAnalytics_LogLevel {
  /**
   * @brief The log message is a debug message.
   */
  GoogleAnalytics_LogLevel_kDebug = 0,
  /**
   * @brief The log message is an info message.
   */
  GoogleAnalytics_LogLevel_kInfo = 1,
  /**
   * @brief The log message is a warning message.
   */
  GoogleAnalytics_LogLevel_kWarning = 2,
  /**
   * @brief The log message is an error message.
   */
  GoogleAnalytics_LogLevel_kError = 3,
} GoogleAnalytics_LogLevel;

/**
 * @brief Function pointer type for a log callback.
 *
 * @param[in] message The log message string. Pass a value from the
 * #GoogleAnalytics_LogLevel enum.
 *
 * @note The parameters are defined as `int32_t` to ensure ABI stability across
 * different compilers, but callers should use the enum constants directly.
 *
 * @see GoogleAnalytics_LogLevel
 */
typedef void (*GoogleAnalytics_LogCallback)(int32_t log_level,
                                            const char* message);

/**
 * @brief Creates an instance of GoogleAnalytics_Options with default values.
 *
 * The caller is responsible for destroying the options using the
 * GoogleAnalytics_Options_Destroy() function, unless it has been passed to the
 * GoogleAnalytics_Initialize() function, in which case it will be destroyed
 * automatically.
 *
 * @return A pointer to a newly allocated GoogleAnalytics_Options instance.
 */
ANALYTICS_API GoogleAnalytics_Options* GoogleAnalytics_Options_Create();

/**
 * @brief Destroys the GoogleAnalytics_Options instance. Must not be called if
 * the options were created with GoogleAnalytics_Options_Create() and passed to
 * the GoogleAnalytics_Initialize() function, which would destroy them
 * automatically.
 *
 * @param[in] options The GoogleAnalytics_Options instance to destroy.
 */
ANALYTICS_API void GoogleAnalytics_Options_Destroy(
    GoogleAnalytics_Options* options);

/**
 * @brief Opaque type for an item.
 *
 * This type is an opaque object that represents an item in an item vector.
 *
 * The caller is responsible for creating the item using the
 * GoogleAnalytics_Item_Create() function, and destroying it using the
 * GoogleAnalytics_Item_Destroy() function, unless it has been added to an
 * item vector, in which case it will be destroyed at that time.
 */
typedef struct GoogleAnalytics_Item_Opaque GoogleAnalytics_Item;

/**
 * @brief Opaque type for an item vector.
 *
 * This type is an opaque object that represents a list of items. It is
 * used to pass item vectors to the
 * GoogleAnalytics_EventParameters_InsertItemVector() function.
 *
 * The caller is responsible for creating the item vector using the
 * GoogleAnalytics_ItemVector_Create() function, and destroying it using the
 * GoogleAnalytics_ItemVector_Destroy() function, unless it has been added
 * to an event parameter map, in which case it will be destroyed at that time.
 */
typedef struct GoogleAnalytics_ItemVector_Opaque GoogleAnalytics_ItemVector;

/**
 * @brief Opaque type for an event parameter map.
 *
 * This type is an opaque object that represents a dictionary of event
 * parameters. It is used to pass event parameters to the
 * GoogleAnalytics_LogEvent() function.
 *
 * The caller is responsible for creating the event parameter map using the
 * GoogleAnalytics_EventParameters_Create() function, and destroying it using
 * the GoogleAnalytics_EventParameters_Destroy() function, unless it has been
 * logged, in which case it will be destroyed automatically.
 */
typedef struct GoogleAnalytics_EventParameters_Opaque
    GoogleAnalytics_EventParameters;

/**
 * @brief Creates an item.
 *
 * The caller is responsible for destroying the item using the
 * GoogleAnalytics_Item_Destroy() function, unless it has been added to an
 * item vector, in which case it will be destroyed when it is added.
 */
ANALYTICS_API GoogleAnalytics_Item* GoogleAnalytics_Item_Create();

/**
 * @brief Inserts an int parameter into the item.
 *
 * @param[in] item The item to insert the int parameter into.
 * @param[in] key The key of the int parameter. Must be UTF-8 encoded.
 * @param[in] value The value of the int parameter.
 * @return true if the int parameter was successfully inserted, false otherwise.
 */
ANALYTICS_API bool GoogleAnalytics_Item_InsertInt(GoogleAnalytics_Item* item,
                                                  const char* key,
                                                  int64_t value);

/**
 * @brief Inserts a double parameter into the item.
 *
 * @param[in] item The item to insert the double parameter into.
 * @param[in] key The key of the double parameter. Must be UTF-8 encoded.
 * @param[in] value The value of the double parameter.
 * @return true if the double parameter was successfully inserted, false
 * otherwise.
 */
ANALYTICS_API bool GoogleAnalytics_Item_InsertDouble(GoogleAnalytics_Item* item,
                                                     const char* key,
                                                     double value);

/**
 * @brief Inserts a string parameter into the item.
 *
 * @param[in] item The item to insert the string parameter into.
 * @param[in] key The key of the string parameter. Must be UTF-8 encoded.
 * @param[in] value The value of the string parameter. Must be UTF-8 encoded.
 * @return true if the string parameter was successfully inserted, false
 * otherwise.
 */
ANALYTICS_API bool GoogleAnalytics_Item_InsertString(GoogleAnalytics_Item* item,
                                                     const char* key,
                                                     const char* value);

/**
 * @brief Destroys the item.
 *
 * The caller is responsible for destroying the item using this
 * function, unless it has been added to an item vector, in which case it
 * will be destroyed when it is added.
 *
 * @param[in] item The item to destroy.
 */
ANALYTICS_API void GoogleAnalytics_Item_Destroy(GoogleAnalytics_Item* item);

/**
 * @brief Creates an item vector.
 *
 * The caller is responsible for destroying the item vector using the
 * GoogleAnalytics_ItemVector_Destroy() function, unless it has been added
 * to an event parameter map, in which case it will be destroyed when it is
 * added.
 */
ANALYTICS_API GoogleAnalytics_ItemVector* GoogleAnalytics_ItemVector_Create();

/**
 * @brief Inserts a item into the item vector.
 *
 * @param[in] item_vector The item vector to insert the item into.
 * @param[in] item The item to insert. Automatically destroyed when added.
 * @return true if the item was successfully inserted, false otherwise.
 */
ANALYTICS_API bool GoogleAnalytics_ItemVector_InsertItem(
    GoogleAnalytics_ItemVector* item_vector, GoogleAnalytics_Item* item);

/**
 * @brief Destroys the item vector.
 *
 * The caller has the option to destroy the item vector using this function,
 * unless it has been added to an event parameter map, in which case it will be
 * destroyed when it is added.
 *
 * @param[in] item_vector The item vector to destroy.
 */
ANALYTICS_API void GoogleAnalytics_ItemVector_Destroy(
    GoogleAnalytics_ItemVector* item_vector);

/**
 * @brief Creates an event parameter map.
 *
 * The caller is responsible for destroying the event parameter map using the
 * GoogleAnalytics_EventParameters_Destroy() function, unless it has been
 * logged, in which case it will be destroyed automatically when it is logged.
 */
ANALYTICS_API GoogleAnalytics_EventParameters*
GoogleAnalytics_EventParameters_Create();

/**
 * @brief Inserts an int parameter into the event parameter map.
 *
 * @param[in] event_parameter_map The event parameter map to insert the int
 * parameter into.
 * @param[in] key The key of the int parameter. Must be UTF-8 encoded.
 * @param[in] value The value of the int parameter.
 * @return true if the int parameter was successfully inserted, false otherwise.
 */
ANALYTICS_API bool GoogleAnalytics_EventParameters_InsertInt(
    GoogleAnalytics_EventParameters* event_parameter_map, const char* key,
    int64_t value);

/**
 * @brief Inserts a double parameter into the event parameter map.
 *
 * @param[in] event_parameter_map The event parameter map to insert the double
 * parameter into.
 * @param[in] key The key of the double parameter. Must be UTF-8 encoded.
 * @param[in] value The value of the double parameter.
 * @return true if the double parameter was successfully inserted, false
 * otherwise.
 */
ANALYTICS_API bool GoogleAnalytics_EventParameters_InsertDouble(
    GoogleAnalytics_EventParameters* event_parameter_map, const char* key,
    double value);

/**
 * @brief Inserts a string parameter into the event parameter map.
 *
 * @param[in] event_parameter_map The event parameter map to insert the string
 * parameter into.
 * @param[in] key The key of the string parameter. Must be UTF-8 encoded.
 * @param[in] value The value of the string parameter. Must be UTF-8 encoded.
 * @return true if the string parameter was successfully inserted, false
 * otherwise.
 */
ANALYTICS_API bool GoogleAnalytics_EventParameters_InsertString(
    GoogleAnalytics_EventParameters* event_parameter_map, const char* key,
    const char* value);

/**
 * @brief Inserts an item vector into the event parameter map.
 *
 * @param[in] event_parameter_map The event parameter map to insert the item
 * vector into.
 * @param[in] key The key of the item vector. Must be UTF-8 encoded.
 * @param[in] value The value of the item vector. Automatically destroyed as it
 * is added.
 * @return true if the item vector was successfully inserted, false otherwise.
 */
ANALYTICS_API bool GoogleAnalytics_EventParameters_InsertItemVector(
    GoogleAnalytics_EventParameters* event_parameter_map, const char* key,
    GoogleAnalytics_ItemVector* value);

/**
 * @brief Destroys the event parameter map.
 *
 * The caller is responsible for destroying the event parameter map using this
 * function. Unless it has been logged, in which case it will be destroyed
 * automatically when it is logged.
 *
 * @param[in] event_parameter_map The event parameter map to destroy.
 */
ANALYTICS_API void GoogleAnalytics_EventParameters_Destroy(
    GoogleAnalytics_EventParameters* event_parameter_map);

/**
 * @brief Initializes the Analytics SDK. Until this is called, all analytics
 * functions below will be no-ops.
 *
 * @param[in] options The options for initializing the Analytics
 * SDK. Deleted regardless of return value, if it was allocated with the
 * GoogleAnalytics_Options_Create() function.
 * @return true if the Analytics SDK was successfully initialized, false
 * otherwise. Also returns false if the Analytics SDK has already been
 * initialized.
 */
ANALYTICS_API bool GoogleAnalytics_Initialize(GoogleAnalytics_Options* options);

/**
 * @brief Returns whether the Analytics SDK is initialized.
 *
 * @return true if the Analytics SDK is initialized, false otherwise.
 */
ANALYTICS_API bool GoogleAnalytics_IsInitialized();

/**
 * @brief Sets whether debug mode is enabled.
 *
 * @param[in] enabled A flag that enables or disables debug mode.
 */
ANALYTICS_API void GoogleAnalytics_SetDebugMode(bool enabled);

/**
 * @brief Logs an app event.
 *
 * The event can have up to 25 parameters. Events with the same name must have
 * the same parameters. Up to 500 event names are supported. Using predefined
 * events and/or parameters is recommended for optimal reporting.
 *
 * The following event names are reserved and cannot be used:
 * - ad_activeview
 * - ad_click
 * - ad_exposure
 * - ad_query
 * - ad_reward
 * - adunit_exposure
 * - app_clear_data
 * - app_exception
 * - app_remove
 * - app_store_refund
 * - app_store_subscription_cancel
 * - app_store_subscription_convert
 * - app_store_subscription_renew
 * - app_update
 * - app_upgrade
 * - dynamic_link_app_open
 * - dynamic_link_app_update
 * - dynamic_link_first_open
 * - error
 * - firebase_campaign
 * - first_open
 * - first_visit
 * - in_app_purchase
 * - notification_dismiss
 * - notification_foreground
 * - notification_open
 * - notification_receive
 * - os_update
 * - session_start
 * - session_start_with_rollout
 * - user_engagement
 *
 * @param[in] name The name of the event. Should contain 1 to 40 alphanumeric
 * characters or underscores. The name must start with an alphabetic
 * character. Some event names are reserved. See event_names.h for the list
 * of reserved event names. The "firebase_", "google_", and "ga_" prefixes are
 * reserved and should not be used. Note that event names are case-sensitive
 * and that logging two events whose names differ only in case will result in
 * two distinct events. To manually log screen view events, use the
 * `screen_view` event name. Must be UTF-8 encoded.
 * @param[in] parameters The map of event parameters. Passing `nullptr`
 * indicates that the event has no parameters. Parameter names can be up to 40
 * characters long and must start with an alphabetic character and contain
 * only alphanumeric characters and underscores. Only String, Int, and Double
 * parameter types are supported. String parameter values can be up to 100
 * characters long for standard Google Analytics properties, and up to 500
 * characters long for Google Analytics 360 properties. The "firebase_",
 * "google_", and "ga_" prefixes are reserved and should not be used for
 * parameter names. The parameter map must be created using the
 * GoogleAnalytics_EventParameters_Create() function. Automatically destroyed
 * when it is logged.
 */
ANALYTICS_API void GoogleAnalytics_LogEvent(
    const char* name, GoogleAnalytics_EventParameters* parameters);

/**
 * @brief Sets a user property to a given value.
 *
 * Up to 25 user property names are supported. Once set, user property values
 * persist throughout the app lifecycle and across sessions.
 *
 * The following user property names are reserved and cannot be used:
 *
 *  - first_open_time
 *  - last_deep_link_referrer
 *  - user_id
 *
 * @param[in] name The name of the user property to set. Should contain 1 to 24
 * alphanumeric characters or underscores, and must start with an alphabetic
 * character. The "firebase_", "google_", and "ga_" prefixes are reserved and
 * should not be used for user property names. Must be UTF-8 encoded.
 * @param[in] value The value of the user property. Values can be up to 36
 * characters long. Setting the value to `nullptr` remove the user property.
 * Must be UTF-8 encoded.
 */
ANALYTICS_API void GoogleAnalytics_SetUserProperty(const char* name,
                                                   const char* value);

/**
 * @brief Sets the user ID property.
 *
 * This feature must be used in accordance with
 * <a href="https://www.google.com/policies/privacy">Google's Privacy
 * Policy</a>
 *
 * @param[in] user_id The user ID associated with the user of this app on this
 * device. The user ID must be non-empty and no more than 256 characters long,
 * and UTF-8 encoded. Setting user_id to nullptr removes the user ID.
 */
ANALYTICS_API void GoogleAnalytics_SetUserId(const char* user_id);

/**
 * @brief Clears all analytics data for this instance from the device and resets
 * the app instance ID.
 */
ANALYTICS_API void GoogleAnalytics_ResetAnalyticsData();

/**
 * @brief Sets whether analytics collection is enabled for this app on this
 * device.
 *
 * This setting is persisted across app sessions. By default it is enabled.
 *
 * @param[in] enabled A flag that enables or disables Analytics collection.
 */
ANALYTICS_API void GoogleAnalytics_SetAnalyticsCollectionEnabled(bool enabled);

/**
 * @brief Allows the passing of a callback to be used when the SDK logs any
 * messages regarding its behavior. The callback must be thread-safe.
 *
 * @param[in] callback The callback to use. Passing `nullptr` removes the
 * currently set callback.
 */
ANALYTICS_API void GoogleAnalytics_SetLogCallback(
    GoogleAnalytics_LogCallback callback);

/**
 * @brief Sets the duration of inactivity in seconds after which a session
 * terminates.
 *
 * If a user interacts with the app after this timeout period, a new session is
 * initiated. If set to a negative value, the value is ignored. The default
 * value is 1800 seconds (30 minutes).
 *
 * @param[in] session_timeout_duration_seconds The session timeout duration in
 * seconds.
 */
ANALYTICS_API void GoogleAnalytics_SetSessionTimeoutInterval(
    int64_t session_timeout_duration_seconds);

/**
 * @brief Notifies the current state of the app's lifecycle.
 *
 * This method is used to notify the Analytics SDK about the current state of
 * the app's lifecycle. The Analytics SDK will use this information to log
 * events, update user properties, upload data, etc.
 *
 * GoogleAnalytics_AppLifecycleState_kTermination is used to indicate that the
 * app is about to be terminated. The caller will be blocked until all pending
 * data is uploaded or an error occurs. The caller must ensure the OS does not
 * terminate background threads before the call returns.
 *
 * @param[in] state The current state of the app's lifecycle. Pass a value from
 * the #GoogleAnalytics_AppLifecycleState enum.
 *
 * @note The parameters are defined as `int32_t` to ensure ABI stability across
 * different compilers, but callers should use the enum constants directly.
 *
 * @see GoogleAnalytics_AppLifecycleState
 */
ANALYTICS_API void GoogleAnalytics_NotifyAppLifecycleChange(int32_t state);


#ifdef __cplusplus
}
#endif

#endif  // ANALYTICS_MOBILE_CONSOLE_MEASUREMENT_PRERELEASE_C_ANALYTICS_H_
