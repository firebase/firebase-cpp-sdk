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

#include <string>

#include "app/src/base64.h"
#include "auth/src/id_token_util.h"
#include "firebase/auth/user.h"
#include "firebase/variant.h"
#include "gtest/gtest.h"

namespace firebase {
namespace auth {
namespace {

std::string MakeJwt(const std::string& header_json,
                    const std::string& payload_json,
                    const std::string& signature = "sig") {
  std::string header_b64;
  std::string payload_b64;
  internal::Base64EncodeUrlSafe(header_json, &header_b64);
  internal::Base64EncodeUrlSafe(payload_json, &payload_b64);
  return header_b64 + "." + payload_b64 + "." + signature;
}

TEST(IdTokenResultTest, ParseValidTokenWithStandardClaims) {
  const std::string payload =
      "{"
      "\"auth_time\": 1600000000,"
      "\"iat\": 1600000001,"
      "\"exp\": 1600003600,"
      "\"sub\": \"user_12345\","
      "\"firebase\": {"
      "  \"sign_in_provider\": \"google.com\","
      "  \"sign_in_second_factor\": \"phone\""
      "}"
      "}";
  const std::string token = MakeJwt("{\"alg\":\"none\"}", payload);

  IdTokenResult result;
  EXPECT_TRUE(ParseIdToken(token, &result));
  EXPECT_EQ(token, result.token);
  EXPECT_EQ(1600000000ULL, result.auth_timestamp);
  EXPECT_EQ(1600000001ULL, result.issued_at_timestamp);
  EXPECT_EQ(1600003600ULL, result.expiration_timestamp);
  EXPECT_EQ("google.com", result.sign_in_provider);
  EXPECT_EQ("phone", result.sign_in_second_factor);

  auto sub_it = result.claims.find(Variant("sub"));
  ASSERT_NE(result.claims.end(), sub_it);
  EXPECT_TRUE(sub_it->second.is_string());
  EXPECT_STREQ("user_12345", sub_it->second.string_value());
}

TEST(IdTokenResultTest, ParseValidTokenWithCustomClaims) {
  const std::string payload =
      "{"
      "\"auth_time\": 1600000000,"
      "\"iat\": 1600000000,"
      "\"exp\": 1600003600,"
      "\"admin\": true,"
      "\"roles\": [\"editor\", \"viewer\"],"
      "\"user_tier\": 3,"
      "\"firebase\": {"
      "  \"sign_in_provider\": \"password\""
      "}"
      "}";
  const std::string token = MakeJwt("{\"alg\":\"none\"}", payload);

  IdTokenResult result;
  EXPECT_TRUE(ParseIdToken(token, &result));
  EXPECT_EQ("password", result.sign_in_provider);
  EXPECT_TRUE(result.sign_in_second_factor.empty());

  // Check boolean custom claim
  auto admin_it = result.claims.find(Variant("admin"));
  ASSERT_NE(result.claims.end(), admin_it);
  EXPECT_TRUE(admin_it->second.is_bool());
  EXPECT_TRUE(admin_it->second.bool_value());

  // Check integer custom claim
  auto tier_it = result.claims.find(Variant("user_tier"));
  ASSERT_NE(result.claims.end(), tier_it);
  EXPECT_TRUE(tier_it->second.is_int64());
  EXPECT_EQ(3, tier_it->second.int64_value());

  // Check vector custom claim
  auto roles_it = result.claims.find(Variant("roles"));
  ASSERT_NE(result.claims.end(), roles_it);
  EXPECT_TRUE(roles_it->second.is_vector());
  EXPECT_EQ(2, roles_it->second.vector().size());
  EXPECT_STREQ("editor", roles_it->second.vector()[0].string_value());
  EXPECT_STREQ("viewer", roles_it->second.vector()[1].string_value());
}

TEST(IdTokenResultTest, ParseBase64UrlSpecialCharsAndUnpaddedLengths) {
  // Any 3 consecutive '>' (0x3e) guarantees one at byte index % 3 == 2 -> '-',
  // and any 3 consecutive '?' (0x3f) guarantees one at byte index % 3 == 2 ->
  // '_'. Input length % 3 == 1 produces unpadded Base64 length % 4 == 2.
  const std::string payload_mod2 =
      "{\"url\":\">>>???\",\"iat\":1700000000,"
      "\"exp\":1700003600,\"auth_time\":1699999999,\"pad\":\"\"}";
  ASSERT_EQ(1U, payload_mod2.size() % 3);
  std::string payload_b64_mod2;
  internal::Base64EncodeUrlSafe(payload_mod2, &payload_b64_mod2);
  EXPECT_NE(std::string::npos, payload_b64_mod2.find('-'));
  EXPECT_NE(std::string::npos, payload_b64_mod2.find('_'));
  EXPECT_EQ(std::string::npos, payload_b64_mod2.find('='));
  EXPECT_EQ(2U, payload_b64_mod2.size() % 4);

  const std::string token_mod2 =
      "eyJhbGciOiJub25lIn0." + payload_b64_mod2 + ".sig";
  IdTokenResult result_mod2;
  EXPECT_TRUE(ParseIdToken(token_mod2, &result_mod2));
  EXPECT_EQ(1699999999ULL, result_mod2.auth_timestamp);
  EXPECT_EQ(1700000000ULL, result_mod2.issued_at_timestamp);
  EXPECT_EQ(1700003600ULL, result_mod2.expiration_timestamp);
  EXPECT_STREQ(">>>???", result_mod2.claims[Variant("url")].string_value());

  // Input length % 3 == 2 produces unpadded Base64 length % 4 == 3.
  const std::string payload_mod3 =
      "{\"url\":\">>>???\",\"iat\":1700000000,"
      "\"exp\":1700003600,\"auth_time\":1699999999,\"pad\":\"x\"}";
  ASSERT_EQ(2U, payload_mod3.size() % 3);
  std::string payload_b64_mod3;
  internal::Base64EncodeUrlSafe(payload_mod3, &payload_b64_mod3);
  EXPECT_EQ(3U, payload_b64_mod3.size() % 4);
  EXPECT_EQ(std::string::npos, payload_b64_mod3.find('='));

  const std::string token_mod3 =
      "eyJhbGciOiJub25lIn0." + payload_b64_mod3 + ".sig";
  IdTokenResult result_mod3;
  EXPECT_TRUE(ParseIdToken(token_mod3, &result_mod3));
  EXPECT_EQ(1700000000ULL, result_mod3.issued_at_timestamp);
  EXPECT_STREQ("x", result_mod3.claims[Variant("pad")].string_value());
}

TEST(IdTokenResultTest, NullPointerReturnsFalse) {
  EXPECT_FALSE(ParseIdToken("header.payload.sig", nullptr));
}

TEST(IdTokenResultTest, EmptyTokenReturnsFalse) {
  IdTokenResult result;
  EXPECT_FALSE(ParseIdToken("", &result));
}

TEST(IdTokenResultTest, MalformedTokenNoDots) {
  IdTokenResult result;
  EXPECT_FALSE(ParseIdToken("invalidtokenwithoutdots", &result));
}

TEST(IdTokenResultTest, MalformedTokenOnlyOneDot) {
  IdTokenResult result;
  EXPECT_FALSE(ParseIdToken("header.payload", &result));
}

TEST(IdTokenResultTest, MalformedTokenTooManyDots) {
  IdTokenResult result;
  EXPECT_FALSE(ParseIdToken("header.payload.sig.extra", &result));
}

TEST(IdTokenResultTest, MalformedTokenInvalidBase64) {
  IdTokenResult result;
  EXPECT_FALSE(ParseIdToken("header.???invalidbase64???.sig", &result));
}

TEST(IdTokenResultTest, MalformedTokenNotJsonPayload) {
  std::string b64;
  internal::Base64EncodeUrlSafe("this is not json", &b64);
  const std::string token = "header." + b64 + ".sig";

  IdTokenResult result;
  EXPECT_FALSE(ParseIdToken(token, &result));
}

TEST(IdTokenResultTest, MalformedTokenNonObjectJsonPayload) {
  std::string b64;
  internal::Base64EncodeUrlSafe("\"just a json string\"", &b64);
  const std::string token = "header." + b64 + ".sig";

  IdTokenResult result;
  EXPECT_FALSE(ParseIdToken(token, &result));
}

TEST(IdTokenResultTest, EqualityOperators) {
  IdTokenResult r1;
  r1.token = "token1";
  r1.auth_timestamp = 100;
  r1.issued_at_timestamp = 101;
  r1.expiration_timestamp = 102;
  r1.sign_in_provider = "google.com";
  r1.sign_in_second_factor = "phone";
  r1.claims[Variant("key")] = Variant("value");

  IdTokenResult r2 = r1;
  EXPECT_EQ(r1, r2);
  EXPECT_FALSE(r1 != r2);

  r2.token = "token2";
  EXPECT_NE(r1, r2);
  EXPECT_FALSE(r1 == r2);

  r2 = r1;
  r2.claims[Variant("key2")] = Variant(42);
  EXPECT_NE(r1, r2);

  r2 = r1;
  r2.auth_timestamp = 999;
  EXPECT_NE(r1, r2);

  r2 = r1;
  r2.sign_in_provider = "apple.com";
  EXPECT_NE(r1, r2);
}

}  // namespace
}  // namespace auth
}  // namespace firebase
