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

#include "app/src/app_state.h"
#include "app/src/future_manager.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/service_interfaces.h"
#include "app_check/src/include/firebase/app_check.h"

namespace firebase {
namespace app_check {
namespace internal {

class AppCheckInternal : public ::firebase::internal::AppCheckTokenProvider {
 public:
  explicit AppCheckInternal(::firebase::internal::AppState& app_state);
  ~AppCheckInternal() override;

  ::firebase::App app() const;

  static void SetAppCheckProviderFactory(AppCheckProviderFactory* factory);

  void SetTokenAutoRefreshEnabled(bool is_token_auto_refresh_enabled);

  Future<AppCheckToken> GetAppCheckToken(bool force_refresh);

  Future<AppCheckToken> GetAppCheckTokenLastResult();

  Future<AppCheckToken> GetLimitedUseAppCheckToken();

  Future<AppCheckToken> GetLimitedUseAppCheckTokenLastResult();

  void AddAppCheckListener(AppCheckListener* listener);

  void RemoveAppCheckListener(AppCheckListener* listener);

  // ::firebase::internal::AppCheckTokenProvider overrides:
  Future<std::string> GetTokenAsync() override;
  Future<std::string> GetLimitedUseTokenAsync() override;

  FutureManager& future_manager() { return future_manager_; }

  ReferenceCountedFutureImpl* future();

 private:
  // Requires mutex_ to be held.
  bool HasValidCacheTokenLocked() const;

  // Update the cached token and notify listeners.
  void UpdateCachedToken(AppCheckToken token);

  // Get the Provider associated with the stored App used to create this.
  AppCheckProvider* GetProvider();

  ::firebase::internal::AppState& app_state_;

  FutureManager future_manager_;

  mutable Mutex mutex_;
  // Cached provider for the App. Use GetProvider() instead of accessing
  // directly.
  AppCheckProvider* cached_provider_ = nullptr;
  // Cached token, can be expired.
  AppCheckToken cached_token_;
  // List of registered public listeners for token changes.
  std::list<AppCheckListener*> token_listeners_;
  // Should it automatically get an App Check token if there is not a valid
  // cached token.
  bool is_token_auto_refresh_enabled_ = true;
};

}  // namespace internal
}  // namespace app_check
}  // namespace firebase

#endif  // FIREBASE_APP_CHECK_SRC_DESKTOP_APP_CHECK_DESKTOP_H_
