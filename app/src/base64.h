/*
 * Copyright 2019 Google LLC
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

#ifndef FIREBASE_APP_CLIENT_CPP_SRC_BASE64_H_
#define FIREBASE_APP_CLIENT_CPP_SRC_BASE64_H_

#include <string>

namespace firebase {
namespace internal {

// Base64 encode a string (binary allowed). Returns true if successful.
bool Base64Encode(const std::string& input, std::string* output);

// Base64 encode a string (binary allowed). Returns true if successful.
// Pads output to 32 bits by adding = or == to the end, as is tradition.
bool Base64EncodeWithPadding(const std::string& input, std::string* output);

// Base64 encode a string (binary allowed). Returns true if successful.
// Uses URL-safe characters (- and _ in place of + and /).
bool Base64EncodeUrlSafe(const std::string& input, std::string* output);

// Base64 encode a string (binary allowed). Returns true if successful.
// Pads output to 32 bits by adding = or == to the end, as is tradition.
// Uses URL-safe characters (- and _ in place of + and /).
bool Base64EncodeUrlSafeWithPadding(const std::string& input,
                                    std::string* output);

// Get the size that a given string would take up if encoded to Base64, rounded
// up to the next 4 bytes.
size_t GetBase64EncodedSize(const std::string& input);

// Base64 decode a string (may output binary). Returns true if successful, or
// false if there is an error (in which case the output string is undefined).
// Can parse standard or url-safe characters.
bool Base64Decode(const std::string& input, std::string* output);

// Get the size that a given string would take up if decoded from Base64.
// Returns 0 if the given string is not a valid Base64 size (or, of course, if
// you passed in the empty string.).
size_t GetBase64DecodedSize(const std::string& input);

}  // namespace internal
}  // namespace firebase

#endif  // FIREBASE_APP_CLIENT_CPP_SRC_BASE64_H_
