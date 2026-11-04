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

#include "auth/src/id_token_util.h"

#include <cmath>
#include <cstdint>
#include <map>
#include <string>
#include <utility>

#include "app/src/base64.h"
#include "app/src/variant_util.h"

namespace firebase {
namespace auth {

namespace {

constexpr double kMaxUint64AsDouble = 18446744073709551616.0;  // 2^64

uint64_t ExtractTimestamp(const std::map<Variant, Variant>& claims,
                          const char* key) {
  auto it = claims.find(Variant(key));
  if (it != claims.end()) {
    if (it->second.is_int64() && it->second.int64_value() > 0) {
      return static_cast<uint64_t>(it->second.int64_value());
    } else if (it->second.is_double()) {
      const double value = it->second.double_value();
      if (std::isfinite(value) && value > 0.0 && value < kMaxUint64AsDouble) {
        return static_cast<uint64_t>(value);
      }
    }
  }
  return 0;
}

}  // namespace

bool ParseIdToken(const std::string& token, IdTokenResult* out_result) {
  if (!out_result) return false;
  *out_result = IdTokenResult();
  if (token.empty()) return false;

  const size_t first_dot = token.find('.');
  if (first_dot == std::string::npos) return false;
  const size_t second_dot = token.find('.', first_dot + 1);
  if (second_dot == std::string::npos ||
      token.find('.', second_dot + 1) != std::string::npos) {
    return false;
  }

  const std::string payload_b64 =
      token.substr(first_dot + 1, second_dot - (first_dot + 1));
  std::string payload_json;
  if (payload_b64.empty() ||
      !::firebase::internal::Base64Decode(payload_b64, &payload_json)) {
    return false;
  }

  const Variant payload_variant = util::JsonToVariant(payload_json.c_str());
  if (!payload_variant.is_map()) return false;

  IdTokenResult parsed;
  parsed.token = token;
  parsed.claims = payload_variant.map();
  parsed.auth_timestamp = ExtractTimestamp(parsed.claims, "auth_time");
  parsed.issued_at_timestamp = ExtractTimestamp(parsed.claims, "iat");
  parsed.expiration_timestamp = ExtractTimestamp(parsed.claims, "exp");

  const auto firebase_it = parsed.claims.find(Variant("firebase"));
  if (firebase_it != parsed.claims.end() && firebase_it->second.is_map()) {
    const auto& fb_map = firebase_it->second.map();
    auto get_str = [&](const char* k, std::string* dst) {
      auto it = fb_map.find(Variant(k));
      if (it != fb_map.end() && it->second.is_string()) {
        *dst = it->second.string_value();
      }
    };
    get_str("sign_in_provider", &parsed.sign_in_provider);
    get_str("sign_in_second_factor", &parsed.sign_in_second_factor);
  }

  *out_result = std::move(parsed);
  return true;
}

}  // namespace auth
}  // namespace firebase
