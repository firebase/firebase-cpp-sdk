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

#include <cassert>
#include <string>

#include "app/src/base64.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  // Test encoding and decoding this string with various permutations of
  // options.
  std::string orig(reinterpret_cast<const char *>(data), size);
  std::string encoded, decoded;
  bool success;
  success = firebase::internal::Base64Encode(orig, &encoded);
  assert(success);
  success = firebase::internal::Base64Decode(encoded, &decoded);
  assert(success);
  assert(orig == decoded);
  success = firebase::internal::Base64EncodeWithPadding(orig, &encoded);
  assert(success);
  success = firebase::internal::Base64Decode(encoded, &decoded);
  assert(success);
  assert(orig == decoded);
  success = firebase::internal::Base64EncodeUrlSafe(orig, &encoded);
  assert(success);
  success = firebase::internal::Base64Decode(encoded, &decoded);
  assert(success);
  assert(orig == decoded);
  success = firebase::internal::Base64EncodeUrlSafeWithPadding(orig, &encoded);
  assert(success);
  success = firebase::internal::Base64Decode(encoded, &decoded);
  assert(success);
  assert(orig == decoded);

  // Test passing in this string to the decoder, and make sure it doesn't crash.
  std::string unused;
  firebase::internal::Base64Decode(orig, &unused);
  return 0;
}
