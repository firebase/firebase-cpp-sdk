/*
 * Copyright 2016 Google LLC
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

#ifndef FIREBASE_APP_CLIENT_CPP_SRC_UTIL_DESKTOP_H_
#define FIREBASE_APP_CLIENT_CPP_SRC_UTIL_DESKTOP_H_

#include <string>

namespace firebase {

// Encoding the input string with base64, and set to output.
bool Base64Encode(const std::string& input, std::string* output);

// Decoding the input string with base64, and set to output.
bool Base64Decode(const std::string& input, std::string* output);

}  // namespace firebase

#endif  // FIREBASE_APP_CLIENT_CPP_SRC_UTIL_DESKTOP_H_
