// Copyright 2020 Google LLC
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

#include "app/tests/flexbuffer_matcher.h"

// For testing purposes, we only care about the basic types.
enum FlexbuffersMetaTypes {
  kNull,
  kBool,
  kInt,
  kUInt,
  kFloat,
  kString,
  kKey,
  kMap,
  kVector,
  kBlob,
};

// Type names for error messages.
const char* meta_type_names[] = {
    "Null",   "Bool", "Int", "UInt",   "Float",
    "String", "Key",  "Map", "Vector", "Blob",
};

FlexbuffersMetaTypes GetFlexbuffersReferenceType(
    const flexbuffers::Reference& ref) {
  switch (ref.GetType()) {
    case flexbuffers::FBT_NULL: {
      return kNull;
    }
    case flexbuffers::FBT_BOOL: {
      return kBool;
    }
    case flexbuffers::FBT_INDIRECT_INT:
    case flexbuffers::FBT_INT: {
      return kInt;
    }
    case flexbuffers::FBT_INDIRECT_UINT:
    case flexbuffers::FBT_UINT: {
      return kUInt;
    }
    case flexbuffers::FBT_INDIRECT_FLOAT:
    case flexbuffers::FBT_FLOAT: {
      return kFloat;
    }
    case flexbuffers::FBT_KEY: {
      return kKey;
    }
    case flexbuffers::FBT_STRING: {
      return kString;
    }
    case flexbuffers::FBT_MAP: {
      return kMap;
    }
    case flexbuffers::FBT_VECTOR:
    case flexbuffers::FBT_VECTOR_INT:
    case flexbuffers::FBT_VECTOR_UINT:
    case flexbuffers::FBT_VECTOR_FLOAT:
    case flexbuffers::FBT_VECTOR_KEY:
    case flexbuffers::FBT_VECTOR_STRING_DEPRECATED:
    case flexbuffers::FBT_VECTOR_INT2:
    case flexbuffers::FBT_VECTOR_UINT2:
    case flexbuffers::FBT_VECTOR_FLOAT2:
    case flexbuffers::FBT_VECTOR_INT3:
    case flexbuffers::FBT_VECTOR_UINT3:
    case flexbuffers::FBT_VECTOR_FLOAT3:
    case flexbuffers::FBT_VECTOR_INT4:
    case flexbuffers::FBT_VECTOR_UINT4:
    case flexbuffers::FBT_VECTOR_FLOAT4:
    case flexbuffers::FBT_VECTOR_BOOL: {
      return kVector;
    }
    case flexbuffers::FBT_BLOB: {
      return kBlob;
    }
  }
}

template <typename T>
void MismatchMessage(const std::string& title, const T& expected, const T& arg,
                     const std::string& location,
                     ::testing::MatchResultListener* result_listener) {
  *result_listener << title << ": Expected " << expected;
  if (!location.empty()) {
    *result_listener << " at " << location;
  }
  *result_listener << ", got " << arg;
}

// TODO(73494146): Check in EqualsFlexbuffer gmock matcher into the canonical
// Flatbuffer repository.
// Because pushing things to the Flatbuffers library is a multistep process, I'm
// including this for now so the tests can be built. Once this has been merged
// into flatbuffers, we can remove this implementation of it and use the one
// supplied by Flatbuffers.
//
// Checks the equality of two Flexbuffers. This checker ignores whether values
// are 'Indirect' and typed vectors are treated as plain vectors.
bool EqualsFlexbufferImpl(const flexbuffers::Reference& expected,
                          const flexbuffers::Reference& arg,
                          const std::string& location,
                          ::testing::MatchResultListener* result_listener) {
  FlexbuffersMetaTypes expected_type = GetFlexbuffersReferenceType(expected);
  FlexbuffersMetaTypes arg_type = GetFlexbuffersReferenceType(arg);

  if (expected_type != arg_type) {
    MismatchMessage("Type mismatch", meta_type_names[expected_type],
                    meta_type_names[arg_type], location, result_listener);
    return false;
  }

  if (expected.IsNull()) {
    // No value checking necessary as Null has no value.
    return true;
  }
  if (expected.IsBool()) {
    if (expected.AsBool() != arg.AsBool()) {
      MismatchMessage("Value mismatch", (expected.AsBool() ? "true" : "false"),
                      (arg.AsBool() ? "true" : "false"), location,
                      result_listener);
      return false;
    }
    return true;
  } else if (expected.IsInt()) {
    if (expected.AsInt64() != arg.AsInt64()) {
      MismatchMessage("Value mismatch", expected.AsInt64(), arg.AsInt64(),
                      location, result_listener);
      return false;
    }
    return true;
  } else if (expected.IsUInt()) {
    if (expected.AsUInt64() != arg.AsUInt64()) {
      MismatchMessage("Value mismatch", expected.AsUInt64(), arg.AsUInt64(),
                      location, result_listener);
      return false;
    }
    return true;
  } else if (expected.IsFloat()) {
    if (expected.AsDouble() != arg.AsDouble()) {
      MismatchMessage("Value mismatch", expected.AsDouble(), arg.AsDouble(),
                      location, result_listener);
      return false;
    }
    return true;
  } else if (expected.IsString()) {
    if (strcmp(expected.AsString().c_str(), arg.AsString().c_str()) != 0) {
      MismatchMessage("Value mismatch", expected.AsString().c_str(),
                      arg.AsString().c_str(), location, result_listener);
      return false;
    }
    return true;
  } else if (expected.IsKey()) {
    if (strcmp(expected.AsKey(), arg.AsKey()) != 0) {
      MismatchMessage("Key mismatch", expected.AsKey(), arg.AsKey(), location,
                      result_listener);
      return false;
    }
    return true;
  } else if (expected.IsBlob()) {
    if (expected.AsBlob().size() != arg.AsBlob().size() |
        std::memcmp(expected.AsBlob().data(), arg.AsBlob().data(),
                    expected.AsBlob().size()) != 0) {
      *result_listener << "Binary mismatch";
      if (!location.empty()) {
        *result_listener << " at " << location;
      }
      return false;
    }
    return true;
  } else if (expected.IsMap()) {
    flexbuffers::Map expected_map = expected.AsMap();
    flexbuffers::Map arg_map = arg.AsMap();
    if (expected_map.size() != arg_map.size()) {
      MismatchMessage("Map size mismatch",
                      std::to_string(expected_map.size()) + " elements",
                      std::to_string(arg_map.size()) + " elements", location,
                      result_listener);
      return false;
    }
    flexbuffers::TypedVector expected_keys = expected_map.Keys();
    flexbuffers::TypedVector arg_keys = arg_map.Keys();
    for (size_t i = 0; i < expected_keys.size(); ++i) {
      std::string new_location =
          location + "[" + expected_keys[i].AsKey() + "]";
      if (!EqualsFlexbufferImpl(expected_keys[i], arg_keys[i], new_location,
                                result_listener)) {
        return false;
      }
    }
    // Don't return in case of success, because we still need to check that
    // the values match. This is done in the IsVector section, since Maps are
    // also Vectors.
  }
  if (expected.IsVector()) {
    flexbuffers::Vector expected_vector = expected.AsVector();
    flexbuffers::Vector arg_vector = arg.AsVector();
    if (expected_vector.size() != arg_vector.size()) {
      MismatchMessage("Vector size mismatch",
                      std::to_string(expected_vector.size()) + " elements",
                      std::to_string(arg_vector.size()) + " elements", location,
                      result_listener);
      return false;
    }
    for (size_t i = 0; i < expected_vector.size(); ++i) {
      std::string new_location = location + "[" + std::to_string(i) + "]";
      if (!EqualsFlexbufferImpl(expected_vector[i], arg_vector[i], new_location,
                                result_listener)) {
        return false;
      }
    }
    return true;
  }
  *result_listener << "Unrecognized type";
  return false;
}

bool EqualsFlexbufferImpl(const flexbuffers::Reference& expected,
                          const std::vector<uint8_t>& arg,
                          const std::string& location,
                          ::testing::MatchResultListener* result_listener) {
  return EqualsFlexbufferImpl(expected, flexbuffers::GetRoot(arg), location,
                              result_listener);
}

bool EqualsFlexbufferImpl(const std::vector<uint8_t>& expected,
                          const flexbuffers::Reference& arg,
                          const std::string& location,
                          ::testing::MatchResultListener* result_listener) {
  return EqualsFlexbufferImpl(flexbuffers::GetRoot(expected), arg, location,
                              result_listener);
}

bool EqualsFlexbufferImpl(const std::vector<uint8_t>& expected,
                          const std::vector<uint8_t>& arg,
                          const std::string& location,
                          ::testing::MatchResultListener* result_listener) {
  return EqualsFlexbufferImpl(flexbuffers::GetRoot(expected),
                              flexbuffers::GetRoot(arg), location,
                              result_listener);
}
