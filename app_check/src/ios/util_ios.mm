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

#include "app_check/src/ios/util_ios.h"
#include <cstdint>

#import "FIRAppCheckErrors.h"
#import "FIRAppCheckToken.h"

#include "app/src/util_ios.h"
#include "firebase/app_check.h"
#include "firebase/internal/common.h"

namespace firebase {
namespace app_check {
namespace internal {

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
  if (![error.domain isEqualToString:FIRAppCheckErrorDomain]) {
    return kAppCheckErrorUnknown;
  }
  for (const auto& [ios_error, cpp_error] : kIosToCppErrorMap) {
    if (error.code == ios_error) {
      return cpp_error;
    }
  }
  return kAppCheckErrorUnknown;
}

NSError* AppCheckErrorToNSError(AppCheckError cpp_error, const std::string& error_message) {
  if (cpp_error == kAppCheckErrorNone) {
    return nil;
  }
  int ios_error_code = FIRAppCheckErrorCodeUnknown;
  for (const auto& [ios_error, cpp_mapped] : kIosToCppErrorMap) {
    if (cpp_error == cpp_mapped) {
      ios_error_code = ios_error;
      break;
    }
  }
  NSString* errorMsg = firebase::util::StringToNSString(error_message);
  NSDictionary* userInfo = errorMsg ? @{NSLocalizedDescriptionKey : errorMsg} : nil;
  return [[NSError alloc] initWithDomain:FIRAppCheckErrorDomain
                                    code:ios_error_code
                                userInfo:userInfo];
}

AppCheckToken AppCheckTokenFromFIRAppCheckToken(FIRAppCheckToken* _Nullable token) {
  AppCheckToken cpp_token;
  if (token) {
    cpp_token.token = util::NSStringToString(token.token);
    NSTimeInterval seconds = token.expirationDate.timeIntervalSince1970;
    cpp_token.expire_time_millis = static_cast<int64_t>(seconds * 1000);
  }
  // Note: if the iOS token is null, the cpp_token will have default values.
  return cpp_token;
}

FIRAppCheckToken* AppCheckTokenToFIRAppCheckToken(const AppCheckToken& cpp_token) {
  if (cpp_token.token.empty()) {
    return nil;
  }
  NSTimeInterval exp = static_cast<NSTimeInterval>(cpp_token.expire_time_millis) / 1000.0;
  return [[FIRAppCheckToken alloc] initWithToken:firebase::util::StringToNSString(cpp_token.token)
                                  expirationDate:[NSDate dateWithTimeIntervalSince1970:exp]];
}

}  // namespace internal
}  // namespace app_check
}  // namespace firebase
