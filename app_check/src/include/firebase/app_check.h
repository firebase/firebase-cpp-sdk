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

namespace firebase {
namespace app_check {

#ifndef FIREBASE_APP_CHECK_SRC_INCLUDE_FIREBASE_APP_CHECK_H_
#define FIREBASE_APP_CHECK_SRC_INCLUDE_FIREBASE_APP_CHECK_H_

#include "app_check/src/app_check_provider.h"
#include "app_check/src/app_check_provider_factory.h"

/// Struct to hold tokens emitted by the Firebase App Check service which are
/// minted upon a successful application verification. These tokens are the
/// federated output of a verification flow, the structure of which is
/// independent of the mechanism by which the application was verified.
struct AppCheckToken {
  /// A Firebase App Check token.
  std::string token;

  /// A Firebase App Check token expiration date in the device local time.
  int64_t expire_time_millis;
}

class AppCheckListener {
  virtual ~AppCheckListener();
  /**
   * This method gets invoked on the UI thread on changes to the token state.
   * Does not trigger on token expiry.
   */
  virtual void OnAppCheckTokenChanged(const AppCheckToken& token) = 0;
}

/**
 * Interface for a provider that generates {@link AppCheckToken}s. This provider
 * can be called at any time by any Firebase library that depends (optionally or
 * otherwise) on {@link AppCheckToken}s. This provider is responsible for
 * determining if it can create a new token at the time of the call and
 * returning that new token if it can.
 */
class AppCheckProvider {
 public:
  virtual ~AppCheckProvider();
  /**
   * Fetches an AppCheckToken and then calls the provided callback method with
   * the token or with an error code and error message.
   */
  virtual GetToken(
    std::function<void(AppCheckToken, int, string)> completion_callback) = 0;
}

/** Interface for a factory that generates {@link AppCheckProvider}s. */
class AppCheckProviderFactory {
 public:
  virtual ~AppCheckProviderFactory();
  /**
   * Gets the {@link AppCheckProvider} associated with the given {@link
   * FirebaseApp} instance, or creates one if none already exists.
   */
  virtual AppCheckProvider* CreateProvider(const App& app) = 0;
}

class AppCheck {
 public:
  /**
   * Gets the instance of {@code AppCheck} associated with the given {@link
   * FirebaseApp} instance.
   */
  static AppCheck* GetInstance(::firebase::App* app);

  /**
   * Installs the given {@link AppCheckProviderFactory}, overwriting any that
   * were previously associated with this {@code AppCheck} instance. Any {@link
   * AppCheckTokenListener}s attached to this {@code AppCheck} instance will be
   * transferred from existing factories to the newly installed one.
   *
   * <p>Automatic token refreshing will only occur if the global {@code
   * isDataCollectionDefaultEnabled} flag is set to true. To allow automatic
   * token refreshing for Firebase App Check without changing the {@code
   * isDataCollectionDefaultEnabled} flag for other Firebase SDKs, use {@link
   * #setAppCheckProviderFactory(AppCheckProviderFactory, bool)} instead or call
   * {@link #setTokenAutoRefreshEnabled(bool)} after installing the {@code
   * factory}.
   *
   * This method should be called before initializing the Firebase App.
   */
  static void SetAppCheckProviderFactory(
      const AppCheckProviderFactory& factory) = 0;

  /** Sets the {@code isTokenAutoRefreshEnabled} flag. */
  void SetTokenAutoRefreshEnabled(bool is_token_auto_refresh_enabled) =
      0;

  /**
   * Requests a Firebase App Check token. This method should be used ONLY if you
   * need to authorize requests to a non-Firebase backend. Requests to Firebase
   * backends are authorized automatically if configured.
   */
  Future<AppCheckToken> GetAppCheckToken(bool force_refresh) = 0;

  /**
   * Registers an {@link AppCheckListener} to changes in the token state. This
   * method should be used ONLY if you need to authorize requests to a
   * non-Firebase backend. Requests to Firebase backends are authorized
   * automatically if configured.
   */
  void AddAppCheckListener(AppCheckListener* listener) = 0;

  /** Unregisters an {@link AppCheckListener} to changes in the token state. */
  void RemoveAppCheckListener(AppCheckListener* listener) = 0;
}

#endif  // FIREBASE_APP_CHECK_SRC_INCLUDE_FIREBASE_APP_CHECK_H_

}  // namespace app_check
}  // namespace firebase

