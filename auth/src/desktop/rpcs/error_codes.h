/*
 * Copyright 2017 Google LLC
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

#ifndef FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_RPCS_ERROR_CODES_H_
#define FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_RPCS_ERROR_CODES_H_

#include <string>
#include "auth/src/include/firebase/auth/types.h"

namespace firebase {
namespace auth {

// Returns error code corresponding to the given backend error. Error reason is
// used for disambiguation of a certain few errors. Falls back to
// kAuthErrorFailure.
AuthError GetAuthErrorCode(const std::string& error, const std::string& reason);

// Returns a human-readable description of the given error code. Falls back to
// an empty string.
const char* GetAuthErrorMessage(AuthError error_code);

}  // namespace auth
}  // namespace firebase
#endif  // FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_RPCS_ERROR_CODES_H_
