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

#include "app/src/future_manager.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/future.h"
#include "app_check/src/include/firebase/app_check.h"

namespace firebase {
namespace app_check {
namespace internal {

class AppCheckInternal {
 public:
  explicit AppCheckInternal(::firebase::App* app);

  ~AppCheckInternal();

  App* app() const;

  static void SetAppCheckProviderFactory(AppCheckProviderFactory* factory);

  void SetTokenAutoRefreshEnabled(bool is_token_auto_refresh_enabled);

  Future<AppCheckToken> GetAppCheckToken(bool force_refresh);

  Future<AppCheckToken> GetAppCheckTokenLastResult();

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

  ::firebase::App* app_;

  FutureManager future_manager_;

  // Cached provider for the App. Use GetProvider instead of this.
  AppCheckProvider* cached_provider_;
  // Cached token, can be expired.
  AppCheckToken cached_token_;
  // List of registered listeners for Token changes.
  std::list<AppCheckListener*> token_listeners_;
};

}  // namespace internal
}  // namespace app_check
}  // namespace firebase

#endif  // FIREBASE_APP_CHECK_SRC_DESKTOP_APP_CHECK_DESKTOP_H_
