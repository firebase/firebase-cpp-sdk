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

#include "app_check/src/include/firebase/app_check.h"

#include <memory>
#include <string>
#include <utility>

#include "app/src/app_state.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/internal/platform.h"
#include "app/src/include/firebase/version.h"
#include "app/src/log.h"
#include "app/src/service_interfaces.h"
#include "app/src/util.h"
#include "app_check/src/common/common.h"

// Include the header that matches the platform being used.
#if FIREBASE_PLATFORM_ANDROID
#include "app_check/src/android/app_check_android.h"
#elif FIREBASE_PLATFORM_IOS || FIREBASE_PLATFORM_TVOS
#include "app_check/src/ios/app_check_ios.h"
#else
#include "app_check/src/desktop/app_check_desktop.h"
#endif  // FIREBASE_PLATFORM_ANDROID, FIREBASE_PLATFORM_IOS,
        // FIREBASE_PLATFORM_TVOS

// Register the module initializer.
FIREBASE_APP_REGISTER_CALLBACKS(
    app_check,
    {
      // Attach AppCheck to each newly created AppState so other products can
      // discover it via AppState::app_check_token_provider().
      ::firebase::app_check::AppCheck::GetInstance(app);
      return ::firebase::kInitResultSuccess;
    },
    // App Check wants to be turned on by default.
    true);

namespace firebase {
namespace app_check {

DEFINE_FIREBASE_VERSION_STRING(FirebaseAppCheck);

// Define the destructors for the virtual listener/provider/factory classes.
AppCheckListener::~AppCheckListener() {}
AppCheckProvider::~AppCheckProvider() {}

// Default implementation. Can be overridden by App check providers that support
// limited use tokens
void AppCheckProvider::GetLimitedUseToken(
    std::function<void(AppCheckToken, int, const std::string&)>
        completion_callback) {
  LogWarning(
      "A limited-use token was requested, but the custom provider did not "
      "implement the GetLimitedUseToken method. The default implementation is "
      "triggered as a result, and GetToken has been invoked instead.");
  GetToken(completion_callback);
}
AppCheckProviderFactory::~AppCheckProviderFactory() {}

AppCheck AppCheck::GetInstance(const ::firebase::App& app) {
  auto state = ::firebase::internal::AppState::FromApp(app);
  if (!state) return AppCheck();

  auto service = state->GetOrCreateService<internal::AppCheckInternal>(
      &::firebase::internal::kAppCheckServiceTag, "",
      [](::firebase::internal::AppState& app_state) {
        return std::make_shared<internal::AppCheckInternal>(app_state);
      });
  return AppCheck(std::move(service));
}

void AppCheck::SetAppCheckProviderFactory(AppCheckProviderFactory* factory) {
  internal::AppCheckInternal::SetAppCheckProviderFactory(factory);
}

AppCheck::AppCheck(std::shared_ptr<internal::AppCheckInternal> internal)
    : internal_(std::move(internal)) {}

::firebase::App AppCheck::app() const {
  return internal_ ? internal_->app() : ::firebase::App();
}

void AppCheck::SetTokenAutoRefreshEnabled(bool is_token_auto_refresh_enabled) {
  if (!internal_) return;
  internal_->SetTokenAutoRefreshEnabled(is_token_auto_refresh_enabled);
}

Future<AppCheckToken> AppCheck::GetAppCheckToken(bool force_refresh) {
  return internal_ ? internal_->GetAppCheckToken(force_refresh)
                   : Future<AppCheckToken>();
}

Future<AppCheckToken> AppCheck::GetAppCheckTokenLastResult() {
  return internal_ ? internal_->GetAppCheckTokenLastResult()
                   : Future<AppCheckToken>();
}

Future<AppCheckToken> AppCheck::GetLimitedUseAppCheckToken() {
  return internal_ ? internal_->GetLimitedUseAppCheckToken()
                   : Future<AppCheckToken>();
}

Future<AppCheckToken> AppCheck::GetLimitedUseAppCheckTokenLastResult() {
  return internal_ ? internal_->GetLimitedUseAppCheckTokenLastResult()
                   : Future<AppCheckToken>();
}

void AppCheck::AddAppCheckListener(AppCheckListener* listener) {
  if (!internal_) return;
  internal_->AddAppCheckListener(listener);
}

void AppCheck::RemoveAppCheckListener(AppCheckListener* listener) {
  if (!internal_) return;
  internal_->RemoveAppCheckListener(listener);
}

}  // namespace app_check
}  // namespace firebase
