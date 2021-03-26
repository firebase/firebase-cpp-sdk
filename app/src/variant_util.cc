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

#include <sstream>

#include "app/src/assert.h"
#include "app/src/log.h"
#include "flatbuffers/flatbuffers.h"
#include "flatbuffers/flexbuffers.h"
#include "flatbuffers/idl.h"
#include "flatbuffers/util.h"

#define FLEXBUFFER_BUILDER_STARTING_SIZE 512

#if !defined(FIREBASE_NAMESPACE)
#define FIREBASE_NAMESPACE firebase
#endif

namespace FIREBASE_NAMESPACE {
namespace util {

// Forward declarations for stringstream variations of the *ToJson functions
// since these aren't made available in the header. These return true on success
// and false on failure. Failure is a result of using binary blobs in the
// variant, or using types that cannot be coerced to a string as a key in a map.
static bool VariantToJson(const Variant& variant, bool prettyPrint,
                          const std::string& indent, std::stringstream* ss);
static bool StdMapToJson(const std::map<Variant, Variant>& map,
                         bool prettyPrint, const std::string& indent,
                         std::stringstream* ss);
static bool StdVectorToJson(const std::vector<Variant>& vector,
                            bool prettyPrint, const std::string& indent,
                            std::stringstream* ss);

static bool VariantToJson(const Variant& variant, bool prettyPrint,
                          const std::string& indent, std::stringstream* ss) {
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
      *ss << variant.double_value();
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
      if (!StdVectorToJson(variant.vector(), prettyPrint, indent, ss)) {
        return false;
      }
      break;
    }
    case Variant::kTypeMap: {
      if (!StdMapToJson(variant.map(), prettyPrint, indent, ss)) {
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
                         std::stringstream* ss) {
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
    if (!VariantToJson(iter->first.AsString(), prettyPrint, nextIndent, ss)) {
      return false;
    }
    *ss << ':';
    if (prettyPrint) {
      *ss << ' ';
    }
    if (!VariantToJson(iter->second, prettyPrint, nextIndent, ss)) {
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
                            std::stringstream* ss) {
  *ss << '[';
  std::string nextIndent = indent + "  ";
  for (auto iter = vector.begin(); iter != vector.end();) {
    if (prettyPrint) {
      *ss << '\n' << nextIndent;
    }
    if (!VariantToJson(*iter, prettyPrint, nextIndent, ss)) {
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
  if (!VariantToJson(variant, prettyPrint, "", &ss)) {
    return "";
  }
  return ss.str();
}

// Converts an std::map<Variant, Variant> to Json
std::string StdMapToJson(const std::map<Variant, Variant>& map) {
  std::stringstream ss;
  if (!StdMapToJson(map, false, "", &ss)) {
    return "";
  }
  return ss.str();
}

// Converts an std::vector<Variant> to Json
std::string StdVectorToJson(const std::vector<Variant>& vector) {
  std::stringstream ss;
  if (!StdVectorToJson(vector, false, "", &ss)) {
    return "";
  }
  return ss.str();
}

Variant FlexbufferVectorToVariant(const flexbuffers::Vector& vector) {
  Variant result = Variant::EmptyVector();
  result.vector().reserve(vector.size());
  for (size_t i = 0; i < vector.size(); i++) {
    result.vector().push_back(FlexbufferToVariant(vector[i]));
  }
  return result;
}

Variant FlexbufferMapToVariant(const flexbuffers::Map& map) {
  Variant result = Variant::EmptyMap();
  flexbuffers::TypedVector keys = map.Keys();
  for (size_t i = 0; i < keys.size(); i++) {
    flexbuffers::Reference key = keys[i];
    flexbuffers::Reference value = map[key.AsKey()];
    result.map()[FlexbufferToVariant(key)] = FlexbufferToVariant(value);
  }
  return result;
}

Variant FlexbufferToVariant(const flexbuffers::Reference& ref) {
  switch (ref.GetType()) {
    case flexbuffers::FBT_NULL:
      return Variant::Null();
    case flexbuffers::FBT_BOOL:
      return Variant(ref.AsBool());
    case flexbuffers::FBT_INT:
    case flexbuffers::FBT_INDIRECT_INT:
    case flexbuffers::FBT_UINT:
    case flexbuffers::FBT_INDIRECT_UINT:
      return Variant(ref.AsInt64());
    case flexbuffers::FBT_FLOAT:
    case flexbuffers::FBT_INDIRECT_FLOAT:
      return Variant(ref.AsDouble());
    case flexbuffers::FBT_STRING:
      return Variant::MutableStringFromStaticString(ref.AsString().c_str());
    case flexbuffers::FBT_KEY:
      return Variant::MutableStringFromStaticString(ref.AsKey());
    case flexbuffers::FBT_MAP:
      return FlexbufferMapToVariant(ref.AsMap());
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
      return FlexbufferVectorToVariant(ref.AsVector());

    case flexbuffers::FBT_BLOB:
      LogError("Flexbuffers containing blobs are not supported.");
      break;
  }
  return Variant::Null();
}

Variant JsonToVariant(const char* json) {
  flatbuffers::Parser parser;
  flexbuffers::Builder builder;
  if (!json || !parser.ParseFlexBuffer(json, nullptr, &builder)) {
    return Variant::Null();
  }
  const std::vector<uint8_t>& buffer = builder.GetBuffer();
  flexbuffers::Reference root = flexbuffers::GetRoot(buffer);
  return FlexbufferToVariant(root);
}

bool VariantToFlexbuffer(const Variant& variant, flexbuffers::Builder* fbb) {
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
      if (!VariantVectorToFlexbuffer(variant.vector(), fbb)) {
        return false;
      }
      break;
    }
    case Variant::kTypeMap: {
      if (!VariantMapToFlexbuffer(variant.map(), fbb)) {
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

bool VariantMapToFlexbuffer(const std::map<Variant, Variant>& map,
                            flexbuffers::Builder* fbb) {
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
    fbb->Key(iter->first.AsString().string_value());
    // Add value.
    if (!VariantToFlexbuffer(iter->second, fbb)) {
      fbb->EndMap(start);
      return false;
    }
  }
  fbb->EndMap(start);
  return true;
}

bool VariantVectorToFlexbuffer(const std::vector<Variant>& vector,
                               flexbuffers::Builder* fbb) {
  auto start = fbb->StartVector();
  for (auto iter = vector.begin(); iter != vector.end(); ++iter) {
    if (!VariantToFlexbuffer(*iter, fbb)) {
      fbb->EndVector(start, false, false);
      return false;
    }
  }
  fbb->EndVector(start, false, false);
  return true;
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
}  // namespace FIREBASE_NAMESPACE
