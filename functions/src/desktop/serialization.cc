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
#include "app/src/variant_util.h"

namespace firebase {
namespace functions {
namespace internal {

Variant Encode(const Variant& variant) {
  if (variant.is_int64()) {
    Variant m = Variant::EmptyMap();
    m.map()["@type"] = "type.googleapis.com/google.protobuf.Int64Value";
    m.map()["value"] = variant.AsString();
    return m;
  } else if (variant.is_map()) {
    // Recursively encode map values.
    Variant m = Variant::EmptyMap();
    for (auto& entry : variant.map()) {
      m.map()[entry.first] = Encode(entry.second);
    }
    return m;
  } else if (variant.is_vector()) {
    // Recursively encode vector values.
    Variant v = Variant::EmptyVector();
    for (auto& entry : variant.vector()) {
      v.vector().push_back(Encode(entry));
    }
    return v;
  }
  return variant;
}

Variant Decode(const Variant& variant) {
  if (variant.is_map()) {
    // If there's a special @type entry, try to parse it.
    auto type_it = variant.map().find("@type");
    if (type_it != variant.map().end() && type_it->second.is_string()) {
      std::string type = type_it->second.string_value();
      if (type == "type.googleapis.com/google.protobuf.Int64Value") {
        auto value_it = variant.map().find("value");
        if (value_it != variant.map().end() && value_it->second.is_string()) {
          // Parse a long out of the string.
          std::string value = value_it->second.string_value();
          int64_t n = strtoull(value.c_str(), nullptr, 10);  // NOLINT
          return Variant(n);
        }
      }
    }

    // Recursively decode map values.
    Variant m = Variant::EmptyMap();
    for (auto& entry : variant.map()) {
      m.map()[entry.first] = Decode(entry.second);
    }
    return m;
  } else if (variant.is_vector()) {
    // Recursively decode vector values.
    Variant v = Variant::EmptyVector();
    for (auto& entry : variant.vector()) {
      v.vector().push_back(Decode(entry));
    }
    return v;
  }
  return variant;
}

}  // namespace internal
}  // namespace functions
}  // namespace firebase
