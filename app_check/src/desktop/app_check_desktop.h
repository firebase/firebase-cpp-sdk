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

#ifndef FIREBASE_APP_CHECK_SRC_DESKTOP_APP_CHECK_DESKTOP_H_
#define FIREBASE_APP_CHECK_SRC_DESKTOP_APP_CHECK_DESKTOP_H_

#include <list>
#include <string>
#include <utility>
#include <vector>

#include "app/src/future_manager.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/future.h"
#include "app_check/src/include/firebase/app_check.h"

namespace firebase {
namespace app_check {
namespace internal {

// The callback type for psuedo-AppCheckListeners added via the
// function registry.
using FunctionRegistryCallback = void (*)(const std::string&, void*);

class FunctionRegistryAppCheckListener : public AppCheckListener {
 public:
  void AddListener(FunctionRegistryCallback callback, void* context);

  void RemoveListener(FunctionRegistryCallback callback, void* context);

  void OnAppCheckTokenChanged(const AppCheckToken& token) override;

 private:
  using Entry = std::pair<FunctionRegistryCallback, void*>;
  std::vector<Entry> callbacks_;
};

class AppCheckInternal {
 public:
  explicit AppCheckInternal(::firebase::App* app);

  ~AppCheckInternal();

  App* app() const;

  static void SetAppCheckProviderFactory(AppCheckProviderFactory* factory);

  void SetTokenAutoRefreshEnabled(bool is_token_auto_refresh_enabled);

  Future<AppCheckToken> GetAppCheckToken(bool force_refresh);

  Future<AppCheckToken> GetAppCheckTokenLastResult();

  // Gets the App Check token as just the string, to be used by
  // internal methods to not conflict with the publicly returned future.
  Future<std::string> GetAppCheckTokenStringInternal();

  void AddAppCheckListener(AppCheckListener* listener);

  void RemoveAppCheckListener(AppCheckListener* listener);

  FutureManager& future_manager() { return future_manager_; }

  ReferenceCountedFutureImpl* future();

 private:
  // Is the cached token valid
  bool HasValidCacheToken() const;

  // Update the cached Token, and call the listeners
  void UpdateCachedToken(AppCheckToken token);

  // Get the Provider associated with the stored App used to create this.
  AppCheckProvider* GetProvider();

  // Adds internal App Check functions to the function registry, which other
  // products can then call to get App Check information without needing a
  // direct dependency.
  void InitRegistryCalls();

  // Removes those functions from the registry.
  void CleanupRegistryCalls();

  // Gets a Future<AppCheckToken> for the given App, stored in the out_future.
  static bool GetAppCheckTokenAsyncForRegistry(App* app, void* /*unused*/,
                                               void* out_future);

  static bool AddAppCheckListenerForRegistry(App* app, void* callback,
                                             void* context);

  static bool RemoveAppCheckListenerForRegistry(App* app, void* callback,
                                                void* context);

  ::firebase::App* app_;

  FutureManager future_manager_;

  // Cached provider for the App. Use GetProvider instead of this.
  AppCheckProvider* cached_provider_;
  // Cached token, can be expired.
  AppCheckToken cached_token_;
  // List of registered listeners for Token changes.
  std::list<AppCheckListener*> token_listeners_;
  // Internal listener used by the function registry to track Token changes.
  FunctionRegistryAppCheckListener internal_listener_;
  // Should it automatically get an App Check token if there is not a valid
  // cached token.
  bool is_token_auto_refresh_enabled_;
};

}  // namespace internal
}  // namespace app_check
}  // namespace firebase

#endif  // FIREBASE_APP_CHECK_SRC_DESKTOP_APP_CHECK_DESKTOP_H_
