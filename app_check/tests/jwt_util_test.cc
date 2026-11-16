// Copyright 2026 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "app_check/src/common/jwt_util.h"

#include <cstdint>
#include <string>
#include <string_view>

#include "app/src/base64.h"
#include "firebase/app_check.h"
#include "gtest/gtest.h"

namespace firebase::app_check::internal {
namespace {

std::string Base64UrlEncode(std::string_view input) {
  std::string encoded;
  ::firebase::internal::Base64EncodeUrlSafe(std::string(input), &encoded);
  return encoded;
}

std::string MakeJwt(
    std::string_view payload_json,
    std::string_view header_json = R"({"typ":"JWT","alg":"RS256"})",
    std::string_view signature = "sig") {
  return Base64UrlEncode(header_json) + "." + Base64UrlEncode(payload_json) +
         "." + std::string(signature);
}

TEST(JwtUtilTest, AppCheckTokenInitializesExpireTimeMillisToZero) {
  AppCheckToken default_token;
  EXPECT_TRUE(default_token.token.empty());
  EXPECT_EQ(default_token.expire_time_millis, 0);

  AppCheckToken value_init_token{};
  EXPECT_TRUE(value_init_token.token.empty());
  EXPECT_EQ(value_init_token.expire_time_millis, 0);

  AppCheckToken aggregate_token{"jwt.payload.sig", 1700003600000LL};
  EXPECT_EQ(aggregate_token.token, "jwt.payload.sig");
  EXPECT_EQ(aggregate_token.expire_time_millis, 1700003600000LL);
}

TEST(JwtUtilTest, ExtractsExpirationFromValidAppCheckJwt) {
  const std::string jwt = MakeJwt(R"({
    "iss": "https://firebaseappcheck.googleapis.com/123456789",
    "sub": "1:123456789:ios:abcdef",
    "aud": ["projects/123456789", "projects/my-project"],
    "provider": "app_attest",
    "iat": 1700000000,
    "exp": 1700003600
  })");

  const AppCheckToken token = AppCheckTokenFromJwt(jwt);
  EXPECT_EQ(token.token, jwt);
  EXPECT_EQ(token.expire_time_millis, 1700003600000LL);
}

TEST(JwtUtilTest, SupportsFractionalAndScientificNotationExp) {
  EXPECT_EQ(AppCheckTokenFromJwt(MakeJwt(R"({"exp":1700003600.25})"))
                .expire_time_millis,
            1700003600250LL);
  EXPECT_EQ(
      AppCheckTokenFromJwt(MakeJwt(R"({"exp":0.001})")).expire_time_millis,
      1LL);
  EXPECT_EQ(
      AppCheckTokenFromJwt(MakeJwt(R"({"exp":1.7e9})")).expire_time_millis,
      1700000000000LL);
}

TEST(JwtUtilTest, HandlesNestedClaimsAndEscapedStringsWithoutShadowing) {
  const std::string nested_and_top_level = MakeJwt(R"({
    "sub": "quote\"backslash\\slash\/unicode\u0041",
    "nested": {"exp": 42, "arr": [1, {"exp": 99}, true, null]},
    "exp": 1700007200
  })");
  EXPECT_EQ(AppCheckTokenFromJwt(nested_and_top_level).expire_time_millis,
            1700007200000LL);

  const std::string only_nested_exp = MakeJwt(R"({
    "nested": {"exp": 1700007200}
  })");
  EXPECT_EQ(AppCheckTokenFromJwt(only_nested_exp).expire_time_millis, 0);
}

TEST(JwtUtilTest, HandlesBase64UrlPaddingAndEmptySignature) {
  const std::string mod2_jwt = MakeJwt(R"({"exp":123})", R"({"alg":"none"})");
  EXPECT_EQ(AppCheckTokenFromJwt(mod2_jwt).expire_time_millis, 123000LL);

  const std::string mod3_jwt = MakeJwt(R"({"exp":1234})", R"({"alg":"none"})");
  EXPECT_EQ(AppCheckTokenFromJwt(mod3_jwt).expire_time_millis, 1234000LL);

  // Unsecured JWS ("alg":"none") allows an empty signature segment.
  const std::string unsecured_jwt =
      MakeJwt(R"({"exp":500})", R"({"alg":"none"})", "");
  EXPECT_EQ(AppCheckTokenFromJwt(unsecured_jwt).expire_time_millis, 500000LL);

  // Standard Base64 '+'/'/' and '=' padding should also decode cleanly.
  // {"exp":1,"s":"?>?"} -> eyJleHAiOjEsInMiOiI/Pj8ifQ==
  const std::string padded_jwt =
      "eyJhbGciOiJub25lIn0.eyJleHAiOjEsInMiOiI/Pj8ifQ==.sig";
  EXPECT_EQ(AppCheckTokenFromJwt(padded_jwt).expire_time_millis, 1000LL);
}

TEST(JwtUtilTest, RejectsMalformedJwtStructureAndInvalidBase64) {
  const std::string high_bit_jwt =
      std::string("eyJhbGciOiJub25lIn0.eyJleHAiOj") + '\x80' + "=.sig";
  const std::string nul_payload_jwt =
      Base64UrlEncode(R"({"alg":"none"})") + "." +
      Base64UrlEncode(std::string_view("{\"exp\":100}\0", 12)) + ".sig";

  for (std::string_view malformed : {
           std::string_view(""),
           std::string_view("not-a-jwt"),
           std::string_view("header.payload"),
           std::string_view("a.b.c.d"),
           std::string_view(".eyJleHAiOjEwMH0.sig"),
           std::string_view("eyJhbGciOiJub25lIn0..sig"),
           std::string_view("eyJhbGciOiJub25lIn0.abcde.sig"),
           std::string_view(high_bit_jwt),
           std::string_view(nul_payload_jwt),
       }) {
    const AppCheckToken token = AppCheckTokenFromJwt(malformed);
    EXPECT_EQ(token.token, malformed);
    EXPECT_EQ(token.expire_time_millis, 0);
  }
}

TEST(JwtUtilTest, RejectsInvalidJsonPayloadsAndInvalidExpValues) {
  // Latest accepted expiration: 9999-12-31T23:59:59Z.
  EXPECT_EQ(AppCheckTokenFromJwt(MakeJwt(R"({"exp":253402300799})"))
                .expire_time_millis,
            253402300799000LL);
  EXPECT_EQ(AppCheckTokenFromJwt(MakeJwt(R"({"exp":253402300799.5})"))
                .expire_time_millis,
            253402300799500LL);

  for (std::string_view bad_payload : {
           std::string_view(R"([1,2,3])"),
           std::string_view(R"("string")"),
           std::string_view(R"(123)"),
           std::string_view(R"({"exp":100)"),
           std::string_view(R"({exp:100})"),
           std::string_view(R"({"exp":100}extra)"),
           std::string_view(R"({"exp":100,"exp":200})"),
           std::string_view(R"({"iat":1700000000})"),
           std::string_view(R"({"exp":"1700003600"})"),
           std::string_view(R"({"exp":null})"),
           std::string_view(R"({"exp":true})"),
           std::string_view(R"({"exp":{}})"),
           std::string_view(R"({"exp":[]})"),
           std::string_view(R"({"exp":0})"),
           std::string_view(R"({"exp":-100})"),
           std::string_view(R"({"exp":0.0001})"),
           std::string_view(R"({"exp":253402300800})"),
           std::string_view(R"({"exp":253402300800.0})"),
           std::string_view(R"({"exp":1700003600000})"),
           std::string_view(R"({"exp":9223372036854776})"),
           std::string_view(R"({"exp":999999999999999999999})"),
           std::string_view(R"({"exp":1e30})"),
       }) {
    const std::string jwt = MakeJwt(bad_payload);
    const AppCheckToken token = AppCheckTokenFromJwt(jwt);
    EXPECT_EQ(token.token, jwt) << bad_payload;
    EXPECT_EQ(token.expire_time_millis, 0) << bad_payload;
  }
}

}  // namespace
}  // namespace firebase::app_check::internal
