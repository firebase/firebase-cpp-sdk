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

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "app/src/base64.h"
#include "flatbuffers/flexbuffers.h"
#include "flatbuffers/idl.h"

namespace firebase::app_check::internal {
namespace {

constexpr bool IsAscii(char c) noexcept {
  return static_cast<unsigned char>(c) < 128;
}

constexpr bool IsJsonWhitespace(char c) noexcept {
  return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

// Extracts the Base64URL payload segment from a JWS compact serialization
// (`header.payload.signature`).
std::optional<std::string_view> ExtractPayloadSegment(
    std::string_view jwt) noexcept {
  const size_t first_dot = jwt.find('.');
  if (first_dot == 0 || first_dot == std::string_view::npos) {
    return std::nullopt;
  }
  const size_t payload_start = first_dot + 1;
  const size_t second_dot = jwt.find('.', payload_start);
  if (second_dot == std::string_view::npos || second_dot == payload_start) {
    return std::nullopt;
  }
  if (jwt.find('.', second_dot + 1) != std::string_view::npos) {
    return std::nullopt;
  }
  return jwt.substr(payload_start, second_dot - payload_start);
}

std::optional<std::string> DecodeBase64Payload(std::string_view encoded) {
  // Base64Decode indexes a lookup table with signed char, so reject non-ASCII.
  if (!std::all_of(encoded.begin(), encoded.end(), IsAscii)) {
    return std::nullopt;
  }
  std::string decoded;
  if (!::firebase::internal::Base64Decode(std::string(encoded), &decoded)) {
    return std::nullopt;
  }
  if (decoded.find('\0') != std::string::npos) {
    return std::nullopt;
  }
  return decoded;
}

// Latest expiration treated as valid: 9999-12-31T23:59:59Z. Anything later is
// a malformed claim (for example `exp` written in milliseconds) and is not
// representable by common date types such as .NET's DateTime.
constexpr std::chrono::seconds kMaxExpiration{253402300799};

std::optional<std::chrono::milliseconds> ExtractExpiration(
    const flexbuffers::Map& claims) noexcept {
  const flexbuffers::Reference exp = claims["exp"];
  if (exp.IsIntOrUint()) {
    const std::chrono::seconds seconds{exp.AsInt64()};
    if (seconds <= std::chrono::seconds::zero() || seconds > kMaxExpiration) {
      return std::nullopt;
    }
    return std::chrono::milliseconds{seconds};
  }
  if (exp.IsFloat()) {
    constexpr double kMaxMillisExclusive = static_cast<double>(
        std::chrono::milliseconds{kMaxExpiration + std::chrono::seconds{1}}
            .count());
    const double millis = exp.AsDouble() * 1000.0;
    if (!std::isfinite(millis) || millis < 1.0 ||
        millis >= kMaxMillisExclusive) {
      return std::nullopt;
    }
    return std::chrono::milliseconds{static_cast<int64_t>(millis)};
  }
  return std::nullopt;
}

std::optional<std::chrono::milliseconds> ParseJwtExpiration(
    std::string_view jwt) {
  const std::optional<std::string_view> payload_segment =
      ExtractPayloadSegment(jwt);
  if (!payload_segment) {
    return std::nullopt;
  }
  const std::optional<std::string> payload_json =
      DecodeBase64Payload(*payload_segment);
  if (!payload_json) {
    return std::nullopt;
  }

  flatbuffers::IDLOptions options;
  options.strict_json = true;
  options.use_flexbuffers = true;
  flatbuffers::Parser parser(options);
  if (!parser.Parse(payload_json->c_str())) {
    return std::nullopt;
  }

  const std::ptrdiff_t consumed = parser.BytesConsumed();
  if (consumed < 0 || static_cast<size_t>(consumed) > payload_json->size()) {
    return std::nullopt;
  }
  const std::string_view trailing =
      std::string_view(*payload_json).substr(static_cast<size_t>(consumed));
  if (!std::all_of(trailing.begin(), trailing.end(), IsJsonWhitespace)) {
    return std::nullopt;
  }

  const auto& buffer = parser.flex_builder_.GetBuffer();
  if (buffer.size() < 2) {
    return std::nullopt;
  }
  const flexbuffers::Reference root = flexbuffers::GetRoot(buffer);
  if (!root.IsMap()) {
    return std::nullopt;
  }
  return ExtractExpiration(root.AsMap());
}

}  // namespace

AppCheckToken AppCheckTokenFromJwt(std::string_view jwt) {
  AppCheckToken result;
  result.token = std::string(jwt);
  if (const auto expiration = ParseJwtExpiration(jwt)) {
    result.expire_time_millis = expiration->count();
  }
  return result;
}

}  // namespace firebase::app_check::internal
