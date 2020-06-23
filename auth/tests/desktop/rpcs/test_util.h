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

#ifndef FIREBASE_AUTH_CLIENT_CPP_TESTS_DESKTOP_RPCS_TEST_UTIL_H_
#define FIREBASE_AUTH_CLIENT_CPP_TESTS_DESKTOP_RPCS_TEST_UTIL_H_

#include <string>

namespace firebase {
namespace auth {

// Sign in a new user and return its local ID and ID token.
bool GetNewUserLocalIdAndIdToken(const char* api_key, std::string* local_id,
                                 std::string* id_token);

// Sign in a new user and return its local ID and refresh token.
bool GetNewUserLocalIdAndRefreshToken(const char* api_key,
                                      std::string* local_id,
                                      std::string* refresh_token);
std::string SignUpNewUserAndGetIdToken(const char* api_key,
                                      const char* email);

}  // namespace auth
}  // namespace firebase

#endif  // FIREBASE_AUTH_CLIENT_CPP_TESTS_DESKTOP_RPCS_TEST_UTIL_H_
