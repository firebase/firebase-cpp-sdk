// Copyright 2016 Google LLC
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

#ifndef FIREBASE_REMOTE_CONFIG_SRC_INCLUDE_FIREBASE_REMOTE_CONFIG_H_
#define FIREBASE_REMOTE_CONFIG_SRC_INCLUDE_FIREBASE_REMOTE_CONFIG_H_

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "firebase/app.h"
#include "firebase/future.h"
#include "firebase/internal/common.h"
#include "firebase/internal/platform.h"
#include "firebase/remote_config/config_update_listener_registration.h"
#ifndef SWIG
#include "firebase/variant.h"
#endif  // SWIG

#if !defined(DOXYGEN) && !defined(SWIG)
FIREBASE_APP_REGISTER_CALLBACKS_REFERENCE(remote_config)
#endif  // !defined(DOXYGEN) && !defined(SWIG)

namespace firebase {

#ifndef SWIG
/// @brief Firebase Remote Config API.
///
/// Firebase Remote Config is a cloud service that lets you change the
/// appearance and behavior of your app without requiring users to download an
/// app update.
#endif  // SWIG
namespace remote_config {

namespace internal {
class RemoteConfigInternal;
}  // namespace internal

/// @brief Describes the most recent fetch request status.
enum LastFetchStatus {
  /// The most recent fetch was a success, and its data is ready to be
  /// applied, if you have not already done so.
  kLastFetchStatusSuccess,
  /// The most recent fetch request failed.
  kLastFetchStatusFailure,
  /// The RemoteConfig instance has not yet fetched data from the server, or
  /// the most recent fetch is still in progress.
  kLastFetchStatusPending,
};

/// @brief Describes the most recent fetch failure.
enum FetchFailureReason {
  /// The fetch has not yet failed.
  kFetchFailureReasonInvalid,
  /// The most recent fetch failed because it exceeded the throttle limit.
  kFetchFailureReasonThrottled,
  /// The most recent fetch failed for an unknown reason.
  kFetchFailureReasonError,
};

/// @brief Describes the state of the most recent `Fetch()` call.
/// Returned by `RemoteConfig::GetInfo()`.
struct ConfigInfo {
  /// The time (in milliseconds since the epoch) that the last fetch
  /// operation completed.
  uint64_t fetch_time;

  /// The status of the last fetch request.
  LastFetchStatus last_fetch_status;

  /// The reason the most recent fetch failed.
  FetchFailureReason last_fetch_failure_reason;

  /// The time (in milliseconds since the epoch) when throttling of Remote
  /// Config fetches ends and refreshing can resume.
  uint64_t throttled_end_time;
};

/// @brief Defines the source of a value.
enum ValueSource {
  /// The value was not specified and no default was specified, so a static
  /// value (`0` for numeric values, `false` for booleans, `""` for strings,
  /// or an empty vector for binary data) was returned.
  kValueSourceStaticValue,
  /// The value was found in the activated remote data store and returned.
  kValueSourceRemoteValue,
  /// The key was not found in the remote data store, so the specified default
  /// value was returned instead.
  kValueSourceDefaultValue,
};

/// @brief Contains information about the source of a value of a requested key.
/// Returned alongside the value by the `Get<Type>(key, info)` functions.
struct ValueInfo {
  /// Where the returned value was retrieved from (`kValueSourceRemoteValue`,
  /// `kValueSourceDefaultValue`, or `kValueSourceStaticValue`).
  ValueSource source;

  /// If `true`, this indicates that the returned value was able to be converted
  /// to the requested type. If `false`, conversion failed, so a static empty
  /// value was returned instead.
  bool conversion_successful;
};

/// @brief Legacy configuration setting keys retained for internal metadata
/// serialization compatibility.
enum ConfigSetting {
  /// Set the value associated with this key to "1" to enable developer mode
  /// (i.e. disable throttling) and "0" to disable.
  kConfigSettingDeveloperMode,
};

/// @brief The default cache expiration used by Fetch(), equal to 12 hours,
/// in milliseconds.
static const uint64_t kDefaultCacheExpiration = 60 * 60 * 12 * 1000;

/// @brief The default timeout used by Fetch(), equal to 30 seconds,
/// in milliseconds.
static const uint64_t kDefaultTimeoutInMilliseconds = 30 * 1000;

/// @brief Settings for Remote Config operations.
struct ConfigSettings {
  /// The timeout in milliseconds specifying how long the client should wait for
  /// a connection to the Firebase Remote Config server. Default is 30 seconds
  /// (`kDefaultTimeoutInMilliseconds`).
  uint64_t fetch_timeout_in_milliseconds = kDefaultTimeoutInMilliseconds;

  /// The minimum interval in milliseconds between successive fetch calls.
  /// Fetches less than `minimum_fetch_interval_in_milliseconds` after the last
  /// fetch from the Firebase Remote Config server use values returned during
  /// the last fetch. Default is 12 hours (`kDefaultCacheExpiration`).
  uint64_t minimum_fetch_interval_in_milliseconds = kDefaultCacheExpiration;
};

/// @brief Describes a mapping of a key to a string value. Used to set default
/// values.
struct ConfigKeyValue {
  /// The lookup key string.
  ///
  /// @note Ensure this string stays valid for the duration of the call to
  /// `SetDefaults()`.
  const char* key;
  /// The value string to be stored.
  ///
  /// @note Ensure this string stays valid for the duration of the call to
  /// `SetDefaults()`.
  const char* value;
};

#ifndef SWIG
/// @brief Describes a mapping of a key to a value of any type. Used to set
/// default values.
struct ConfigKeyValueVariant {
  /// The lookup key string.
  ///
  /// @note Ensure this string stays valid for the duration of the call to
  /// `SetDefaults()`.
  const char* key;
  /// The value `Variant` to be stored.
  Variant value;
};
#endif  // SWIG

/// @brief Information about the updated config, passed to the
/// `AddOnConfigUpdateListener()` callback.
struct ConfigUpdate {
  /// Parameter keys whose values have been updated from the currently activated
  /// values. Includes keys that are added, deleted, or whose value, value
  /// source, or metadata has changed.
  std::vector<std::string> updated_keys;
};

/// @brief Error codes passed to the `AddOnConfigUpdateListener()` real-time
/// config update callback.
enum RemoteConfigError {
  /// Unimplemented error. Real-time config updates are not supported on
  /// Desktop.
  kRemoteConfigErrorUnimplemented = -1,
  /// No error.
  kRemoteConfigErrorNone = 0,
  /// Unable to make a connection to the Remote Config backend for config
  /// updates.
  kRemoteConfigErrorConfigUpdateStreamError,
  /// The ConfigUpdate message was unparsable.
  kRemoteConfigErrorConfigUpdateMessageInvalid,
  /// Unable to fetch the latest version of the config.
  kRemoteConfigErrorConfigUpdateNotFetched,
  /// The Remote Config real-time config update service is unavailable.
  kRemoteConfigErrorConfigUpdateUnavailable,
};

#ifndef SWIG
/// @brief Entry point for the Firebase C++ SDK for Remote Config.
///
/// To use the SDK, call `firebase::remote_config::RemoteConfig::GetInstance()`
/// to obtain an instance of `RemoteConfig`, then call `Fetch()` / `Activate()`
/// and `GetBoolean()` / `GetLong()` / `GetDouble()` / `GetString()` /
/// `GetData()`.
#endif  // SWIG
class RemoteConfig {
 public:
  ~RemoteConfig();

  RemoteConfig(const RemoteConfig&) = delete;
  RemoteConfig& operator=(const RemoteConfig&) = delete;

  /// @brief Get the `RemoteConfig` instance for the default `firebase::App`.
  ///
  /// @returns An instance of `RemoteConfig` corresponding to the default `App`,
  /// or `nullptr` if the default `App` does not exist.
  static RemoteConfig* GetInstance() {
    App* default_app = App::GetInstance();
    return default_app ? GetInstance(default_app) : nullptr;
  }

  /// @brief Get an instance of `RemoteConfig` corresponding to the given `App`.
  ///
  /// If you call `GetInstance()` multiple times with the same `App`, you will
  /// get the same instance of `RemoteConfig`. On Desktop, cached configuration
  /// state is loaded synchronously from persistent storage during
  /// `GetInstance()`.
  ///
  /// @param[in] app Your instance of `firebase::App`.
  ///
  /// @returns An instance of `RemoteConfig` corresponding to the given `App`,
  /// or `nullptr` if `app` is null or initialization fails.
  static RemoteConfig* GetInstance(App* app);

  /// @brief Get the `firebase::App` that this `RemoteConfig` was created with.
  ///
  /// @returns The `firebase::App` this `RemoteConfig` was created with.
  App* app() { return app_; }
  App* app() const { return app_; }

  /// @brief Async initialization of the `RemoteConfig` instance.
  ///
  /// Creates a `Future` that resolves once the fetched, activated, and default
  /// configs have been loaded from persistent storage.
  ///
  /// @returns A `Future<ConfigInfo>` containing the `ConfigInfo` of the
  /// initialized instance upon completion.
  Future<ConfigInfo> EnsureInitialized();

  /// @brief Get the (possibly still pending) results of the most recent
  /// `EnsureInitialized()` call.
  ///
  /// @return The future result from the last call to `EnsureInitialized()`.
  Future<ConfigInfo> EnsureInitializedLastResult();
  Future<ConfigInfo> EnsureInitializedLastResult() const;

  /// @brief Asynchronously activates the most recently fetched configs, so that
  /// the fetched key-value pairs take effect.
  ///
  /// @return A `Future<bool>` that resolves to `true` if newly fetched configs
  /// were activated, or `false` if the fetched configs were already active.
  Future<bool> Activate();

  /// @brief Get the (possibly still pending) results of the most recent
  /// `Activate()` call.
  ///
  /// @return The future result from the last call to `Activate()`.
  Future<bool> ActivateLastResult();
  Future<bool> ActivateLastResult() const;

  /// @brief Asynchronously fetches and then activates the fetched configs.
  ///
  /// If the time elapsed since the last fetch from the Firebase Remote Config
  /// backend is more than the configured
  /// `ConfigSettings::minimum_fetch_interval_in_milliseconds`, configs are
  /// fetched from the backend. After the fetch completes, the configs are
  /// activated so that the fetched key-value pairs take effect.
  ///
  /// @return A `Future<bool>` that resolves to `true` if the call activated new
  /// fetched configs, or `false` if no new configs were activated.
  Future<bool> FetchAndActivate();

  /// @brief Get the (possibly still pending) results of the most recent
  /// `FetchAndActivate()` call.
  ///
  /// @return The future result from the last call to `FetchAndActivate()`.
  Future<bool> FetchAndActivateLastResult();
  Future<bool> FetchAndActivateLastResult() const;

  /// @brief Fetches config data from the server using the configured
  /// `GetConfigSettings().minimum_fetch_interval_in_milliseconds` (converted to
  /// whole seconds).
  ///
  /// @note This does not apply the data or make it accessible; to access newly
  /// retrieved values, call `Activate()`.
  ///
  /// @return A `Future<void>` which can be used to determine when the fetch is
  /// complete.
  Future<void> Fetch();

  /// @brief Fetches config data from the server.
  ///
  /// @note This does not apply the data or make it accessible; to access newly
  /// retrieved values, call `Activate()`.
  ///
  /// @param[in] cache_expiration_in_seconds The number of seconds to keep
  /// previously fetched data available. If cached data is available that is
  /// newer than `cache_expiration_in_seconds`, then the function returns
  /// immediately and does not fetch any data. A `cache_expiration_in_seconds`
  /// of zero will always cause a fetch.
  ///
  /// @return A `Future<void>` which can be used to determine when the fetch is
  /// complete.
  Future<void> Fetch(uint64_t cache_expiration_in_seconds);

  /// @brief Get the (possibly still pending) results of the most recent
  /// `Fetch()` call.
  ///
  /// @return The future result from the last call to `Fetch()`.
  Future<void> FetchLastResult();
  Future<void> FetchLastResult() const;

#if defined(__ANDROID__) || defined(DOXYGEN)
  /// @brief Sets the default values based on an Android resource ID.
  ///
  /// This completely overrides all previous default values.
  ///
  /// @param[in] defaults_resource_id ID for the XML resource in `res/xml`.
  ///
  /// @return A `Future<void>` which can be used to determine when the operation
  /// is complete.
  Future<void> SetDefaults(int defaults_resource_id);
#endif  // defined(__ANDROID__) || defined(DOXYGEN)

#ifndef SWIG
  /// @brief Sets the default values based on an array of
  /// `ConfigKeyValueVariant`.
  ///
  /// The type of each `Variant` in the array is used to interpret the default
  /// value to store, which can then be retrieved via `GetBoolean()`,
  /// `GetLong()`, `GetDouble()`, `GetString()`, or `GetData()`.
  ///
  /// This completely overrides all previous default values.
  ///
  /// @param[in] defaults Array of `ConfigKeyValueVariant` representing the new
  /// set of defaults to apply. If the same key is specified multiple times, the
  /// value associated with the last duplicate key is applied.
  /// @param[in] number_of_defaults Number of elements in the `defaults` array.
  ///
  /// @return A `Future<void>` which can be used to determine when the operation
  /// is complete.
  Future<void> SetDefaults(const ConfigKeyValueVariant* defaults,
                           size_t number_of_defaults);
#endif  // SWIG

  /// @brief Sets the default values based on an array of string key-value
  /// pairs.
  ///
  /// This completely overrides all previous default values.
  ///
  /// @param[in] defaults Array of `ConfigKeyValue` representing the new set of
  /// defaults to apply. If the same key is specified multiple times, the value
  /// associated with the last duplicate key is applied.
  /// @param[in] number_of_defaults Number of elements in the `defaults` array.
  ///
  /// @return A `Future<void>` which can be used to determine when the operation
  /// is complete.
  Future<void> SetDefaults(const ConfigKeyValue* defaults,
                           size_t number_of_defaults);

  /// @brief Get the (possibly still pending) results of the most recent
  /// `SetDefaults()` call.
  ///
  /// @return The future result from the last call to `SetDefaults()`.
  Future<void> SetDefaultsLastResult();
  Future<void> SetDefaultsLastResult() const;

  /// @brief Asynchronously changes the settings for this `RemoteConfig`
  /// instance.
  ///
  /// @note On Android and iOS, millisecond fields in `ConfigSettings` are
  /// converted to whole seconds when passed to the underlying platform SDK.
  ///
  /// @param[in] settings The new settings to be applied.
  ///
  /// @return A `Future<void>` which can be used to determine when the operation
  /// is complete.
  Future<void> SetConfigSettings(ConfigSettings settings);

  /// @brief Gets the current settings of the `RemoteConfig` object.
  ///
  /// @return A `ConfigSettings` struct containing the current settings.
  ConfigSettings GetConfigSettings();
  ConfigSettings GetConfigSettings() const;

  /// @brief Get the (possibly still pending) results of the most recent
  /// `SetConfigSettings()` call.
  ///
  /// @return The future result from the last call to `SetConfigSettings()`.
  Future<void> SetConfigSettingsLastResult();
  Future<void> SetConfigSettingsLastResult() const;

  /// @brief Returns the value associated with a key, converted to a `bool`.
  ///
  /// Values of `"1"`, `"true"`, `"t"`, `"yes"`, `"y"`, and `"on"` are
  /// interpreted as `true`, and `"0"`, `"false"`, `"f"`, `"no"`, `"n"`,
  /// `"off"`, and empty strings are interpreted as `false`.
  ///
  /// @param[in] key The key of the value to be retrieved.
  ///
  /// @return The boolean value associated with the specified key, or `false` if
  /// the key is missing or cannot be converted to a `bool`.
  bool GetBoolean(const char* key);
  bool GetBoolean(const char* key) const;
  bool GetBoolean(const std::string& key);
  bool GetBoolean(const std::string& key) const {
    return GetBoolean(key.c_str());
  }

  /// @brief Returns the value associated with a key, converted to a `bool`.
  ///
  /// @param[in] key The key of the value to be retrieved.
  /// @param[out] info Optional pointer receiving the source and conversion
  /// status of the returned value (may be `nullptr`).
  ///
  /// @return The boolean value associated with the specified key, or `false` if
  /// the key is missing or conversion fails.
  bool GetBoolean(const char* key, ValueInfo* info);
  bool GetBoolean(const char* key, ValueInfo* info) const;
  bool GetBoolean(const std::string& key, ValueInfo* info);
  bool GetBoolean(const std::string& key, ValueInfo* info) const {
    return GetBoolean(key.c_str(), info);
  }

  /// @brief Returns the value associated with a key, converted to a 64-bit
  /// integer.
  ///
  /// @param[in] key The key of the value to be retrieved.
  ///
  /// @return The 64-bit integer value associated with the specified key, or `0`
  /// if the key is missing or cannot be converted to a 64-bit integer.
  int64_t GetLong(const char* key);
  int64_t GetLong(const char* key) const;
  int64_t GetLong(const std::string& key);
  int64_t GetLong(const std::string& key) const { return GetLong(key.c_str()); }

  /// @brief Returns the value associated with a key, converted to a 64-bit
  /// integer.
  ///
  /// @param[in] key The key of the value to be retrieved.
  /// @param[out] info Optional pointer receiving the source and conversion
  /// status of the returned value (may be `nullptr`).
  ///
  /// @return The 64-bit integer value associated with the specified key, or `0`
  /// if the key is missing or conversion fails.
  int64_t GetLong(const char* key, ValueInfo* info);
  int64_t GetLong(const char* key, ValueInfo* info) const;
  int64_t GetLong(const std::string& key, ValueInfo* info);
  int64_t GetLong(const std::string& key, ValueInfo* info) const {
    return GetLong(key.c_str(), info);
  }

  /// @brief Returns the value associated with a key, converted to a `double`.
  ///
  /// @param[in] key The key of the value to be retrieved.
  ///
  /// @return The `double` value associated with the specified key, or `0.0` if
  /// the key is missing or cannot be converted to a `double`.
  double GetDouble(const char* key);
  double GetDouble(const char* key) const;
  double GetDouble(const std::string& key);
  double GetDouble(const std::string& key) const {
    return GetDouble(key.c_str());
  }

  /// @brief Returns the value associated with a key, converted to a `double`.
  ///
  /// @param[in] key The key of the value to be retrieved.
  /// @param[out] info Optional pointer receiving the source and conversion
  /// status of the returned value (may be `nullptr`).
  ///
  /// @return The `double` value associated with the specified key, or `0.0` if
  /// the key is missing or conversion fails.
  double GetDouble(const char* key, ValueInfo* info);
  double GetDouble(const char* key, ValueInfo* info) const;
  double GetDouble(const std::string& key, ValueInfo* info);
  double GetDouble(const std::string& key, ValueInfo* info) const {
    return GetDouble(key.c_str(), info);
  }

  /// @brief Returns the value associated with a key, converted to a string.
  ///
  /// @param[in] key The key of the value to be retrieved.
  ///
  /// @return Value as a string, or `""` if the key was not found.
  std::string GetString(const char* key);
  std::string GetString(const char* key) const;
  std::string GetString(const std::string& key);
  std::string GetString(const std::string& key) const {
    return GetString(key.c_str());
  }

  /// @brief Returns the value associated with a key, converted to a string.
  ///
  /// @param[in] key The key of the value to be retrieved.
  /// @param[out] info Optional pointer receiving the source and conversion
  /// status of the returned value (may be `nullptr`).
  ///
  /// @return Value as a string, or `""` if the key was not found.
  std::string GetString(const char* key, ValueInfo* info);
  std::string GetString(const char* key, ValueInfo* info) const;
  std::string GetString(const std::string& key, ValueInfo* info);
  std::string GetString(const std::string& key, ValueInfo* info) const {
    return GetString(key.c_str(), info);
  }

  /// @brief Returns the value associated with a key as a vector of raw bytes.
  ///
  /// @param[in] key The key of the value to be retrieved.
  ///
  /// @return Vector of bytes, or an empty vector if the key was not found.
  std::vector<unsigned char> GetData(const char* key);
  std::vector<unsigned char> GetData(const char* key) const;
  std::vector<unsigned char> GetData(const std::string& key);
  std::vector<unsigned char> GetData(const std::string& key) const {
    return GetData(key.c_str());
  }

  /// @brief Returns the value associated with a key as a vector of raw bytes.
  ///
  /// @param[in] key The key of the value to be retrieved.
  /// @param[out] info Optional pointer receiving the source and conversion
  /// status of the returned value (may be `nullptr`).
  ///
  /// @return Vector of bytes, or an empty vector if the key was not found.
  std::vector<unsigned char> GetData(const char* key, ValueInfo* info);
  std::vector<unsigned char> GetData(const char* key, ValueInfo* info) const;
  std::vector<unsigned char> GetData(const std::string& key, ValueInfo* info);
  std::vector<unsigned char> GetData(const std::string& key,
                                     ValueInfo* info) const {
    return GetData(key.c_str(), info);
  }

  /// @brief Gets the list of keys that start with the given prefix.
  ///
  /// @param[in] prefix The key prefix to look for. If empty or `nullptr`, this
  /// method returns all keys.
  ///
  /// @return Vector of keys that start with the specified prefix.
  std::vector<std::string> GetKeysByPrefix(const char* prefix);
  std::vector<std::string> GetKeysByPrefix(const char* prefix) const;
  std::vector<std::string> GetKeysByPrefix(const std::string& prefix);
  std::vector<std::string> GetKeysByPrefix(const std::string& prefix) const {
    return GetKeysByPrefix(prefix.c_str());
  }

  /// @brief Gets the list of all keys.
  ///
  /// @return Vector of all keys.
  std::vector<std::string> GetKeys();
  std::vector<std::string> GetKeys() const;

  /// @brief Returns a map of Firebase Remote Config key-value pairs.
  ///
  /// Evaluates the values of the parameters in the following order:
  /// 1. The activated value, if the last successful `Activate()` contained the
  ///    key.
  /// 2. The default value, if the key was set with `SetDefaults()`.
  ///
  /// Each value string is coerced into a `Variant` (`Int64`, `Double`, `Bool`,
  /// or `MutableString` based on its lexical representation).
  ///
  /// @return A map of key/`Variant` pairs.
  std::map<std::string, Variant> GetAll();
  std::map<std::string, Variant> GetAll() const;

  /// @brief Returns information about the last fetch request, in the form
  /// of a `ConfigInfo` struct.
  ///
  /// @return A `ConfigInfo` struct.
  const ConfigInfo GetInfo();
  const ConfigInfo GetInfo() const;

  /// @brief Sets the custom signals for this Remote Config instance.
  ///
  /// Keys may not exceed 256 characters, string values may not exceed 500
  /// characters, and at most 100 custom signals may be stored. Passing
  /// `Variant::Null()` for a key removes that custom signal.
  ///
  /// @param[in] custom_signals A map of signal keys to `Variant` values
  /// (`String`, `Int64`, `Double`, or `Null` to remove an existing key).
  ///
  /// @return A `Future<void>` which can be used to determine when the operation
  /// is complete.
  Future<void> SetCustomSignals(
      const std::map<std::string, Variant>& custom_signals);

  /// @brief Get the (possibly still pending) results of the most recent
  /// `SetCustomSignals()` call.
  ///
  /// @return The future result from the last call to `SetCustomSignals()`.
  Future<void> SetCustomSignalsLastResult();
  Future<void> SetCustomSignalsLastResult() const;

  /// @brief Starts listening for real-time config updates from the Remote
  /// Config backend and automatically fetches updates when available.
  ///
  /// When the listener is invoked with a `ConfigUpdate`, call `Activate()` to
  /// make the newly fetched values take effect.
  ///
  /// @note Real-time config updates are not supported on Desktop; on Desktop
  /// the listener is invoked with `kRemoteConfigErrorUnimplemented`.
  ///
  /// @return A `ConfigUpdateListenerRegistration` that can be used to remove
  /// the listener.
  ConfigUpdateListenerRegistration AddOnConfigUpdateListener(
      std::function<void(ConfigUpdate&&, RemoteConfigError)>
          config_update_listener);

 private:
  /// @cond FIREBASE_APP_INTERNAL
  explicit RemoteConfig(App* app);

  // Find RemoteConfig instance using App. Return null if the instance does not
  // exist.
  static RemoteConfig* FindRemoteConfig(App* app);

  // Clean up RemoteConfig instance.
  void Cleanup();

  // The firebase::App that this RemoteConfig was created with.
  App* app_;

  // Initialize internal RemoteConfig object. Return false if initialization
  // failed.
  bool InitInternal();

  // Internal object for the implementation.
  internal::RemoteConfigInternal* internal_;
  /// @endcond
};

}  // namespace remote_config
}  // namespace firebase

#endif  // FIREBASE_REMOTE_CONFIG_SRC_INCLUDE_FIREBASE_REMOTE_CONFIG_H_
