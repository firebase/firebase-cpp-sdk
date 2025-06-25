// Copyright 2025 Google LLC

#ifndef ANALYTICS_MOBILE_CONSOLE_MEASUREMENT_PUBLIC_ANALYTICS_H_
#define ANALYTICS_MOBILE_CONSOLE_MEASUREMENT_PUBLIC_ANALYTICS_H_

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "c/analytics.h"

namespace google::analytics {

/**
 * The top level Firebase Analytics singleton that provides methods for logging
 * events and setting user properties. See <a href="http://goo.gl/gz8SLz">the
 * developer guides</a> for general information on using Firebase Analytics in
 * your apps.
 *
 * @note The Analytics SDK uses SQLite to persist events and other app-specific
 * data. Calling certain thread-unsafe global SQLite methods like
 * `sqlite3_shutdown()` can result in unexpected crashes at runtime.
 */
class Analytics {
 public:
  using PrimitiveValue = std::variant<int64_t, double, std::string>;
  using Item = std::unordered_map<std::string, PrimitiveValue>;
  using ItemVector = std::vector<Item>;
  using EventParameterValue =
      std::variant<int64_t, double, std::string, ItemVector>;
  using EventParameters = std::unordered_map<std::string, EventParameterValue>;

  /**
   * @brief Options for initializing the Analytics SDK.
   */
  struct Options {
    /**
     * @brief The unique identifier for the Firebase app across all of Firebase
     * with a platform-specific format. This is a required field, can not be
     * empty, and must be UTF-8 encoded.
     *
     * Example: 1:1234567890:android:321abc456def7890
     */
    std::string app_id;

    /**
     * @brief Unique identifier for the application implementing the SDK. The
     * format typically follows a reversed domain name convention. This is a
     * required field, can not be empty, and must be UTF-8 encoded.
     *
     * Example: com.google.analytics.AnalyticsApp
     */
    std::string package_name;

    /**
     * @brief Whether Analytics is enabled at the very first launch.
     * This value is then persisted across app sessions, and from then on, takes
     * precedence over the value of this field.
     * SetAnalyticsCollectionEnabled() can be used to enable/disable after that
     * point.
     */
    bool analytics_collection_enabled_at_first_launch = true;
  };

  /**
   * @brief Returns the singleton instance of the Analytics class.
   */
  static Analytics& GetInstance() {
    static Analytics instance;
    return instance;
  }

  // This type is neither copyable nor movable.
  Analytics(const Analytics&) = delete;
  Analytics& operator=(const Analytics&) = delete;
  Analytics(Analytics&&) = delete;
  Analytics& operator=(Analytics&&) = delete;

  /**
   * @brief Initializes the Analytics SDK with the given options. Until this is
   * called, all analytics methods below will be no-ops.
   *
   * @param[in] options The options to initialize the Analytics SDK with.
   *
   * @return true if the Analytics SDK was successfully initialized, false
   * otherwise. Also returns false if the Analytics SDK has already been
   * initialized.
   */
  bool Initialize(const Options& options) {
    GoogleAnalytics_Options* google_analytics_options =
        GoogleAnalytics_Options_Create();
    google_analytics_options->app_id = options.app_id.c_str();
    google_analytics_options->package_name = options.package_name.c_str();
    google_analytics_options->analytics_collection_enabled_at_first_launch =
        options.analytics_collection_enabled_at_first_launch;
    return GoogleAnalytics_Initialize(google_analytics_options);
  }

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
   * @param[in] parameters The map of event parameters. Passing `std::nullopt`
   * indicates that the event has no parameters. Parameter names can be up to 40
   * characters long and must start with an alphabetic character and contain
   * only alphanumeric characters and underscores. Only String, Int, and Double
   * parameter types are supported. String parameter values can be up to 100
   * characters long for standard Google Analytics properties, and up to 500
   * characters long for Google Analytics 360 properties. The "firebase_",
   * "google_", and "ga_" prefixes are reserved and should not be used for
   * parameter names. String keys and values must be UTF-8 encoded.
   */
  void LogEvent(const std::string& event_name,
                const std::optional<const EventParameters>& parameters) {
    if (!parameters.has_value()) {
      GoogleAnalytics_LogEvent(std::string(event_name).c_str(), nullptr);
      return;
    }
    GoogleAnalytics_EventParameters* map =
        GoogleAnalytics_EventParameters_Create();
    for (const auto& [name, value] : parameters.value()) {
      if (auto* int_value = std::get_if<int64_t>(&value)) {
        GoogleAnalytics_EventParameters_InsertInt(map, name.c_str(),
                                                  *int_value);
      } else if (auto* double_value = std::get_if<double>(&value)) {
        GoogleAnalytics_EventParameters_InsertDouble(map, name.c_str(),
                                                     *double_value);
      } else if (auto* string_value = std::get_if<std::string>(&value)) {
        GoogleAnalytics_EventParameters_InsertString(map, name.c_str(),
                                                     string_value->c_str());
      } else if (auto* items = std::get_if<ItemVector>(&value)) {
        GoogleAnalytics_ItemVector* item_vector =
            GoogleAnalytics_ItemVector_Create();
        for (const auto& item : *items) {
          GoogleAnalytics_Item* nested_item = GoogleAnalytics_Item_Create();
          for (const auto& [nested_name, nested_value] : item) {
            if (auto* nested_int_value = std::get_if<int64_t>(&nested_value)) {
              GoogleAnalytics_Item_InsertInt(nested_item, nested_name.c_str(),
                                             *nested_int_value);
            } else if (auto* nested_double_value =
                           std::get_if<double>(&nested_value)) {
              GoogleAnalytics_Item_InsertDouble(
                  nested_item, nested_name.c_str(), *nested_double_value);
            } else if (auto* nested_string_value =
                           std::get_if<std::string>(&nested_value)) {
              GoogleAnalytics_Item_InsertString(nested_item,
                                                nested_name.c_str(),
                                                nested_string_value->c_str());
            }
          }
          GoogleAnalytics_ItemVector_InsertItem(item_vector, nested_item);
        }
        GoogleAnalytics_EventParameters_InsertItemVector(map, name.c_str(),
                                                         item_vector);
      }
    }
    GoogleAnalytics_LogEvent(std::string(event_name).c_str(), map);
  }

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
   * @param[in] name The name of the user property to set. Should contain 1 to
   * 24 alphanumeric characters or underscores, and must start with an
   * alphabetic character. The "firebase_", "google_", and "ga_" prefixes are
   * reserved and should not be used for user property names. Must be UTF-8
   * encoded.
   * @param[in] value The value of the user property. Values can be up to 36
   * characters long. Setting the value to `std::nullopt` removes the user
   * property. Must be UTF-8 encoded.
   */
  void SetUserProperty(const std::string& name,
                       std::optional<std::string> value) {
    const char* value_ptr = value.has_value() ? value->c_str() : nullptr;
    GoogleAnalytics_SetUserProperty(name.c_str(), value_ptr);
  }

  /**
   * @brief Sets the user ID property.
   *
   * This feature must be used in accordance with
   * <a href="https://www.google.com/policies/privacy">Google's Privacy
   * Policy</a>
   *
   * @param[in] user_id The user ID associated with the user of this app on this
   * device. The user ID must be non-empty and no more than 256 characters
   * long, and UTF-8 encoded. Setting user_id to std::nullopt removes the
   * user ID.
   */
  void SetUserId(std::optional<std::string> user_id) {
    if (!user_id.has_value()) {
      GoogleAnalytics_SetUserId(nullptr);
    } else {
      GoogleAnalytics_SetUserId(user_id->c_str());
    }
  }

  /**
   * @brief Clears all analytics data for this instance from the device and
   * resets the app instance ID.
   */
  void ResetAnalyticsData() { GoogleAnalytics_ResetAnalyticsData(); }

  /**
   * @brief Sets whether analytics collection is enabled for this app on this
   * device.
   *
   * This setting is persisted across app sessions. By default it is enabled.
   *
   * @param[in] enabled A flag that enables or disables Analytics collection.
   */
  void SetAnalyticsCollectionEnabled(bool enabled) {
    GoogleAnalytics_SetAnalyticsCollectionEnabled(enabled);
  }

 private:
  Analytics() = default;
};

}  // namespace google::analytics

#endif  // ANALYTICS_MOBILE_CONSOLE_MEASUREMENT_PUBLIC_ANALYTICS_H_
