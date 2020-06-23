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

#include "gtest/gtest.h"
#include "gmock/gmock.h"

namespace firebase {
namespace internal {

TEST(Base64Test, EncodeAndDecodeText) {
  // Test 3 different lengths of string to ensure that trailing = is handled
  // correctly.
  const std::string kOrig0("Hello, world!"), kEncoded0("SGVsbG8sIHdvcmxkIQ");
  const std::string kOrig1("How are you?"), kEncoded1("SG93IGFyZSB5b3U/");
  const std::string kOrig2("I'm fine..."), kEncoded2("SSdtIGZpbmUuLi4");

  std::string encoded, decoded;
  EXPECT_TRUE(Base64Encode(kOrig0, &encoded));
  EXPECT_EQ(encoded, kEncoded0);
  EXPECT_TRUE(Base64Decode(encoded, &decoded)) << "Couldn't decode " << encoded;
  EXPECT_EQ(decoded, kOrig0);

  EXPECT_TRUE(Base64Encode(kOrig1, &encoded));
  EXPECT_EQ(encoded, kEncoded1);
  EXPECT_TRUE(Base64Decode(encoded, &decoded)) << "Couldn't decode " << encoded;
  EXPECT_EQ(decoded, kOrig1);

  EXPECT_TRUE(Base64Encode(kOrig2, &encoded));
  EXPECT_EQ(encoded, kEncoded2);
  EXPECT_TRUE(Base64Decode(encoded, &decoded)) << "Couldn't decode " << encoded;
  EXPECT_EQ(decoded, kOrig2);
}

TEST(Base64Test, EncodeAndDecodeTextWithPadding) {
  // Test 3 different lengths of string to ensure that trailing = is handled
  // correctly.
  const std::string kOrig0("Hello, world!"), kEncoded0("SGVsbG8sIHdvcmxkIQ==");
  const std::string kOrig1("How are you?"), kEncoded1("SG93IGFyZSB5b3U/");
  const std::string kOrig2("I'm fine..."), kEncoded2("SSdtIGZpbmUuLi4=");

  std::string encoded, decoded;
  EXPECT_TRUE(Base64EncodeWithPadding(kOrig0, &encoded));
  EXPECT_EQ(encoded, kEncoded0);
  EXPECT_TRUE(Base64Decode(encoded, &decoded)) << "Couldn't decode " << encoded;
  EXPECT_EQ(decoded, kOrig0);

  EXPECT_TRUE(Base64EncodeWithPadding(kOrig1, &encoded));
  EXPECT_EQ(encoded, kEncoded1);
  EXPECT_TRUE(Base64Decode(encoded, &decoded)) << "Couldn't decode " << encoded;
  EXPECT_EQ(decoded, kOrig1);

  EXPECT_TRUE(Base64EncodeWithPadding(kOrig2, &encoded));
  EXPECT_EQ(encoded, kEncoded2);
  EXPECT_TRUE(Base64Decode(encoded, &decoded)) << "Couldn't decode " << encoded;
  EXPECT_EQ(decoded, kOrig2);
}

TEST(Base64Test, SmallEncodeAndDecode) {
  const std::string kEmpty;
  std::string encoded, decoded;
  EXPECT_TRUE(Base64Encode(kEmpty, &encoded));
  EXPECT_EQ(encoded, kEmpty);
  EXPECT_TRUE(Base64Decode(encoded, &decoded)) << "Couldn't decode empty";
  EXPECT_EQ(decoded, kEmpty);

  EXPECT_TRUE(Base64EncodeWithPadding("\xFF", &encoded));
  EXPECT_TRUE(Base64Decode(encoded, &decoded)) << "Couldn't decode " << encoded;
  EXPECT_EQ(decoded, "\xFF");
  EXPECT_TRUE(Base64EncodeWithPadding("\xFF\xA0", &encoded));
  EXPECT_TRUE(Base64Decode(encoded, &decoded)) << "Couldn't decode " << encoded;
  EXPECT_EQ(decoded, "\xFF\xA0");
}

TEST(Base64Test, FullCharacterSet) {
  // Ensure all 64 possible characters are properly parsed in all 4 positions.
  const std::string kEncoded(
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"
      "BCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/A"
      "CDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/AB"
      "DEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/ABC");
  std::string decoded, encoded;
  EXPECT_TRUE(Base64Decode(kEncoded, &decoded))
      << "Couldn't decode " << kEncoded;
  EXPECT_TRUE(Base64EncodeWithPadding(decoded, &encoded));
  EXPECT_EQ(encoded, kEncoded);
}

TEST(Base64Test, BinaryEncodeAndDecode) {
  // Check binary string.
  const char kBinaryData[] =
      "\x00\x05\x20\x3C\x40\x45\x50\x60\x70\x80\x90\x00\xA0\xB5\xC2\xD1\xF0"
      "\xFF\x00\xE0\x42";

  const std::string kBinaryOrig(kBinaryData, sizeof(kBinaryData) - 1);
  const std::string kBinaryEncoded = "AAUgPEBFUGBwgJAAoLXC0fD/AOBC";
  std::string encoded, decoded;

  EXPECT_TRUE(Base64Encode(kBinaryOrig, &encoded));
  EXPECT_EQ(encoded, kBinaryEncoded);
  EXPECT_TRUE(Base64Decode(encoded, &decoded)) << "Couldn't decode " << encoded;
  EXPECT_EQ(decoded, kBinaryOrig);
}

TEST(Base64Test, InPlaceEncodeAndDecode) {
  const std::string kOrig("Hello, world!"), kEncoded("SGVsbG8sIHdvcmxkIQ"),
      kEncodedWithPadding("SGVsbG8sIHdvcmxkIQ==");

  // Ensure we can encode and decode in-place in the same buffer.
  std::string buffer = kOrig;
  EXPECT_TRUE(Base64Encode(buffer, &buffer));
  EXPECT_EQ(buffer, kEncoded);
  EXPECT_TRUE(Base64Decode(buffer, &buffer));
  EXPECT_EQ(buffer, kOrig);
  EXPECT_TRUE(Base64EncodeWithPadding(buffer, &buffer));
  EXPECT_EQ(buffer, kEncodedWithPadding);
  EXPECT_TRUE(Base64Decode(buffer, &buffer));
  EXPECT_EQ(buffer, kOrig);
}

TEST(Base64Test, FailToEncode) {
  EXPECT_FALSE(Base64Encode("Hello", nullptr));
  EXPECT_FALSE(Base64EncodeWithPadding("Hello", nullptr));
}

TEST(Base64Test, FailToDecode) {
  // Test some malformed base64.
  std::string unused;
  EXPECT_FALSE(Base64Decode("BadCharacterCountHere", &unused));
  EXPECT_FALSE(Base64Decode("HasEqual=SignInTheMiddle", &unused));
  EXPECT_FALSE(Base64Decode("EqualsFourFromEndA==AAAA", &unused));
  EXPECT_FALSE(Base64Decode("EqualsFourFromEndAA=AAAA", &unused));
  EXPECT_FALSE(Base64Decode("HasTooManyEqualsSignA===", &unused));
  EXPECT_FALSE(Base64Decode("PenultimateEqualsOnlyO=o", &unused));
  EXPECT_FALSE(Base64Decode("HasAnIncompatible$Symbol", &unused));

  // Decoding should fail if there are any dangling '1' bits past the end of the
  // encoded text.
  EXPECT_FALSE(Base64Decode("ExtraLowBitsAtTheEnd0a==", &unused));
  EXPECT_FALSE(Base64Decode("ExtraLowBitsAtTheEnd0a", &unused));
  EXPECT_FALSE(Base64Decode("ExtraLowBitsAtTheEnd0a/=", &unused));
  EXPECT_FALSE(Base64Decode("ExtraLowBitsAtTheEnd0a/", &unused));

  // Too short.
  EXPECT_FALSE(Base64Decode("a", &unused));

  // Test passing in nullptr as output.
  EXPECT_FALSE(Base64Decode("abcd", nullptr));
}

TEST(Base64Test, TestSizeCalculations) {
  EXPECT_EQ(GetBase64EncodedSize(""), 0);
  EXPECT_EQ(GetBase64EncodedSize("a"), 4);
  EXPECT_EQ(GetBase64EncodedSize("aa"), 4);
  EXPECT_EQ(GetBase64EncodedSize("aaa"), 4);
  EXPECT_EQ(GetBase64EncodedSize("aaaa"), 8);
  EXPECT_EQ(GetBase64EncodedSize("aaaaa"), 8);
  EXPECT_EQ(GetBase64EncodedSize("aaaaaa"), 8);
  EXPECT_EQ(GetBase64EncodedSize("aaaaaaa"), 12);

  EXPECT_EQ(GetBase64DecodedSize(""), 0);
  EXPECT_EQ(GetBase64DecodedSize("A"), 0);
  EXPECT_EQ(GetBase64DecodedSize("AA"), 1);
  EXPECT_EQ(GetBase64DecodedSize("AA=="), 1);
  EXPECT_EQ(GetBase64DecodedSize("AAA"), 2);
  EXPECT_EQ(GetBase64DecodedSize("AAA="), 2);
  EXPECT_EQ(GetBase64DecodedSize("AAAA"), 3);
  EXPECT_EQ(GetBase64DecodedSize("AAAAA"), 0);
  EXPECT_EQ(GetBase64DecodedSize("AAAAAA"), 4);
  EXPECT_EQ(GetBase64DecodedSize("AAAAAA=="), 4);
  EXPECT_EQ(GetBase64DecodedSize("AAAAAAA"), 5);
  EXPECT_EQ(GetBase64DecodedSize("AAAAAAA="), 5);
  EXPECT_EQ(GetBase64DecodedSize("AAAAAAAA"), 6);
}

TEST(Base64Test, TestUrlSafeEncoding) {
  const std::string kEncoded(
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"
      "BCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/A"
      "CDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/AB"
      "DEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/ABCAA");
  const std::string kEncodedUrlSafe(
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_"
      "BCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_A"
      "CDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_AB"
      "DEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_ABCAA");
  const std::string kEncodedUrlSafeWithPadding(
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_"
      "BCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_A"
      "CDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_AB"
      "DEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_ABCAA==");
  std::string decoded, decoded_urlsafe;
  EXPECT_TRUE(Base64Decode(kEncoded, &decoded));
  EXPECT_TRUE(Base64Decode(kEncodedUrlSafe, &decoded_urlsafe));
  EXPECT_EQ(decoded_urlsafe, decoded);

  std::string encoded_urlsafe;
  EXPECT_TRUE(Base64EncodeUrlSafe(decoded, &encoded_urlsafe));
  EXPECT_EQ(encoded_urlsafe, kEncodedUrlSafe);

  std::string encoded_urlsafe_padded;
  EXPECT_TRUE(Base64EncodeUrlSafeWithPadding(decoded, &encoded_urlsafe_padded));
  EXPECT_EQ(encoded_urlsafe_padded, kEncodedUrlSafeWithPadding);
}

}  // namespace internal
}  // namespace firebase
