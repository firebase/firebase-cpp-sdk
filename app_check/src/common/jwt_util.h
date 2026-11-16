// Copyright 2026 Google LLC
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

#ifndef FIREBASE_APP_CHECK_SRC_COMMON_JWT_UTIL_H_
#define FIREBASE_APP_CHECK_SRC_COMMON_JWT_UTIL_H_

#include <string_view>

#include "firebase/app_check.h"

namespace firebase::app_check::internal {

// Constructs an AppCheckToken from a raw JWT string, populating
// `expire_time_millis` from the JWT payload's numeric `exp` claim (or leaving
// it as 0 if `jwt` cannot be decoded as a JWT with a valid `exp`: a positive
// number of seconds no later than 9999-12-31T23:59:59Z).
AppCheckToken AppCheckTokenFromJwt(std::string_view jwt);

}  // namespace firebase::app_check::internal

#endif  // FIREBASE_APP_CHECK_SRC_COMMON_JWT_UTIL_H_
