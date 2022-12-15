/*
 * Copyright 2022 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef FIREBASE_APP_CHECK_SRC_IOS_UTIL_IOS_H_
#define FIREBASE_APP_CHECK_SRC_IOS_UTIL_IOS_H_

#include <string>

#import "FIRAppCheckToken.h"
#include "firebase/app_check.h"

namespace firebase {
namespace app_check {
namespace internal {

AppCheckError AppCheckErrorFromNSError(NSError* _Nullable error);

NSError* AppCheckErrorToNSError(AppCheckError cpp_error,
                                const std::string& error_message);

AppCheckToken AppCheckTokenFromFIRAppCheckToken(
    FIRAppCheckToken* _Nullable token);

FIRAppCheckToken* AppCheckTokenToFIRAppCheckToken(AppCheckToken cpp_token);

}  // namespace internal
}  // namespace app_check
}  // namespace firebase

#endif  // FIREBASE_APP_CHECK_SRC_IOS_UTIL_IOS_H_
