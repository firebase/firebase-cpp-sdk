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

class FirebaseAppCheck { // ALMOSTMATT: implements InternalAppCheckTokenProvider 

  public:
  /** Gets the default instance of {@code FirebaseAppCheck}. */
  static FirebaseAppCheck getInstance();

  /**
   * Gets the instance of {@code FirebaseAppCheck} associated with the given {@link FirebaseApp}
   * instance.
   */
  static FirebaseAppCheck getInstance(const App& app);

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
  virtual void setAppCheckProviderFactory(const AppCheckProviderFactory& factory) = 0;

  /**
   * Installs the given {@link AppCheckProviderFactory}, overwriting any that were previously
   * associated with this {@code FirebaseAppCheck} instance. Any {@link AppCheckTokenListener}s
   * attached to this {@code FirebaseAppCheck} instance will be transferred from existing factories
   * to the newly installed one.
   *
   * <p>Automatic token refreshing will only occur if the {@code isTokenAutoRefreshEnabled} field is
   * set to true. To use the global {@code isDataCollectionDefaultEnabled} flag for determining
   * automatic token refreshing, call {@link
   * #setAppCheckProviderFactory(AppCheckProviderFactory)} instead.
   */
  // ALMOSTMATT: does iOS have this variation?
  virtual void setAppCheckProviderFactory(
      const AppCheckProviderFactory& factory, boolean isTokenAutoRefreshEnabled) = 0;

  /** Sets the {@code isTokenAutoRefreshEnabled} flag. */
  virtual void setTokenAutoRefreshEnabled(boolean isTokenAutoRefreshEnabled) = 0;

  /**
   * Requests a Firebase App Check token. This method should be used ONLY if you need to authorize
   * requests to a non-Firebase backend. Requests to Firebase backends are authorized automatically 
   * if configured.
   */
  virtual Future<AppCheckToken> getAppCheckToken(boolean forceRefresh) = 0;

  /**
   * Registers an {@link AppCheckListener} to changes in the token state. This method should be used
   * ONLY if you need to authorize requests to a non-Firebase backend. Requests to Firebase backends
   * are authorized automatically if configured.
   */
  virtual void addAppCheckListener(AppCheckListener* listener) = 0;

  /** Unregisters an {@link AppCheckListener} to changes in the token state. */
  virtual void removeAppCheckListener(AppCheckListener* listener) = 0;

  // ALMOSTMATT: should this be a separate file? or a non-nested class? it is an interface so is virtual
  class AppCheckListener {
    virtual ~AppCheckListener();
    /**
     * This method gets invoked on the UI thread on changes to the token state. Does not trigger on
     * token expiry.
     */
    virtual void onAppCheckTokenChanged(const AppCheckToken& token) = 0;
  }
}

}  // namespace app_check
}  // namespace firebase
