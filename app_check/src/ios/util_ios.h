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

#ifndef FIREBASE_APP_CHECK_SRC_IOS_UTIL_IOS_H_
#define FIREBASE_APP_CHECK_SRC_IOS_UTIL_IOS_H_

#include <functional>
#include <string>

#import "FIRAppCheckProvider.h"
#import "FIRAppCheckToken.h"
#include "app/src/util_ios.h"
#include "firebase/app_check.h"

namespace firebase::app_check::internal {

AppCheckError AppCheckErrorFromNSError(NSError* _Nullable error);
NSError* AppCheckErrorToNSError(AppCheckError cpp_error,
                                const std::string& error_message);
AppCheckToken AppCheckTokenFromFIRAppCheckToken(
    FIRAppCheckToken* _Nullable token);
FIRAppCheckToken* AppCheckTokenToFIRAppCheckToken(
    const AppCheckToken& cpp_token);

class IosAppCheckProvider : public AppCheckProvider {
 public:
  explicit IosAppCheckProvider(id<FIRAppCheckProvider> provider)
      : ios_provider_(provider) {}
  ~IosAppCheckProvider() override = default;

  void GetToken(std::function<void(AppCheckToken, int, const std::string&)>
                    cb) override {
    Fetch(false, std::move(cb));
  }
  void GetLimitedUseToken(
      std::function<void(AppCheckToken, int, const std::string&)> cb) override {
    Fetch(true, std::move(cb));
  }

 private:
  void Fetch(bool limited_use,
             std::function<void(AppCheckToken, int, const std::string&)> cb) {
    if (!cb) return;
    if (!ios_provider_) {
      cb(AppCheckToken{}, kAppCheckErrorUnknown, "Provider not initialized");
      return;
    }
    auto handler = ^(FIRAppCheckToken* _Nullable token,
                     NSError* _Nullable error) {
      if (cb) {
        cb(AppCheckTokenFromFIRAppCheckToken(token),
           AppCheckErrorFromNSError(error),
           util::NSStringToString(error.localizedDescription));
      }
    };
    if (limited_use && [ios_provider_ respondsToSelector:@selector
                                      (getLimitedUseTokenWithCompletion:)]) {
      [ios_provider_ getLimitedUseTokenWithCompletion:handler];
    } else {
      [ios_provider_ getTokenWithCompletion:handler];
    }
  }

  id<FIRAppCheckProvider> ios_provider_;
};

}  // namespace firebase::app_check::internal

#endif  // FIREBASE_APP_CHECK_SRC_IOS_UTIL_IOS_H_
