/*
 * Copyright 2017 Google LLC
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

#include "app/src/variant_util.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <sstream>

#include "app/src/log.h"
#include "flatbuffers/flatbuffers.h"
#include "flatbuffers/flexbuffers.h"
#include "flatbuffers/idl.h"
#include "flatbuffers/util.h"

#define FLEXBUFFER_BUILDER_STARTING_SIZE 512

namespace firebase {
namespace util {

namespace {
constexpr size_t kMaxVariantDepth = 128;
}  // namespace

// Forward declarations for stringstream variations of the *ToJson functions
// since these aren't made available in the header. These return true on success
// and false on failure. Failure is a result of using binary blobs in the
// variant, or using types that cannot be coerced to a string as a key in a map.
static bool VariantToJson(const Variant& variant, bool prettyPrint,
                          const std::string& indent, std::stringstream* ss,
                          size_t depth = 0);
static bool StdMapToJson(const std::map<Variant, Variant>& map,
                         bool prettyPrint, const std::string& indent,
                         std::stringstream* ss, size_t depth = 0);
static bool StdVectorToJson(const std::vector<Variant>& vector,
                            bool prettyPrint, const std::string& indent,
                            std::stringstream* ss, size_t depth = 0);

static bool VariantToJson(const Variant& variant, bool prettyPrint,
                          const std::string& indent, std::stringstream* ss,
                          size_t depth) {
  if (depth > kMaxVariantDepth) {
    LogError("Variant nesting depth exceeds maximum (%zu).", kMaxVariantDepth);
    return false;
  }
  switch (variant.type()) {
    case Variant::kTypeNull: {
      *ss << "null";
      break;
    }
    case Variant::kTypeInt64: {
      *ss << variant.int64_value();
      break;
    }
    case Variant::kTypeDouble: {
      // IEEE 754 double-precision binary floating-point format: binary64 — The
      // 53-bit significand precision gives from 15 to 17 significant decimal
      // digits Default 32-bit only keeps 7 digit precision
      double d = variant.double_value();
      if (!std::isfinite(d)) {
        *ss << "null";
      } else {
        // Normalize IEEE 754 negative zero (-0.0) to positive zero (+0.0).
        if (d == 0.0) {
          d = 0.0;
        }
        *ss << std::setprecision(17) << d;
      }
      break;
    }
    case Variant::kTypeBool: {
      *ss << (variant.bool_value() ? "true" : "false");
      break;
    }
    case Variant::kTypeStaticString:
    case Variant::kTypeMutableString: {
      std::string escaped_string;
      const char* str = variant.string_value();
      size_t len = variant.is_mutable_string() ? variant.mutable_string().size()
                                               : strlen(str);
      flatbuffers::EscapeString(str, len, &escaped_string, true, false);
      *ss << escaped_string;
      break;
    }
    case Variant::kTypeVector: {
      if (!StdVectorToJson(variant.vector(), prettyPrint, indent, ss, depth)) {
        return false;
      }
      break;
    }
    case Variant::kTypeMap: {
      if (!StdMapToJson(variant.map(), prettyPrint, indent, ss, depth)) {
        return false;
      }
      break;
    }
    case Variant::kTypeStaticBlob:
    case Variant::kTypeMutableBlob: {
      LogError("Variants containing blobs are not supported.");
      return false;
    }
  }
  return true;
}

static bool StdMapToJson(const std::map<Variant, Variant>& map,
                         bool prettyPrint, const std::string& indent,
                         std::stringstream* ss, size_t depth) {
  if (depth > kMaxVariantDepth) {
    LogError("Variant nesting depth exceeds maximum (%zu).", kMaxVariantDepth);
    return false;
  }
  *ss << '{';
  std::string nextIndent = indent + "  ";
  for (auto iter = map.begin(); iter != map.end();) {
    if (prettyPrint) {
      *ss << '\n' << nextIndent;
    }
    // JSON only supports string keys, return false if the key is not a type
    // that can be coerced to a string.
    if (iter->first.is_null() || !iter->first.is_fundamental_type()) {
      LogError(
          "Variants of non-fundamental types may not be used as map keys.");
      return false;
    }
    if (!VariantToJson(iter->first.AsString(), prettyPrint, nextIndent, ss,
                       depth + 1)) {
      return false;
    }
    *ss << ':';
    if (prettyPrint) {
      *ss << ' ';
    }
    if (!VariantToJson(iter->second, prettyPrint, nextIndent, ss, depth + 1)) {
      return false;
    }
    if (++iter != map.end()) {
      *ss << ',';
    }
  }
  if (prettyPrint) {
    *ss << '\n' << indent;
  }
  *ss << '}';
  return true;
}

static bool StdVectorToJson(const std::vector<Variant>& vector,
                            bool prettyPrint, const std::string& indent,
                            std::stringstream* ss, size_t depth) {
  if (depth > kMaxVariantDepth) {
    LogError("Variant nesting depth exceeds maximum (%zu).", kMaxVariantDepth);
    return false;
  }
  *ss << '[';
  std::string nextIndent = indent + "  ";
  for (auto iter = vector.begin(); iter != vector.end();) {
    if (prettyPrint) {
      *ss << '\n' << nextIndent;
    }
    if (!VariantToJson(*iter, prettyPrint, nextIndent, ss, depth + 1)) {
      return false;
    }
    if (++iter != vector.end()) {
      *ss << ',';
    }
  }
  if (prettyPrint) {
    *ss << '\n' << indent;
  }
  *ss << ']';
  return true;
}

std::string VariantToJson(const Variant& variant) {
  return VariantToJson(variant, false);
}

std::string VariantToJson(const Variant& variant, bool prettyPrint) {
  std::stringstream ss;
  if (!VariantToJson(variant, prettyPrint, "", &ss, 0)) {
    return "";
  }
  return ss.str();
}

// Converts an std::map<Variant, Variant> to Json
std::string StdMapToJson(const std::map<Variant, Variant>& map) {
  std::stringstream ss;
  if (!StdMapToJson(map, false, "", &ss, 0)) {
    return "";
  }
  return ss.str();
}

// Converts an std::vector<Variant> to Json
std::string StdVectorToJson(const std::vector<Variant>& vector) {
  std::stringstream ss;
  if (!StdVectorToJson(vector, false, "", &ss, 0)) {
    return "";
  }
  return ss.str();
}

static Variant FlexbufferToVariantInternal(const flexbuffers::Reference& ref,
                                           size_t depth);

static Variant FlexbufferVectorToVariantInternal(
    const flexbuffers::Vector& vector, size_t depth) {
  if (depth > kMaxVariantDepth) {
    LogError("Flexbuffer nesting depth exceeds maximum (%zu).",
             kMaxVariantDepth);
    return Variant::Null();
  }
  Variant result = Variant::EmptyVector();
  result.vector().reserve(vector.size());
  for (size_t i = 0; i < vector.size(); i++) {
    result.vector().push_back(
        FlexbufferToVariantInternal(vector[i], depth + 1));
  }
  return result;
}

static Variant FlexbufferMapToVariantInternal(const flexbuffers::Map& map,
                                              size_t depth) {
  if (depth > kMaxVariantDepth) {
    LogError("Flexbuffer nesting depth exceeds maximum (%zu).",
             kMaxVariantDepth);
    return Variant::Null();
  }
  Variant result = Variant::EmptyMap();
  flexbuffers::TypedVector keys = map.Keys();
  for (size_t i = 0; i < keys.size(); i++) {
    flexbuffers::Reference key = keys[i];
    flexbuffers::Reference value = map[key.AsKey()];
    result.map()[FlexbufferToVariantInternal(key, depth + 1)] =
        FlexbufferToVariantInternal(value, depth + 1);
  }
  return result;
}

static Variant FlexbufferToVariantInternal(const flexbuffers::Reference& ref,
                                           size_t depth) {
  if (depth > kMaxVariantDepth) {
    LogError("Flexbuffer nesting depth exceeds maximum (%zu).",
             kMaxVariantDepth);
    return Variant::Null();
  }
  switch (ref.GetType()) {
    case flexbuffers::FBT_NULL:
      return Variant::Null();
    case flexbuffers::FBT_BOOL:
      return Variant(ref.AsBool());
    case flexbuffers::FBT_INT:
    case flexbuffers::FBT_INDIRECT_INT:
      return Variant(ref.AsInt64());
    case flexbuffers::FBT_UINT:
    case flexbuffers::FBT_INDIRECT_UINT: {
      uint64_t u = ref.AsUInt64();
      if (u <= static_cast<uint64_t>(INT64_MAX)) {
        return Variant(static_cast<int64_t>(u));
      }
      return Variant(static_cast<double>(u));
    }
    case flexbuffers::FBT_FLOAT:
    case flexbuffers::FBT_INDIRECT_FLOAT:
      return Variant(ref.AsDouble());
    case flexbuffers::FBT_STRING: {
      flexbuffers::String str = ref.AsString();
      return Variant(std::string(str.c_str(), str.length()));
    }
    case flexbuffers::FBT_KEY:
      return Variant::MutableStringFromStaticString(ref.AsKey());
    case flexbuffers::FBT_MAP:
      return FlexbufferMapToVariantInternal(ref.AsMap(), depth);
    case flexbuffers::FBT_VECTOR_BOOL:
    case flexbuffers::FBT_VECTOR_FLOAT2:
    case flexbuffers::FBT_VECTOR_FLOAT3:
    case flexbuffers::FBT_VECTOR_FLOAT4:
    case flexbuffers::FBT_VECTOR_FLOAT:
    case flexbuffers::FBT_VECTOR_INT2:
    case flexbuffers::FBT_VECTOR_INT3:
    case flexbuffers::FBT_VECTOR_INT4:
    case flexbuffers::FBT_VECTOR_INT:
    case flexbuffers::FBT_VECTOR_KEY:
    case flexbuffers::FBT_VECTOR_STRING_DEPRECATED:
    case flexbuffers::FBT_VECTOR_UINT2:
    case flexbuffers::FBT_VECTOR_UINT3:
    case flexbuffers::FBT_VECTOR_UINT4:
    case flexbuffers::FBT_VECTOR_UINT:
    case flexbuffers::FBT_VECTOR:
      return FlexbufferVectorToVariantInternal(ref.AsVector(), depth);

    case flexbuffers::FBT_BLOB:
      LogError("Flexbuffers containing blobs are not supported.");
      break;
    case flexbuffers::FBT_MAX_TYPE:
    default:
      LogError("Unknown or unsupported flexbuffer type: %d",
               static_cast<int>(ref.GetType()));
      break;
  }
  return Variant::Null();
}

Variant FlexbufferVectorToVariant(const flexbuffers::Vector& vector) {
  return FlexbufferVectorToVariantInternal(vector, 0);
}

Variant FlexbufferMapToVariant(const flexbuffers::Map& map) {
  return FlexbufferMapToVariantInternal(map, 0);
}

Variant FlexbufferToVariant(const flexbuffers::Reference& ref) {
  return FlexbufferToVariantInternal(ref, 0);
}

Variant JsonToVariant(const char* json) {
  if (json == nullptr) {
    LogError("JsonToVariant called with null JSON string.");
    return Variant::Null();
  }
  flatbuffers::Parser parser;
  flexbuffers::Builder builder;
  if (!parser.ParseFlexBuffer(json, nullptr, &builder)) {
    if (!parser.error_.empty()) {
      LogError("Failed to parse JSON: %s", parser.error_.c_str());
    }
    return Variant::Null();
  }
  const std::vector<uint8_t>& buffer = builder.GetBuffer();
  if (buffer.empty()) {
    return Variant::Null();
  }
  flexbuffers::Reference root = flexbuffers::GetRoot(buffer);
  return FlexbufferToVariant(root);
}

static bool VariantToFlexbufferInternal(const Variant& variant,
                                        flexbuffers::Builder* fbb,
                                        size_t depth);
static bool VariantMapToFlexbufferInternal(
    const std::map<Variant, Variant>& map, flexbuffers::Builder* fbb,
    size_t depth);
static bool VariantVectorToFlexbufferInternal(
    const std::vector<Variant>& vector, flexbuffers::Builder* fbb,
    size_t depth);
static bool IsValidForFlexbuffer(const Variant& variant, size_t depth);

static bool IsValidVectorForFlexbuffer(const std::vector<Variant>& vector,
                                       size_t depth) {
  if (depth > kMaxVariantDepth) {
    LogError("Variant nesting depth exceeds maximum (%zu).", kMaxVariantDepth);
    return false;
  }
  for (const auto& item : vector) {
    if (!IsValidForFlexbuffer(item, depth + 1)) {
      return false;
    }
  }
  return true;
}

static bool IsValidMapForFlexbuffer(const std::map<Variant, Variant>& map,
                                    size_t depth) {
  if (depth > kMaxVariantDepth) {
    LogError("Variant nesting depth exceeds maximum (%zu).", kMaxVariantDepth);
    return false;
  }
  for (const auto& kv : map) {
    if (kv.first.is_null() || !kv.first.is_fundamental_type()) {
      LogError(
          "Variants of non-fundamental types may not be used as map "
          "keys.");
      return false;
    }
    if (!IsValidForFlexbuffer(kv.second, depth + 1)) {
      return false;
    }
  }
  return true;
}

static bool IsValidForFlexbuffer(const Variant& variant, size_t depth) {
  if (depth > kMaxVariantDepth) {
    LogError("Variant nesting depth exceeds maximum (%zu).", kMaxVariantDepth);
    return false;
  }
  switch (variant.type()) {
    case Variant::kTypeNull:
    case Variant::kTypeInt64:
    case Variant::kTypeDouble:
    case Variant::kTypeBool:
    case Variant::kTypeStaticString:
    case Variant::kTypeMutableString:
      return true;
    case Variant::kTypeVector:
      return IsValidVectorForFlexbuffer(variant.vector(), depth);
    case Variant::kTypeMap:
      return IsValidMapForFlexbuffer(variant.map(), depth);
    case Variant::kTypeStaticBlob:
    case Variant::kTypeMutableBlob:
      LogError("Variants containing blobs are not supported.");
      return false;
  }
  return false;
}

static bool VariantToFlexbufferInternal(const Variant& variant,
                                        flexbuffers::Builder* fbb,
                                        size_t depth) {
  if (fbb == nullptr) {
    return false;
  }
  if (depth > kMaxVariantDepth) {
    LogError("Variant nesting depth exceeds maximum (%zu).", kMaxVariantDepth);
    return false;
  }
  switch (variant.type()) {
    case Variant::kTypeNull: {
      fbb->Null();
      break;
    }
    case Variant::kTypeInt64: {
      fbb->Int(variant.int64_value());
      break;
    }
    case Variant::kTypeDouble: {
      fbb->Double(variant.double_value());
      break;
    }
    case Variant::kTypeBool: {
      fbb->Bool(variant.bool_value());
      break;
    }
    case Variant::kTypeStaticString:
    case Variant::kTypeMutableString: {
      const char* str = variant.string_value();
      size_t len = variant.is_mutable_string() ? variant.mutable_string().size()
                                               : strlen(str);
      fbb->String(str, len);
      break;
    }
    case Variant::kTypeVector: {
      if (!VariantVectorToFlexbufferInternal(variant.vector(), fbb, depth)) {
        return false;
      }
      break;
    }
    case Variant::kTypeMap: {
      if (!VariantMapToFlexbufferInternal(variant.map(), fbb, depth)) {
        return false;
      }
      break;
    }
    case Variant::kTypeStaticBlob:
    case Variant::kTypeMutableBlob: {
      LogError("Variants containing blobs are not supported.");
      return false;
    }
  }
  return true;
}

static bool VariantMapToFlexbufferInternal(
    const std::map<Variant, Variant>& map, flexbuffers::Builder* fbb,
    size_t depth) {
  if (fbb == nullptr) {
    return false;
  }
  if (depth > kMaxVariantDepth) {
    LogError("Variant nesting depth exceeds maximum (%zu).", kMaxVariantDepth);
    return false;
  }
  auto start = fbb->StartMap();
  for (auto iter = map.begin(); iter != map.end(); ++iter) {
    // Flexbuffers only supports string keys, return false if the key is not a
    // type that can be coerced to a string.
    if (iter->first.is_null() || !iter->first.is_fundamental_type()) {
      LogError(
          "Variants of non-fundamental types may not be used as map keys.");
      fbb->EndMap(start);
      return false;
    }
    // Add key.
    Variant key_str = iter->first.AsString();
    fbb->Key(key_str.string_value());
    // Add value.
    if (!VariantToFlexbufferInternal(iter->second, fbb, depth + 1)) {
      fbb->EndMap(start);
      return false;
    }
  }
  fbb->EndMap(start);
  return true;
}

static bool VariantVectorToFlexbufferInternal(
    const std::vector<Variant>& vector, flexbuffers::Builder* fbb,
    size_t depth) {
  if (fbb == nullptr) {
    return false;
  }
  if (depth > kMaxVariantDepth) {
    LogError("Variant nesting depth exceeds maximum (%zu).", kMaxVariantDepth);
    return false;
  }
  auto start = fbb->StartVector();
  for (auto iter = vector.begin(); iter != vector.end(); ++iter) {
    if (!VariantToFlexbufferInternal(*iter, fbb, depth + 1)) {
      fbb->EndVector(start, false, false);
      return false;
    }
  }
  fbb->EndVector(start, false, false);
  return true;
}

bool VariantToFlexbuffer(const Variant& variant, flexbuffers::Builder* fbb) {
  if (fbb == nullptr || !IsValidForFlexbuffer(variant, 0)) {
    return false;
  }
  return VariantToFlexbufferInternal(variant, fbb, 0);
}

bool VariantMapToFlexbuffer(const std::map<Variant, Variant>& map,
                            flexbuffers::Builder* fbb) {
  if (fbb == nullptr || !IsValidMapForFlexbuffer(map, 0)) {
    return false;
  }
  return VariantMapToFlexbufferInternal(map, fbb, 0);
}

bool VariantVectorToFlexbuffer(const std::vector<Variant>& vector,
                               flexbuffers::Builder* fbb) {
  if (fbb == nullptr || !IsValidVectorForFlexbuffer(vector, 0)) {
    return false;
  }
  return VariantVectorToFlexbufferInternal(vector, fbb, 0);
}

// Convert from a Variant to a Flexbuffer buffer.
std::vector<uint8_t> VariantToFlexbuffer(const Variant& variant) {
  flexbuffers::Builder fbb(FLEXBUFFER_BUILDER_STARTING_SIZE);
  if (!VariantToFlexbuffer(variant, &fbb)) {
    return std::vector<uint8_t>();
  }
  fbb.Finish();
  return fbb.GetBuffer();
}

std::vector<uint8_t> VariantMapToFlexbuffer(
    const std::map<Variant, Variant>& map) {
  flexbuffers::Builder fbb(FLEXBUFFER_BUILDER_STARTING_SIZE);
  if (!VariantMapToFlexbuffer(map, &fbb)) {
    return std::vector<uint8_t>();
  }
  fbb.Finish();
  return fbb.GetBuffer();
}

std::vector<uint8_t> VariantVectorToFlexbuffer(
    const std::vector<Variant>& vector) {
  flexbuffers::Builder fbb(FLEXBUFFER_BUILDER_STARTING_SIZE);
  if (!VariantVectorToFlexbuffer(vector, &fbb)) {
    return std::vector<uint8_t>();
  }
  fbb.Finish();
  return fbb.GetBuffer();
}

}  // namespace util
// NOLINTNEXTLINE - allow namespace overridden
}  // namespace firebase
