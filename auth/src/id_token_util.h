/*
 * Copyright 2026 Google LLC
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

#ifndef FIREBASE_AUTH_SRC_ID_TOKEN_UTIL_H_
#define FIREBASE_AUTH_SRC_ID_TOKEN_UTIL_H_

#include <string>

#include "firebase/auth/user.h"

namespace firebase {
namespace auth {

// Parses an encoded JWT ID token string into an IdTokenResult struct.
// Populates token, claims, auth_timestamp, issued_at_timestamp,
// expiration_timestamp, sign_in_provider, and sign_in_second_factor.
// Returns true if parsing was successful, false if the token was malformed.
bool ParseIdToken(const std::string& token, IdTokenResult* out_result);

}  // namespace auth
}  // namespace firebase

#endif  // FIREBASE_AUTH_SRC_ID_TOKEN_UTIL_H_
