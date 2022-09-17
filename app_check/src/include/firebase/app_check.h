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

struct AppCheckToken {
  /** The raw JWT attesting to this application’s identity. */
  std::string token;
 
  /** The time at which the token will expire in milliseconds since epoch. */
  long expireTimeMillis;
};

/**
* Interface for a provider that generates {@link AppCheckToken}s. This provider can be called at
* any time by any Firebase library that depends (optionally or otherwise) on {@link
* AppCheckToken}s. This provider is responsible for determining if it can create a new token at the
* time of the call and returning that new token if it can.
*/
class AppCheckProvider {
 
 public:
 virtual ~AppCheckProvider();
 /**
  * Returns a {@link Future} which resolves to a valid {@link AppCheckToken} or an {@link Exception}
  * in the case that an unexpected failure occurred while getting the token.
  */
 virtual Future<AppCheckToken> GetToken() = 0;
};

/** Interface for a factory that generates {@link AppCheckProvider}s. */
class AppCheckProviderFactory {
 
 public:
  virtual ~AppCheckProviderFactory();

  /**
   * Gets the {@link AppCheckProvider} associated with the given {@link FirebaseApp} instance, or
   * creates one if none already exists.
   */
  virtual AppCheckProvider* CreateProvider(App* app) = 0;
};

class AppCheckListener {
 public:
  virtual ~AppCheckListener();

  /**
   * This method gets invoked on the UI thread on changes to the token state. Does not trigger on
   * token expiry.
   */
  virtual void onAppCheckTokenChanged(const AppCheckToken& token) = 0;
};

namespace internal {
class AppCheckInternal;
}  // namespace internal

class AppCheck {
 
 public:
  /// @brief Destructor. You may delete an instance of AppCheck when
  /// you are finished using it, to shut down the AppCheck library.
  ~AppCheck();

  /**
   * Gets the instance of {@code FirebaseAppCheck} associated with the given {@link FirebaseApp}
   * instance.
   */
  static AppCheck* GetInstance(::firebase::App* app);
 
  /**
   * Installs the given {@link AppCheckProviderFactory}, overwriting any that were previously
   * associated with this {@code FirebaseAppCheck} instance. Any {@link AppCheckTokenListener}s
   * attached to this {@code FirebaseAppCheck} instance will be transferred from existing factories
   * to the newly installed one.
   *
   * <p>Automatic token refreshing will only occur if the global {@code
   * isDataCollectionDefaultEnabled} flag is set to true. To allow automatic token refreshing for
   * Firebase App Check without changing the {@code isDataCollectionDefaultEnabled} flag for other
   * Firebase SDKs, use {@link #setAppCheckProviderFactory(AppCheckProviderFactory, boolean)}
   * instead or call {@link #setTokenAutoRefreshEnabled(boolean)} after installing the {@code
   * factory}.
   */
  static void SetAppCheckProviderFactory(AppCheckProviderFactory* factory);
 
  /// @brief Get the firebase::App that this AppCheck was created with.
  ///
  /// @returns The firebase::App this AppCheck was created with.
  ::firebase::App* app();

  /** Sets the {@code isTokenAutoRefreshEnabled} flag. */
  void SetTokenAutoRefreshEnabled(bool isTokenAutoRefreshEnabled);
 
  /**
   * Requests a Firebase App Check token. This method should be used ONLY if you need to authorize
   * requests to a non-Firebase backend. Requests to Firebase backends are authorized automatically
   * if configured.
   */
  Future<AppCheckToken> GetAppCheckToken(bool forceRefresh);
  Future<AppCheckToken> GetAppCheckTokenLastResult();
 
  /**
   * Registers an {@link AppCheckListener} to changes in the token state. This method should be used
   * ONLY if you need to authorize requests to a non-Firebase backend. Requests to Firebase backends
   * are authorized automatically if configured.
   */
  void AddAppCheckListener(AppCheckListener* listener);
 
  /** Unregisters an {@link AppCheckListener} to changes in the token state. */
  void RemoveAppCheckListener(AppCheckListener* listener);

 private:
  AppCheck(::firebase::App* app);

  void DeleteInternal();

  internal::AppCheckInternal* internal_;
};
 
}  // namespace app_check
}  // namespace firebase

#endif  // FIREBASE_APP_CHECK_SRC_INCLUDE_FIREBASE_APP_CHECK_H_
