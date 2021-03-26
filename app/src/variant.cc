/*
 * Copyright 2016 Google LLC
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

#include "app/src/include/firebase/variant.h"

#include <limits.h>
#include <stdlib.h>

#include <iomanip>
#include <sstream>

#include "app/src/assert.h"
#include "app/src/include/firebase/internal/platform.h"

namespace firebase {

Variant& Variant::operator=(const Variant& other) {
  if (this != &other) {
    Clear(static_cast<Type>(other.type_));
    switch (type_) {
      case kInternalTypeNull: {
        break;
      }
      case kInternalTypeInt64: {
        set_int64_value(other.int64_value());
        break;
      }
      case kInternalTypeDouble: {
        set_double_value(other.double_value());
        break;
      }
      case kInternalTypeBool: {
        set_bool_value(other.bool_value());
        break;
      }
      case kInternalTypeStaticString: {
        set_string_value(other.string_value());
        break;
      }
      case kInternalTypeMutableString: {
        set_mutable_string(other.mutable_string());
        break;
      }
      case kInternalTypeSmallString: {
        strcpy(value_.small_string, other.value_.small_string);  // NOLINT
        break;
      }
      case kInternalTypeVector: {
        set_vector(other.vector());
        break;
      }
      case kInternalTypeMap: {
        set_map(other.map());
        break;
      }
      case kInternalTypeStaticBlob: {
        set_blob_pointer(other.value_.blob_value.ptr,
                         other.value_.blob_value.size);
        break;
      }
      case kInternalTypeMutableBlob: {
        set_mutable_blob(other.value_.blob_value.ptr,
                         other.value_.blob_value.size);
        break;
      }
      case kMaxTypeValue:
        FIREBASE_ASSERT(false);  // Should never hit this
        break;
    }
  }
  return *this;
}

#if defined(FIREBASE_USE_MOVE_OPERATORS)

Variant& Variant::operator=(Variant&& other) noexcept {
  if (this != &other) {
    Clear();
    type_ = other.type_;
    other.type_ = kInternalTypeNull;
    switch (type_) {
      case kInternalTypeNull: {
        break;
      }
      case kInternalTypeInt64: {
        value_.int64_value = other.value_.int64_value;
        break;
      }
      case kInternalTypeDouble: {
        value_.double_value = other.value_.double_value;
        break;
      }
      case kInternalTypeBool: {
        value_.bool_value = other.value_.bool_value;
        break;
      }
      case kInternalTypeStaticString: {
        value_.static_string_value = other.value_.static_string_value;
        other.value_.static_string_value = nullptr;
        break;
      }
      case kInternalTypeMutableString: {
        value_.mutable_string_value = other.value_.mutable_string_value;
        other.value_.mutable_string_value = nullptr;
        break;
      }
      case kInternalTypeSmallString: {
        memcpy(value_.small_string, other.value_.small_string,
               kMaxSmallStringSize);
        other.value_.small_string[0] = '\0';
        break;
      }
      case kInternalTypeVector: {
        value_.vector_value = other.value_.vector_value;
        other.value_.vector_value = nullptr;
        break;
      }
      case kInternalTypeMap: {
        value_.map_value = other.value_.map_value;
        other.value_.map_value = nullptr;
        break;
      }
      case kInternalTypeStaticBlob: {
        set_static_blob(other.value_.blob_value.ptr,
                        other.value_.blob_value.size);
        break;
      }
      case kInternalTypeMutableBlob: {
        set_blob_pointer(other.value_.blob_value.ptr,
                         other.value_.blob_value.size);
        other.value_.blob_value.ptr = nullptr;
        other.value_.blob_value.size = 0;
        break;
      }
      case kMaxTypeValue:
        FIREBASE_ASSERT(false);  // Should never hit this
        break;
    }
  }
  return *this;
}

#endif  // defined(FIREBASE_USE_MOVE_OPERATORS)

bool Variant::operator==(const Variant& other) const {
  // If types don't match and we aren't both strings or blobs, fail.
  if (type_ != other.type_ && !(is_string() && other.is_string()) &&
      !(is_blob() && other.is_blob()))
    return false;
  // Now we know their types are equivalent. So:
  switch (type_) {
    case kInternalTypeNull:
      return true;  // nulls are always equal
    case kInternalTypeInt64:
      return int64_value() == other.int64_value();
    case kInternalTypeDouble:
      return double_value() == other.double_value();
    case kInternalTypeBool:
      return bool_value() == other.bool_value();
    case kInternalTypeMutableString:
    case kInternalTypeStaticString:
    case kInternalTypeSmallString:
      // string == performs string comparison
      return strcmp(string_value(), other.string_value()) == 0;
    case kInternalTypeVector:
      // std::vector == performs element-by-element comparison
      return vector() == other.vector();
    case kInternalTypeMap:
      // std::map == performs element-by-element comparison
      return map() == other.map();
    case kInternalTypeStaticBlob:
    case kInternalTypeMutableBlob:
      // Return true if both are static blobs with the same pointers, otherwise
      // compare the contents. Also the sizes must match.
      return blob_size() == other.blob_size() &&
             ((is_static_blob() && other.is_static_blob() &&
               blob_data() == other.blob_data()) ||
              memcmp(blob_data(), other.blob_data(), blob_size()) == 0);
    case kMaxTypeValue:
      FIREBASE_ASSERT(false);  // Should never hit this
      break;
  }
  return false;  // Should never reach this.
}

bool Variant::operator<(const Variant& other) const {
  Type left_type = type();
  Type right_type = other.type();

  // If we are any string type set type to static string as we care about string
  // value not type of string.
  if (is_string()) {
    left_type = kTypeStaticString;
  }

  // If other is any string type set type to static string as we care about
  // string value not type of string.
  if (other.is_string()) {
    right_type = kTypeStaticString;
  }

  // If we are any blob type set type to static blob as we care about blob value
  // not type of blob.
  if (is_blob()) {
    left_type = kTypeStaticBlob;
  }

  // If other is any blob type set type to static blob as we care about blob
  // value not type of blob.
  if (other.is_blob()) {
    right_type = kTypeStaticBlob;
  }

  // If the types don't match (except count both string types as matching, and
  // both blob types as matching), compare the types.
  if (left_type != right_type)
    return static_cast<int>(left_type) < static_cast<int>(right_type);
  // Type is now equal (or both strings, or both blobs).
  switch (type_) {
    case kInternalTypeNull: {
      return false;  // nulls are always equal
    }
    case kInternalTypeInt64: {
      return int64_value() < other.int64_value();
    }
    case kInternalTypeDouble: {
      return double_value() < other.double_value();
    }
    case kInternalTypeBool: {
      return bool_value() < other.bool_value();
    }
    case kInternalTypeMutableString:
    case kInternalTypeStaticString:
    case kInternalTypeSmallString:
      return strcmp(string_value(), other.string_value()) < 0;
    case kInternalTypeVector: {
      auto i = vector().begin();
      auto j = other.vector().begin();
      for (; i != vector().end() && j != other.vector().end(); ++i, ++j) {
        if (*i != *j) return *i < *j;
      }
      // The elements we compared are the same, but one might be larger than
      // the other. Consider the smaller one as < the other.
      if (i == vector().end() && j != other.vector().end()) return true;
      if (i != vector().end() && j == other.vector().end()) return false;
      return false;  // Equal!
    }
    case kInternalTypeMap: {
      auto i = map().begin();
      auto j = other.map().begin();
      for (; i != map().end() && j != other.map().end(); ++i, ++j) {
        if (i->first != j->first) return i->first < j->first;
        if (i->second != j->second) return i->second < j->second;
      }
      // The elements we compared are the same, but one might be larger than
      // the other. Consider the smaller one as < the other.
      if (i == map().end() && j != other.map().end()) return true;
      if (i != map().end() && j == other.map().end()) return false;
      return false;  // Equal!
    }
    case kInternalTypeMutableBlob:
    case kInternalTypeStaticBlob:
      return blob_size() == other.blob_size()
                 ? memcmp(blob_data(), other.blob_data(), blob_size()) < 0
                 : blob_size() < other.blob_size();
    case kMaxTypeValue:
      FIREBASE_ASSERT(false);  // Should never hit this
      break;
  }
  return false;  // Should never reach this.
}

void Variant::Clear(Type new_type) {
  switch (type_) {
    case kInternalTypeNull: {
      break;
    }
    case kInternalTypeInt64: {
      value_.int64_value = 0;
      break;
    }
    case kInternalTypeDouble: {
      value_.double_value = 0;
      break;
    }
    case kInternalTypeBool: {
      value_.bool_value = false;
      break;
    }
    case kInternalTypeStaticString: {
      value_.static_string_value = nullptr;
      break;
    }
    case kInternalTypeMutableString: {
      if (new_type != kTypeMutableString
          || value_.mutable_string_value == nullptr) {
        delete value_.mutable_string_value;
        value_.mutable_string_value = nullptr;
      } else {
        value_.mutable_string_value->clear();
      }
      break;
    }
    case kInternalTypeSmallString: {
      value_.small_string[0] = '\0';
      break;
    }
    case kInternalTypeVector: {
      if (new_type != kTypeVector || value_.vector_value == nullptr) {
        delete value_.vector_value;
        value_.vector_value = nullptr;
      } else {
        value_.vector_value->clear();
      }
      break;
    }
    case kInternalTypeMap: {
      if (new_type != kTypeMap || value_.map_value == nullptr) {
        delete value_.map_value;
        value_.map_value = nullptr;
      } else {
        value_.map_value->clear();
      }
      break;
    }
    case kInternalTypeStaticBlob: {
      set_blob_pointer(nullptr, 0);
      break;
    }
    case kInternalTypeMutableBlob: {
      uint8_t* prev_data = const_cast<uint8_t*>(value_.blob_value.ptr);
      set_blob_pointer(nullptr, 0);
      delete[] prev_data;
      break;
    }
    case kMaxTypeValue:
      FIREBASE_ASSERT(false);  // Should never hit this
      break;
  }

  InternalType old_type = type_;
  type_ = static_cast<InternalType>(new_type);
  switch (type_) {
    case kInternalTypeNull: {
      break;
    }
    case kInternalTypeInt64: {
      value_.int64_value = 0;
      break;
    }
    case kInternalTypeDouble: {
      value_.double_value = 0;
      break;
    }
    case kInternalTypeBool: {
      value_.bool_value = false;
      break;
    }
    case kInternalTypeStaticString: {
      value_.static_string_value = "";
      break;
    }
    case kInternalTypeMutableString: {
      if (old_type != kInternalTypeMutableString ||
          value_.mutable_string_value == nullptr) {
        value_.mutable_string_value = new std::string();
      }
      break;
    }
    case kInternalTypeSmallString: {
      value_.small_string[0] = '\0';
      break;
    }
    case kInternalTypeVector: {
      if (old_type != kInternalTypeVector || value_.vector_value == nullptr) {
        value_.vector_value = new std::vector<Variant>(0);
      }
      break;
    }
    case kInternalTypeMap: {
      if (old_type != kInternalTypeMap || value_.map_value == nullptr) {
        value_.map_value = new std::map<Variant, Variant>();
      }
      break;
    }
    case kInternalTypeStaticBlob: {
      set_blob_pointer(nullptr, 0);
      break;
    }
    case kInternalTypeMutableBlob: {
      set_blob_pointer(nullptr, 0);
      break;
    }
    case kMaxTypeValue:
      FIREBASE_ASSERT(false);  // Should never hit this
      break;
  }
}

const char* const Variant::kTypeNames[] = {
    // In case you want to iterate through these for some reason.
    "Null",         "Int64",         "Double",      "Bool",
    "StaticString", "MutableString", "Vector",      "Map",
    "StaticBlob",   "MutableBlob",   "SmallString", nullptr,
};

void Variant::assert_is_type(Variant::Type type) const {
    static_assert(FIREBASE_ARRAYSIZE(Variant::kTypeNames) ==
                Variant::kMaxTypeValue + 1,
                  "Type Enum should match kTypeNames");

  FIREBASE_ASSERT_MESSAGE(
      this->type_ == static_cast<InternalType>(type),
      "Expected Variant to be of type %s, but it was of type %s.",
      kTypeNames[type], kTypeNames[this->type_]);
}

void Variant::assert_is_not_type(Variant::Type type) const {
  FIREBASE_ASSERT_MESSAGE(this->type_ == static_cast<InternalType>(type),
                          "Expected Variant to NOT be of type %s, but it is.",
                          kTypeNames[type]);
}

void Variant::assert_is_string() const {
  FIREBASE_ASSERT_MESSAGE(
      is_string(), "Expected Variant to be a String, but it was of type %s.",
      kTypeNames[this->type_]);
}

void Variant::assert_is_blob() const {
  FIREBASE_ASSERT_MESSAGE(
      is_blob(), "Expected Variant to be a Blob, but it was of type %s.",
      kTypeNames[this->type_]);
}

#if FIREBASE_PLATFORM_WINDOWS
#define INT64FORMAT "%I64d"
#else
#define INT64FORMAT "%jd"
#endif  // FIREBASE_PLATFORM_WINDOWS

Variant Variant::AsString() const {
#ifdef FIREBASE_USE_SNPRINTF
  static const size_t kBufferSize = 64;
#endif  // FIREBASE_USE_SNPRINTF
  switch (type_) {
    case kInternalTypeInt64: {
#ifdef FIREBASE_USE_SNPRINTF
      char buffer[kBufferSize];
      snprintf(buffer, kBufferSize, INT64FORMAT,
               static_cast<intmax_t>(int64_value()));
      return Variant::FromMutableString(buffer);
#else
      std::stringstream ss;
      ss << static_cast<intmax_t>(int64_value());
      return Variant::FromMutableString(ss.str());
#endif  // FIREBASE_USE_SNPRINTF
    }
    case kInternalTypeDouble: {
#ifdef FIREBASE_USE_SNPRINTF
      char buffer[kBufferSize];
      snprintf(buffer, kBufferSize, "%.16f", double_value());
      return Variant::FromMutableString(buffer);
#else
      std::stringstream ss;
      ss << std::fixed << std::setprecision(16) << double_value();
      return Variant::FromMutableString(ss.str());
#endif  // FIREBASE_USE_SNPRINTF
    }
    case kInternalTypeBool: {
      return bool_value() ? Variant("true") : Variant("false");
    }
    case kInternalTypeMutableString:
    case kInternalTypeStaticString:
    case kInternalTypeSmallString: {
      return *this;
    }
    default: {
      return Variant::EmptyString();
    }
  }
}

Variant Variant::AsInt64() const {
  switch (type_) {
    case kInternalTypeInt64: {
      return *this;
    }
    case kInternalTypeDouble: {
      return Variant::FromInt64(static_cast<int64_t>(double_value()));
    }
    case kInternalTypeBool: {
      return bool_value() ? Variant::One() : Variant::Zero();
    }
    case kInternalTypeMutableString:
    case kInternalTypeStaticString:
    case kInternalTypeSmallString: {
      return Variant::FromInt64(strtol(string_value(), nullptr, 10));  // NOLINT
    }
    default: {
      return Variant::Zero();
    }
  }
}

Variant Variant::AsDouble() const {
  switch (type_) {
    case kInternalTypeInt64: {
      return Variant::FromDouble(static_cast<double>(int64_value()));
    }
    case kInternalTypeDouble: {
      return *this;
    }
    case kInternalTypeBool: {
      return bool_value() ? Variant::OnePointZero() : Variant::ZeroPointZero();
    }
    case kInternalTypeMutableString:
    case kInternalTypeStaticString:
    case kInternalTypeSmallString: {
      return Variant::FromDouble(strtod(string_value(), nullptr));
    }
    default: {
      return Variant::ZeroPointZero();
    }
  }
}

Variant Variant::AsBool() const {
  if (*this == Variant::Null() || *this == Variant::Zero() ||
      *this == Variant::ZeroPointZero() || *this == Variant::False() ||
      *this == Variant::EmptyString() || *this == Variant::EmptyVector() ||
      *this == Variant::EmptyMap() || *this == Variant("false") ||
      (is_blob() && blob_size() == 0))
    return Variant::False();
  else
    return Variant::True();
}

const char* Variant::TypeName(Variant::Type t) {
  static int num_variant_types = -1;
  if (num_variant_types == -1) {
    int i;
    for (i = 0; kTypeNames[i] != nullptr; i++) {
      // Count the number of types.
    }
    num_variant_types = i;
  }
  FIREBASE_ASSERT(t >= 0 && t <= num_variant_types);
  return kTypeNames[t];
}

}  // namespace firebase
