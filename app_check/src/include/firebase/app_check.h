// Copyright 2022 Google LLC
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

#ifndef FIREBASE_APP_CHECK_SRC_INCLUDE_FIREBASE_APP_CHECK_H_
#define FIREBASE_APP_CHECK_SRC_INCLUDE_FIREBASE_APP_CHECK_H_

#include <string>

#include "firebase/app.h"
#include "firebase/future.h"

namespace firebase {
namespace app_check {

/// Error code returned by AppCheck C++ functions.
enum AppCheckError {
  /// The operation was a success, no error occurred.
  kAppCheckErrorNone = 0,
  /// A network connection error.
  kAppCheckErrorServerUnreachable = 1,
  /// Invalid configuration error. Currently, an exception is thrown but this
  /// error is reserved for future implementations of invalid configuration
  /// detection.
  kAppCheckErrorInvalidConfiguration = 2,
  /// System keychain access error. Ensure that the app has proper keychain
  /// access.
  kAppCheckErrorSystemKeychain = 3,
  /// Selected AppCheckProvider provider is not supported on the current
  /// platform
  /// or OS version.
  kAppCheckErrorUnsupportedProvider = 4,
  /// An unknown error occurred.
  kAppCheckErrorUnknown = 5,
};

/// Struct to hold tokens emitted by the Firebase App Check service which are
/// minted upon a successful application verification. These tokens are the
/// federated output of a verification flow, the structure of which is
/// independent of the mechanism by which the application was verified.
struct AppCheckToken {
  /// A Firebase App Check token.
  std::string token;

  /// The time at which the token will expire in milliseconds since epoch.
  int64_t expire_time_millis;
};

/// @brief Base class used to receive messages when AppCheck token changes.
class AppCheckListener {
 public:
  virtual ~AppCheckListener() = 0;
  /// This method gets invoked on the UI thread on changes to the token state.
  /// Does not trigger on token expiry.
  virtual void OnAppCheckTokenChanged(const AppCheckToken& token) = 0;
};

/// Interface for a provider that generates {@link AppCheckToken}s. This
/// provider can be called at any time by any Firebase library that depends
/// (optionally or otherwise) on {@link AppCheckToken}s. This provider is
/// responsible for determining if it can create a new token at the time of the
/// call and returning that new token if it can.
class AppCheckProvider {
 public:
  virtual ~AppCheckProvider() = 0;
  /// Fetches an AppCheckToken and then calls the provided callback method with
  /// the token or with an error code and error message.
  virtual void GetToken(
      std::function<void(AppCheckToken, int, const std::string&)>
          completion_callback) = 0;
};

/// Interface for a factory that generates {@link AppCheckProvider}s.
class AppCheckProviderFactory {
 public:
  virtual ~AppCheckProviderFactory() = 0;
  /// Gets the {@link AppCheckProvider} associated with the given
  /// {@link App} instance, or creates one if none
  /// already exists.
  virtual AppCheckProvider* CreateProvider(App* app) = 0;
};

namespace internal {
class AppCheckInternal;
}  // namespace internal

/// @brief Firebase App Check object.
///
/// App Check helps protect your API resources from abuse by preventing
/// unauthorized clients from accessing your backend resources.
///
/// With App Check, devices running your app will use an AppCheckProvider that
/// attests to one or both of the following:
/// * Requests originate from your authentic app
/// * Requests originate from an authentic, untampered device
class AppCheck {
 public:
  /// @brief Destructor. You may delete an instance of AppCheck when
  /// you are finished using it to shut down the AppCheck library.
  ~AppCheck();

  /// Gets the instance of AppCheck associated with the given
  /// {@link App} instance.
  static AppCheck* GetInstance(::firebase::App* app);

  /// Installs the given AppCheckProviderFactory, overwriting any that
  /// were previously associated with this AppCheck instance. Any
  /// AppCheckTokenListeners attached to this AppCheck instance
  /// will be transferred from existing factories to the newly installed one.
  ///
  /// Automatic token refreshing will only occur if the global
  /// isDataCollectionDefaultEnabled flag is set to true. To allow
  /// automatic token refreshing for Firebase App Check without changing the
  /// isDataCollectionDefaultEnabled flag for other Firebase SDKs, call
  /// setTokenAutoRefreshEnabled(bool) after installing the factory.
  ///
  /// This method should be called before initializing the Firebase App.
  static void SetAppCheckProviderFactory(AppCheckProviderFactory* factory);

  /// @brief Get the firebase::App that this AppCheck was created with.
  ///
  /// @returns The firebase::App this AppCheck was created with.
  ::firebase::App* app();

  /// Sets the isTokenAutoRefreshEnabled flag.
  void SetTokenAutoRefreshEnabled(bool is_token_auto_refresh_enabled);

  /// Requests a Firebase App Check token. This method should be used ONLY if
  /// you need to authorize requests to a non-Firebase backend. Requests to
  /// Firebase backends are authorized automatically if configured.
  Future<AppCheckToken> GetAppCheckToken(bool force_refresh);

  /// Returns the result of the most recent call to GetAppCheckToken();
  Future<AppCheckToken> GetAppCheckTokenLastResult();

  /// Registers an {@link AppCheckListener} to changes in the token state. This
  /// method should be used ONLY if you need to authorize requests to a
  /// non-Firebase backend. Requests to Firebase backends are authorized
  /// automatically if configured.
  void AddAppCheckListener(AppCheckListener* listener);

  /// Unregisters an {@link AppCheckListener} to changes in the token state.
  void RemoveAppCheckListener(AppCheckListener* listener);

 private:
  explicit AppCheck(::firebase::App* app);

  void DeleteInternal();

  // Make the Internal version a friend class, so that it can access itself.
  friend class internal::AppCheckInternal;
  internal::AppCheckInternal* internal_;
};

}  // namespace app_check
}  // namespace firebase

#endif  // FIREBASE_APP_CHECK_SRC_INCLUDE_FIREBASE_APP_CHECK_H_
