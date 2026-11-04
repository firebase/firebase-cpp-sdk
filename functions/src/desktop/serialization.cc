// Copyright 2018 Google LLC
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

#include "functions/src/desktop/serialization.h"

#include <charconv>
#include <cmath>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "app/src/include/firebase/variant.h"
#include "app/src/log.h"

namespace firebase::functions::internal {
namespace {
inline constexpr char kTypeLong[] =
    "type.googleapis.com/google.protobuf.Int64Value";
inline constexpr char kTypeLongLegacy[] =
    "type.google.com/google.protobuf.Int64Value";
inline constexpr char kTypeUnsignedLong[] =
    "type.googleapis.com/google.protobuf.UInt64Value";
inline constexpr char kTypeUnsignedLongLegacy[] =
    "type.google.com/google.protobuf.UInt64Value";

template <typename IntT>
Variant ParseProtoInt(const Variant& val_var, const char* type_str) {
  if (!val_var.is_string()) {
    LogError("Invalid value of type %s for %s",
             Variant::TypeName(val_var.type()), type_str);
    return Variant::Null();
  }
  std::string_view sv = val_var.string_value();
  while (!sv.empty() && (sv.front() == ' ' || sv.front() == '\t' ||
                         sv.front() == '\n' || sv.front() == '\r')) {
    sv.remove_prefix(1);
  }
  while (!sv.empty() && (sv.back() == ' ' || sv.back() == '\t' ||
                         sv.back() == '\n' || sv.back() == '\r')) {
    sv.remove_suffix(1);
  }
  if (!sv.empty() && sv.front() == '+') sv.remove_prefix(1);
  IntT val = 0;
  const char* begin = sv.data();
  const char* end = begin + sv.size();
  auto [ptr, ec] = std::from_chars(begin, end, val);
  if (sv.empty() || ec != std::errc() || ptr != end) {
    LogError("Failed to parse %s from %s", type_str, val_var.string_value());
    return Variant::Null();
  }
  return Variant(static_cast<int64_t>(val));
}
}  // namespace

Variant Encode(const Variant& variant) {
  switch (variant.type()) {
    case Variant::kTypeNull:
    case Variant::kTypeBool:
    case Variant::kTypeStaticString:
    case Variant::kTypeMutableString:
      return variant;
    case Variant::kTypeDouble:
      if (!std::isfinite(variant.double_value())) {
        LogError("Cannot serialize non-finite double to JSON.");
        return Variant::Null();
      }
      return variant;
    case Variant::kTypeInt64: {
      Variant map = Variant::EmptyMap();
      map.map()["@type"] = kTypeLong;
      map.map()["value"] = std::to_string(variant.int64_value());
      return map;
    }
    case Variant::kTypeVector: {
      Variant list = Variant::EmptyVector();
      list.vector().reserve(variant.vector().size());
      for (const Variant& element : variant.vector()) {
        Variant value = Encode(element);
        if (value.is_null() && !element.is_null()) return Variant::Null();
        list.vector().push_back(value);
      }
      return list;
    }
    case Variant::kTypeMap: {
      Variant map = Variant::EmptyMap();
      for (const auto& [key, raw_val] : variant.map()) {
        if (!key.is_fundamental_type()) {
          LogError("Cannot serialize map with non-fundamental key type %s",
                   Variant::TypeName(key.type()));
          return Variant::Null();
        }
        Variant v = Encode(raw_val);
        if (v.is_null() && !raw_val.is_null()) return Variant::Null();
        map.map()[key.AsString()] = v;
      }
      return map;
    }
    default:
      LogError("Cannot serialize unrecognized Variant of type %s",
               Variant::TypeName(variant.type()));
      return Variant::Null();
  }
}

Variant Decode(const Variant& variant) {
  switch (variant.type()) {
    case Variant::kTypeNull:
    case Variant::kTypeInt64:
    case Variant::kTypeDouble:
    case Variant::kTypeBool:
    case Variant::kTypeStaticString:
    case Variant::kTypeMutableString:
      return variant;
    case Variant::kTypeMap: {
      if (auto type_it = variant.map().find("@type");
          type_it != variant.map().end()) {
        if (!type_it->second.is_string()) {
          LogError("Invalid @type of type %s",
                   Variant::TypeName(type_it->second.type()));
          return Variant::Null();
        }
        std::string_view type = type_it->second.string_value();
        auto val_it = variant.map().find("value");
        if (val_it == variant.map().end()) {
          LogError("No value field in %s", type_it->second.string_value());
          return Variant::Null();
        }
        if (type == kTypeLong || type == kTypeLongLegacy) {
          return ParseProtoInt<int64_t>(val_it->second,
                                        type_it->second.string_value());
        }
        if (type == kTypeUnsignedLong || type == kTypeUnsignedLongLegacy) {
          return ParseProtoInt<uint64_t>(val_it->second,
                                         type_it->second.string_value());
        }
      }
      Variant map = Variant::EmptyMap();
      for (const auto& [key, raw_val] : variant.map()) {
        if (!key.is_string()) {
          LogError("Map has non-string key of type %s",
                   Variant::TypeName(key.type()));
          return Variant::Null();
        }
        Variant v = Decode(raw_val);
        if (v.is_null() && !raw_val.is_null()) return Variant::Null();
        map.map()[key] = v;
      }
      return map;
    }
    case Variant::kTypeVector: {
      Variant list = Variant::EmptyVector();
      list.vector().reserve(variant.vector().size());
      for (const Variant& element : variant.vector()) {
        Variant value = Decode(element);
        if (value.is_null() && !element.is_null()) return Variant::Null();
        list.vector().push_back(value);
      }
      return list;
    }
    default:
      LogError("Cannot deserialize unrecognized Variant of type %s",
               Variant::TypeName(variant.type()));
      return Variant::Null();
  }
}

}  // namespace firebase::functions::internal
