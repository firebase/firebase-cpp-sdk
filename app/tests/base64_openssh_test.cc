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

#include "app/src/base64.h"
#include "app/src/log.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "absl/strings/string_view.h"
#include "openssl/base64.h"

namespace firebase {
namespace internal {

size_t OpenSSHEncodedLength(size_t input_size) {
  size_t length;
  if (!EVP_EncodedLength(&length, input_size)) {
    return 0;
  }
  return length;
}

bool OpenSSHEncode(absl::string_view input, std::string* output) {
  size_t base64_length = OpenSSHEncodedLength(input.size());
  output->resize(base64_length);
  if (EVP_EncodeBlock(reinterpret_cast<uint8_t*>(&(*output)[0]),
                      reinterpret_cast<const uint8_t*>(&input[0]),
                      input.size()) == 0u) {
    return false;
  }
  // Trim the terminating null character.
  output->resize(base64_length - 1);
  return true;
}

size_t OpenSSHDecodedLength(size_t input_size) {
  size_t length;
  if (!EVP_DecodedLength(&length, input_size)) {
    return 0;
  }
  return length;
}

bool OpenSSHDecode(absl::string_view input, std::string* output) {
  size_t decoded_length = OpenSSHDecodedLength(input.size());
  output->resize(decoded_length);
  if (EVP_DecodeBase64(reinterpret_cast<uint8_t*>(&(*output)[0]),
                       &decoded_length, decoded_length,
                       reinterpret_cast<const uint8_t*>(&(input)[0]),
                       input.size()) == 0) {
    return false;
  }
  // Decoded length includes null termination, remove.
  output->resize(decoded_length);
  return true;
}

TEST(Base64TestAgainstOpenSSH, TestEncodingAgainstOpenSSH) {
  // Run this test 100 times.
  for (int i = 0; i < 100; i++) {
    // Generate 1-10000 random bytes. OpenSSH can't encode an empty string.
    size_t bytes = 1 + rand() % 9999;  // NOLINT
    std::string orig;
    orig.resize(bytes);
    for (int c = 0; c < orig.size(); ++c) {
      orig[c] = rand() % 0xFF;  // NOLINT
    }

    std::string encoded_firebase, encoded_openssh;
    ASSERT_TRUE(Base64EncodeWithPadding(orig, &encoded_firebase));
    ASSERT_TRUE(OpenSSHEncode(orig, &encoded_openssh));
    EXPECT_EQ(encoded_firebase, encoded_openssh)
        << "Encoding mismatch on source buffer: " << orig;

    std::string decoded_firebase_to_openssh;
    std::string decoded_openssh_to_firebase;
    ASSERT_TRUE(Base64Decode(encoded_openssh, &decoded_openssh_to_firebase));
    ASSERT_TRUE(OpenSSHDecode(encoded_firebase, &decoded_firebase_to_openssh));
    EXPECT_EQ(decoded_openssh_to_firebase, decoded_firebase_to_openssh)
        << "Cross-decoding mismatch on source buffer: " << orig;
    EXPECT_EQ(orig, decoded_firebase_to_openssh);
    EXPECT_EQ(orig, decoded_openssh_to_firebase);
  }
}

}  // namespace internal
}  // namespace firebase
