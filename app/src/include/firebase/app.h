/*
 * Copyright 2016 Google LLC
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

#ifndef FIREBASE_APP_SRC_INCLUDE_FIREBASE_APP_H_
#define FIREBASE_APP_SRC_INCLUDE_FIREBASE_APP_H_

#include "firebase/internal/platform.h"

#if FIREBASE_PLATFORM_ANDROID
#include <jni.h>
#endif  // FIREBASE_PLATFORM_ANDROID

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#if FIREBASE_PLATFORM_IOS || FIREBASE_PLATFORM_TVOS
#ifdef __OBJC__
@class FIRApp;
#endif  // __OBJC__
#endif  // FIREBASE_PLATFORM_IOS

namespace firebase {

#ifdef FIREBASE_LINUX_BUILD_CONFIG_STRING
// Check to see if the shared object compiler string matches the input
void CheckCompilerString(const char* input);
#endif  // FIREBASE_LINUX_BUILD_CONFIG_STRING

#ifdef INTERNAL_EXPERIMENTAL
#if FIREBASE_PLATFORM_DESKTOP
namespace heartbeat {
class HeartbeatController;  // forward declaration
}  // namespace heartbeat
#endif  // FIREBASE_PLATFORM_DESKTOP
#endif  // INTERNAL_EXPERIMENTAL

namespace internal {
class AppState;
}  // namespace internal

/// @brief Reports whether a Firebase module initialized successfully.
enum InitResult {
  /// The given library was successfully initialized.
  kInitResultSuccess = 0,

  /// The given library failed to initialize due to a missing dependency.
  ///
  /// On Android, this typically means that Google Play services is not
  /// available and the library requires it.
  kInitResultFailedMissingDependency
};

/// @brief Default name for firebase::App() objects.
extern const char* const kDefaultAppName;

/// @brief Options that control the creation of a Firebase App.
class AppOptions {
  friend class App;

 public:
  AppOptions() = default;

  void set_app_id(const char* id) { app_id_ = id; }
  const char* app_id() const { return app_id_.c_str(); }

  void set_api_key(const char* key) { api_key_ = key; }
  const char* api_key() const { return api_key_.c_str(); }

  void set_messaging_sender_id(const char* sender_id) {
    fcm_sender_id_ = sender_id;
  }
  const char* messaging_sender_id() const { return fcm_sender_id_.c_str(); }

  void set_database_url(const char* url) { database_url_ = url; }
  const char* database_url() const { return database_url_.c_str(); }

  /// @cond FIREBASE_APP_INTERNAL
  void set_ga_tracking_id(const char* id) { ga_tracking_id_ = id; }
  const char* ga_tracking_id() const { return ga_tracking_id_.c_str(); }
  /// @endcond

  void set_storage_bucket(const char* bucket) { storage_bucket_ = bucket; }
  const char* storage_bucket() const { return storage_bucket_.c_str(); }

  void set_project_id(const char* project) { project_id_ = project; }
  const char* project_id() const { return project_id_.c_str(); }

#ifdef INTERNAL_EXPERIMENTAL
  void set_client_id(const char* client_id) { client_id_ = client_id; }
  const char* client_id() const { return client_id_.c_str(); }

  void set_package_name(const char* package_name) {
    package_name_ = package_name;
  }
  const char* package_name() const { return package_name_.c_str(); }
#endif  // INTERNAL_EXPERIMENTAL

  static AppOptions* LoadFromJsonConfig(const char* config,
                                        AppOptions* options = nullptr);

#if INTERNAL_EXPERIMENTAL
  bool operator==(const AppOptions& options) const {
    return (package_name_.empty() || package_name_ == options.package_name_) &&
           (api_key_.empty() || api_key_ == options.api_key_) &&
           (app_id_.empty() || app_id_ == options.app_id_) &&
           (database_url_.empty() || database_url_ == options.database_url_) &&
           (ga_tracking_id_.empty() ||
            ga_tracking_id_ == options.ga_tracking_id_) &&
           (fcm_sender_id_.empty() ||
            fcm_sender_id_ == options.fcm_sender_id_) &&
           (storage_bucket_.empty() ||
            storage_bucket_ == options.storage_bucket_) &&
           (project_id_.empty() || project_id_ == options.project_id_);
  }

  bool operator!=(const AppOptions& options) const {
    return !operator==(options);
  }

#if FIREBASE_PLATFORM_ANDROID || defined(DOXYGEN)
  static AppOptions* LoadDefault(AppOptions* options, JNIEnv* jni_env,
                                 jobject activity);
  bool PopulateRequiredWithDefaults(JNIEnv* jni_env, jobject activity);
#endif  // FIREBASE_PLATFORM_ANDROID || defined(DOXYGEN)

#if !FIREBASE_PLATFORM_ANDROID || defined(DOXYGEN)
  static AppOptions* LoadDefault(AppOptions* options);
  bool PopulateRequiredWithDefaults();
#endif  // !FIREBASE_PLATFORM_ANDROID || defined(DOXYGEN)
#endif  // INTERNAL_EXPERIMENTAL

  /// @cond FIREBASE_APP_INTERNAL
 private:
  std::string package_name_;
  std::string api_key_;
  std::string app_id_;
  std::string client_id_;
  std::string database_url_;
  std::string ga_tracking_id_;
  std::string fcm_sender_id_;
  std::string storage_bucket_;
  std::string project_id_;
  /// @endcond
};

/// @brief Value handle to a reference-counted Firebase application instance.
///
/// Copying or moving an App shares ownership of its underlying AppState. Any
/// product handles obtained from an App (Auth, Database, Storage, Firestore,
/// etc.) also keep the underlying AppState alive, so dropping handles in any
/// order — including during static destruction after main() — is safe.
///
/// By default, App::Create() does NOT register the App in any global registry.
/// Pass App explicitly to avoid all global app state. If you need name-based
/// lookup via App::GetInstance(name) or App::GetApps(), opt in explicitly via
/// app.Register() or App::CreateRegistered().
class App {
 public:
  /// Constructs an invalid (empty) App handle.
  App() = default;
  App(const App&) = default;
  App(App&&) noexcept = default;
  App& operator=(const App&) = default;
  App& operator=(App&&) noexcept = default;
  ~App() = default;

  /// Returns true if this handle refers to a live App instance.
  bool is_valid() const noexcept { return state_ != nullptr; }
  explicit operator bool() const noexcept { return is_valid(); }

  /// Compares underlying AppState identity.
  bool operator==(const App& other) const noexcept {
    return state_ == other.state_;
  }
  bool operator!=(const App& other) const noexcept {
    return state_ != other.state_;
  }

#if !FIREBASE_PLATFORM_ANDROID || defined(DOXYGEN)
  /// Initializes an App with default options (not registered globally).
  /// Returns an invalid App if default options cannot be loaded.
  static App Create();

  /// Initializes an App with the given options and default name (not
  /// registered globally).
  static App Create(const AppOptions& options);

  /// Initializes a named App with the given options (not registered globally).
  static App Create(const AppOptions& options, std::string_view name);

  /// Convenience: Create() + Register(). Returns an invalid App if creation or
  /// registration fails (e.g., another live App is already registered under
  /// that name).
  static App CreateRegistered();
  static App CreateRegistered(const AppOptions& options);
  static App CreateRegistered(const AppOptions& options, std::string_view name);
#endif  // !FIREBASE_PLATFORM_ANDROID || defined(DOXYGEN)

#ifndef SWIG
#if FIREBASE_PLATFORM_ANDROID || defined(DOXYGEN)
  /// Initializes an App with default options (not registered globally).
  static App Create(JNIEnv* jni_env, jobject activity);

  /// Initializes an App with the given options and default name (not
  /// registered globally).
  static App Create(const AppOptions& options, JNIEnv* jni_env,
                    jobject activity);

  /// Initializes a named App with the given options (not registered globally).
  static App Create(const AppOptions& options, std::string_view name,
                    JNIEnv* jni_env, jobject activity);

  /// Convenience: Create(...) + Register().
  static App CreateRegistered(JNIEnv* jni_env, jobject activity);
  static App CreateRegistered(const AppOptions& options, JNIEnv* jni_env,
                              jobject activity);
  static App CreateRegistered(const AppOptions& options, std::string_view name,
                              JNIEnv* jni_env, jobject activity);
#endif  // FIREBASE_PLATFORM_ANDROID || defined(DOXYGEN)
#endif  // SWIG

  /// Opts this App into the process-wide weak name registry so
  /// App::GetInstance(name()) and App::GetApps() can look it up while alive.
  ///
  /// The registry holds only a non-owning weak reference and does not extend
  /// the App's lifetime. Returns false if this App is invalid or if a
  /// different live App is already registered under the same name.
  bool Register() const;

  /// Removes this App from the process-wide weak name registry if it is
  /// currently registered. Calling Unregister() is optional; expired entries
  /// are pruned lazily on subsequent registry operations.
  void Unregister() const;

  /// Looks up an opt-in registered App by name. Returns std::nullopt if no
  /// live App has been registered under `name`.
  static std::optional<App> GetInstance(
      std::string_view name = kDefaultAppName);

  /// Returns all currently live opt-in registered Apps (with the default app,
  /// if registered, placed last).
  static std::vector<App> GetApps();

#ifndef SWIG
#if FIREBASE_PLATFORM_ANDROID || defined(DOXYGEN)
  JavaVM* java_vm() const;
  JNIEnv* GetJNIEnv() const;
  jobject activity() const;
#endif  // FIREBASE_PLATFORM_ANDROID || defined(DOXYGEN)
#endif  // SWIG

  /// Get the name of this App instance, or "" if invalid.
  const char* name() const;

  /// Get options the App was created with (empty options if invalid).
  const AppOptions& options() const;

#if defined(INTERNAL_EXPERIMENTAL) || defined(SWIG)
  void SetDataCollectionDefaultEnabled(bool enabled);
  bool IsDataCollectionDefaultEnabled() const;
#endif  // defined(INTERNAL_EXPERIMENTAL) || defined(SWIG)

#ifdef INTERNAL_EXPERIMENTAL
  const std::map<std::string, InitResult>& init_results() const;

  static void RegisterLibrary(const char* library, const char* version,
                              void* platform_resource);
  static const char* GetUserAgent();
  static void SetDefaultConfigPath(const char* path);
#endif  // INTERNAL_EXPERIMENTAL

#ifdef INTERNAL_EXPERIMENTAL
#if FIREBASE_PLATFORM_DESKTOP
  void LogHeartbeat() const;
  std::shared_ptr<heartbeat::HeartbeatController> GetHeartbeatController()
      const;
#endif  // FIREBASE_PLATFORM_DESKTOP
#endif  // INTERNAL_EXPERIMENTAL

#ifdef INTERNAL_EXPERIMENTAL
#if FIREBASE_PLATFORM_ANDROID
  jobject GetPlatformApp() const;
#elif FIREBASE_PLATFORM_IOS || FIREBASE_PLATFORM_TVOS
#ifdef __OBJC__
  FIRApp* GetPlatformApp() const;
#endif  // __OBJC__
#endif
#endif  // INTERNAL_EXPERIMENTAL

 private:
  /// @cond FIREBASE_APP_INTERNAL
  friend class internal::AppState;

  explicit App(std::shared_ptr<internal::AppState> state);

  std::shared_ptr<internal::AppState> state_;
  /// @endcond
};

}  // namespace firebase

#endif  // FIREBASE_APP_SRC_INCLUDE_FIREBASE_APP_H_
