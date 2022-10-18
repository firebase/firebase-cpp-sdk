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

#include "app_check/src/ios/app_check_ios.h"

#import "FIRAppCheckErrors.h"
#import "FIRAppCheckToken.h"

#include "app/src/util_ios.h"
#include "app_check/src/common/common.h"
#include "firebase/app_check.h"
#include "firebase/internal/common.h"

namespace firebase {
namespace app_check {
namespace internal {

static AppCheckProviderFactory* g_provider_factory = nullptr;

AppCheckInternal::AppCheckInternal(App* app) : app_(app) {
  future_manager().AllocFutureApi(this, kAppCheckFnCount);
}

AppCheckInternal::~AppCheckInternal() {
  future_manager().ReleaseFutureApi(this);
  app_ = nullptr;
}

::firebase::App* AppCheckInternal::app() const { return app_; }

ReferenceCountedFutureImpl* AppCheckInternal::future() {
  return future_manager().GetFutureApi(this);
}

void AppCheckInternal::SetAppCheckProviderFactory(AppCheckProviderFactory* factory) {
  g_provider_factory = factory;
}

void AppCheckInternal::SetTokenAutoRefreshEnabled(bool is_token_auto_refresh_enabled) {}

Future<AppCheckToken> AppCheckInternal::GetAppCheckToken(bool force_refresh) {
  auto handle = future()->SafeAlloc<AppCheckToken>(kAppCheckFnGetAppCheckToken);
  AppCheckToken token;
  future()->CompleteWithResult(handle, 0, token);
  return MakeFuture(future(), handle);
}

Future<AppCheckToken> AppCheckInternal::GetAppCheckTokenLastResult() {
  return static_cast<const Future<AppCheckToken>&>(
      future()->LastResult(kAppCheckFnGetAppCheckToken));
}

void AppCheckInternal::AddAppCheckListener(AppCheckListener* listener) {}

void AppCheckInternal::RemoveAppCheckListener(AppCheckListener* listener) {}

}  // namespace internal

static const struct {
  int ios_error;
  AppCheckError cpp_error;
} kIosToCppErrorMap[] = {
    {FIRAppCheckErrorCodeUnknown, kAppCheckErrorUnknown},
    {FIRAppCheckErrorCodeServerUnreachable, kAppCheckErrorServerUnreachable},
    {FIRAppCheckErrorCodeInvalidConfiguration, kAppCheckErrorInvalidConfiguration},
    {FIRAppCheckErrorCodeKeychain, kAppCheckErrorSystemKeychain},
    {FIRAppCheckErrorCodeUnsupported, kAppCheckErrorUnsupportedProvider},
};

AppCheckError AppCheckErrorFromNSError(NSError* _Nullable error) {
  if (!error) {
    return kAppCheckErrorNone;
  }
  for (size_t i = 0; i < FIREBASE_ARRAYSIZE(kIosToCppErrorMap); i++) {
    if (error.code == kIosToCppErrorMap[i].ios_error) return kIosToCppErrorMap[i].cpp_error;
  }
  return kAppCheckErrorUnknown;
}

AppCheckToken AppCheckTokenFromFIRAppCheckToken(FIRAppCheckToken* _Nullable token) {
  AppCheckToken cpp_token;
  if (token) {
    cpp_token.token = util::NSStringToString(token.token);
    NSTimeInterval seconds = token.expirationDate.timeIntervalSince1970;
    cpp_token.expire_time_millis = static_cast<int64_t>(seconds * 1000);
  }
  // TODO: what to do if ios token is null? should cpp token be nullable?
  return cpp_token;
}

}  // namespace app_check
}  // namespace firebase
