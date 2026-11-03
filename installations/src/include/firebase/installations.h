// Copyright 2020 Google LLC
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

#ifndef FIREBASE_INSTALLATIONS_SRC_INCLUDE_FIREBASE_INSTALLATIONS_H_
#define FIREBASE_INSTALLATIONS_SRC_INCLUDE_FIREBASE_INSTALLATIONS_H_

#include <cstdint>
#include <string>

#include "firebase/app.h"
#include "firebase/future.h"
#include "firebase/internal/common.h"

#if !defined(DOXYGEN) && !defined(SWIG)
FIREBASE_APP_REGISTER_CALLBACKS_REFERENCE(installations)
#endif  // !defined(DOXYGEN) && !defined(SWIG)

/// @brief Namespace that encompasses all Firebase APIs.
namespace firebase {

/// @brief Namespace for the Firebase Installations C++ SDK.
namespace installations {

/// @brief Installations error codes returned by @ref Future::error().
enum Error {
  /// The operation succeeded.
  kErrorNone = 0,
  /// Compatibility alias for @ref kErrorNone.
  kInstallationsErrorNone = kErrorNone,
  /// An unknown or unmapped error occurred.
  kErrorUnknown = 1,
  /// Compatibility alias for @ref kErrorUnknown.
  kErrorFailure = kErrorUnknown,
  /// Compatibility alias for @ref kErrorUnknown.
  kInstallationsErrorFailure = kErrorUnknown,
  /// The Installations service could not be accessed (for example, network
  /// unavailable, server unreachable, or rate-limited).
  kErrorNoAccess = 2,
  /// Some of the parameters of the request or the @ref App configuration were
  /// invalid, or the owning @ref App has already been destroyed.
  kErrorInvalidConfiguration = 3,
  /// Local installation storage or Keychain could not be loaded or modified.
  kErrorUnableToLoad = 4,
};

#if !defined(SWIG)
/// @brief Returns a human-readable description for an Installations @ref Error.
const char* GetErrorMessage(Error error) noexcept;

/// @brief Authentication token and expiration metadata for a Firebase
/// installation.
struct InstallationTokenResult {
  /// The authentication token string.
  std::string token;
  /// Token expiration timestamp in milliseconds since the Unix epoch (`0` if
  /// unknown).
  std::int64_t expiration_timestamp = 0;

  bool operator==(const InstallationTokenResult& other) const noexcept {
    return token == other.token &&
           expiration_timestamp == other.expiration_timestamp;
  }
  bool operator!=(const InstallationTokenResult& other) const noexcept {
    return !(*this == other);
  }
};
#endif  // !defined(SWIG)

#if !defined(DOXYGEN) && !defined(SWIG)
namespace internal {
// Implementation specific data for an Installation.
class InstallationsInternal;
}  // namespace internal
#endif  // !defined(DOXYGEN) && !defined(SWIG)

/// @brief Provides a unique identifier for each app installation and a
/// mechanism to authenticate and authorize installation-scoped actions (for
/// example, sending an FCM message).
class Installations {
 public:
  /// @brief Destroys this `Installations` instance, evicting it from the
  /// per-@ref App cache and cancelling any pending platform callbacks.
  ~Installations();

  Installations(const Installations&) = delete;
  Installations& operator=(const Installations&) = delete;

  /// @brief Returns the @ref App this object is connected to, or `nullptr` if
  /// the owning @ref App has already been destroyed.
  ///
  /// @return @ref App this object is connected to, or `nullptr`.
  App* app() const noexcept { return app_; }

#if !defined(SWIG)
  /// @brief Returns `true` if this `Installations` instance is attached to a
  /// live @ref App and initialized platform backend.
  bool is_valid() const noexcept {
    return app_ != nullptr && installations_internal_ != nullptr;
  }

  /// @brief Returns the `Installations` object for the default @ref App,
  /// creating it if required, or `nullptr` if no default @ref App exists.
  static Installations* GetInstance();
#endif  // !defined(SWIG)

  /// @brief Returns the `Installations` object for an @ref App, creating the
  /// `Installations` instance if required.
  ///
  /// Repeated calls with the same @ref App return the cached `Installations`
  /// instance until it is deleted or the owning @ref App is destroyed.
  ///
  /// @param[in] app The @ref App to retrieve or create an `Installations`
  /// object from. Passing `nullptr` logs an error and returns `nullptr`.
  ///
  /// @return `Installations` object if successful, `nullptr` otherwise.
  static Installations* GetInstance(App* app);

#if !defined(SWIG)
  /// @brief Returns the `Installations` object for @p app and writes the
  /// initialization result to @p init_result_out.
  ///
  /// @param[in] app The @ref App to retrieve or create an `Installations`
  /// object from.
  /// @param[out] init_result_out Optional pointer receiving the initialization
  /// outcome (`kInitResultSuccess` or `kInitResultFailedMissingDependency`).
  ///
  /// @return `Installations` object if successful, `nullptr` otherwise.
  static Installations* GetInstance(App* app, InitResult* init_result_out);
#endif  // !defined(SWIG)

  /// @brief Returns a stable identifier that uniquely identifies the app
  /// installation, creating a new installation ID if none exists or after
  /// @ref Delete is called.
  ///
  /// @return A @ref Future containing the unique identifier for the app
  /// installation.
  Future<std::string> GetId();

  /// @brief Get the results of the most recent call to @ref GetId.
  Future<std::string> GetIdLastResult() const;

  /// @brief Deletes this Firebase app installation from both local storage and
  /// the Firebase backend, revoking associated authentication tokens.
  ///
  /// On mobile platforms, an active network connection is required. Subsequent
  /// calls to @ref GetId or @ref GetToken will generate a new installation ID.
  ///
  /// @return A @ref Future tracking completion of the deletion request.
  Future<void> Delete();

  /// @brief Get the results of the most recent call to @ref Delete.
  Future<void> DeleteLastResult() const;

  /// @brief Returns an authentication token that authorizes this installation
  /// to perform actions on behalf of the application.
  ///
  /// This is similar to an OAuth2 token except it applies to the application
  /// installation instance instead of an individual user.
  ///
  /// @param[in] force_refresh If `true`, bypasses any locally cached token and
  /// requests a fresh token from the Firebase backend.
  ///
  /// @return A @ref Future containing a valid authentication token for the
  /// Firebase installation.
  Future<std::string> GetToken(bool force_refresh);

#if !defined(SWIG)
  /// @brief Convenience overload equivalent to `GetToken(false)`.
  Future<std::string> GetToken() { return GetToken(false); }
#endif  // !defined(SWIG)

  /// @brief Get the results of the most recent call to @ref GetToken.
  Future<std::string> GetTokenLastResult() const;

#if !defined(SWIG)
  /// @brief Retrieves an @ref InstallationTokenResult containing both the
  /// authentication token string and its expiration timestamp.
  Future<InstallationTokenResult> GetTokenResult(bool force_refresh = false);

  /// @brief Get the results of the most recent call to @ref GetTokenResult.
  Future<InstallationTokenResult> GetTokenResultLastResult() const;
#endif  // !defined(SWIG)

 private:
  explicit Installations(App* app);

  static Installations* FindInstallationsLocked(App* app);
  static Installations* FindInstallations(App* app);
  // Installations internal initialize.
  bool InitInternal();
  // Clean up Installations instance.
  void DeleteInternal();
  void DeleteInternalLocked();

  App* app_ = nullptr;
  internal::InstallationsInternal* installations_internal_ = nullptr;
};

}  // namespace installations
}  // namespace firebase

#endif  // FIREBASE_INSTALLATIONS_SRC_INCLUDE_FIREBASE_INSTALLATIONS_H_
