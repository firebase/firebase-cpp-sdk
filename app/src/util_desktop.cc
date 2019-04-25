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

#include "app/src/util_desktop.h"

#include "app/src/log.h"
#include "openssl/base64.h"

namespace firebase {

bool Base64Encode(const std::string& input, std::string* output) {
  size_t base64_length = 0;
  if ((EVP_EncodedLength(&base64_length, input.size()) == 0) ||
      (base64_length == 0)) {
    LogError("Failed base64 EVP_EncodedLength(%s)", input.c_str());
    return false;
  }
  output->resize(base64_length);
  if (EVP_EncodeBlock(reinterpret_cast<uint8_t*>(&(*output)[0]),
                      reinterpret_cast<const uint8_t*>(&input[0]),
                      input.size()) == 0u) {
    LogError("Failed base64 EVP_EncodeBlock()");
    return false;
  }
  // Trim the terminating null character.
  output->resize(base64_length - 1);
  return true;
}

bool Base64Decode(const std::string& input, std::string* output) {
  size_t decoded_length = 0;
  if ((EVP_DecodedLength(&decoded_length, input.size()) == 0) ||
      (decoded_length == 0)) {
    LogError("Failed to get decoded length for input(%s)", input.c_str());
    return false;
  }
  std::string buffer;
  buffer.resize(decoded_length);
  if (EVP_DecodeBase64(reinterpret_cast<uint8_t*>(&(buffer)[0]),
                       &decoded_length, decoded_length,
                       reinterpret_cast<const uint8_t*>(&(input)[0]),
                       input.size()) == 0) {
    LogError("Failed to decode input");
    return false;
  }
  // Decoded length includes null termination, remove.
  buffer.resize(buffer.size() - 1);
  output->assign(buffer);
  return true;
}

}  // namespace firebase
