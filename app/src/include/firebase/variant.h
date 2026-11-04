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

#ifndef FIREBASE_APP_SRC_INCLUDE_FIREBASE_VARIANT_H_
#define FIREBASE_APP_SRC_INCLUDE_FIREBASE_VARIANT_H_

#include <stdint.h>

#include <cstring>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "firebase/internal/common.h"

namespace firebase {
namespace internal {
class VariantInternal;
}

class Variant {
 public:
  enum Type {
    kTypeNull,
    kTypeInt64,
    kTypeDouble,
    kTypeBool,
    kTypeStaticString,
    kTypeMutableString,
    kTypeVector,
    kTypeMap,
    kTypeStaticBlob,
    kTypeMutableBlob,
  };

#ifndef SWIG
  Variant() : type_(kInternalTypeNull), value_({}) {}

  template <typename T>
  Variant(T value)  // NOLINT
      : type_(kInternalTypeNull) {
    set_value_t(value);
  }

  Variant(const std::string& value)  // NOLINT
      : type_(kInternalTypeNull) {
    set_mutable_string(value);
  }

  Variant(std::string&& value)  // NOLINT
      : type_(kInternalTypeNull) {
    set_mutable_string(std::move(value));
  }

  Variant(const std::vector<Variant>& value)  // NOLINT
      : type_(kInternalTypeNull) {
    set_vector(value);
  }

  Variant(std::vector<Variant>&& value)  // NOLINT
      : type_(kInternalTypeNull) {
    set_vector(std::move(value));
  }

  template <typename T>
  Variant(const std::vector<T>& value)  // NOLINT
      : type_(kInternalTypeNull) {
    Clear(kTypeVector);
    vector().reserve(value.size());
    for (size_t i = 0; i < value.size(); i++) {
      vector().push_back(Variant(value[i]));
    }
  }

  Variant(const std::vector<bool>& value)  // NOLINT
      : type_(kInternalTypeNull) {
    Clear(kTypeVector);
    vector().reserve(value.size());
    for (size_t i = 0; i < value.size(); i++) {
      vector().push_back(Variant(static_cast<bool>(value[i])));
    }
  }

  template <typename T>
  Variant(const T array_of_values[], size_t array_size)
      : type_(kInternalTypeNull) {
    Clear(kTypeVector);
    if (array_of_values != nullptr && array_size > 0) {
      vector().resize(array_size);
      for (size_t i = 0; i < array_size; i++) {
        vector()[i] = Variant(array_of_values[i]);
      }
    }
  }

  Variant(const std::map<Variant, Variant>& value)  // NOLINT
      : type_(kInternalTypeNull) {
    set_map(value);
  }

  Variant(std::map<Variant, Variant>&& value)  // NOLINT
      : type_(kInternalTypeNull) {
    set_map(std::move(value));
  }

  template <typename K, typename V>
  Variant(const std::map<K, V>& value)  // NOLINT
      : type_(kInternalTypeNull) {
    Clear(kTypeMap);
    for (auto i = value.begin(); i != value.end(); ++i) {
      map().emplace(Variant(i->first), Variant(i->second));
    }
  }

  Variant(const Variant& other) : type_(kInternalTypeNull) { *this = other; }
  Variant& operator=(const Variant& other);
  Variant(Variant&& other) noexcept : type_(kInternalTypeNull) {
    *this = std::move(other);
  }
  Variant& operator=(Variant&& other) noexcept;
#endif  // SWIG

  ~Variant() { Clear(); }

  bool operator==(const Variant& other) const;
  bool operator<(const Variant& other) const;
  bool operator!=(const Variant& other) const { return !(*this == other); }
  bool operator>(const Variant& other) const { return other < *this; }
  bool operator>=(const Variant& other) const { return !(*this < other); }
  bool operator<=(const Variant& other) const { return !(*this > other); }

  void Clear(Type new_type = kTypeNull);

  static Variant Null() { return Variant(); }
  static Variant Zero() { return Variant::FromInt64(0L); }
  static Variant One() { return Variant::FromInt64(1L); }
  static Variant ZeroPointZero() { return Variant::FromDouble(0.0); }
  static Variant OnePointZero() { return Variant::FromDouble(1.0); }
  static Variant False() { return Variant::FromBool(false); }
  static Variant True() { return Variant::FromBool(true); }
  static Variant EmptyString() { return Variant::FromStaticString(""); }
  static Variant EmptyMutableString() {
    Variant v;
    v.type_ = kInternalTypeSmallString;
    v.value_.small_string[0] = '\0';
    return v;
  }
  static Variant EmptyVector() {
    Variant v;
    v.Clear(kTypeVector);
    return v;
  }
  static Variant EmptyMap() {
    Variant v;
    v.Clear(kTypeMap);
    return v;
  }
  static Variant EmptyMutableBlob(size_t size_bytes) {
    return FromMutableBlob(nullptr, size_bytes);
  }

  Type type() const {
    return type_ == kInternalTypeSmallString ? kTypeMutableString
                                             : static_cast<Type>(type_);
  }

  bool is_null() const { return type() == kTypeNull; }
  bool is_int64() const { return type() == kTypeInt64; }
  bool is_double() const { return type() == kTypeDouble; }
  bool is_bool() const { return type() == kTypeBool; }
  bool is_vector() const { return type() == kTypeVector; }
  bool is_map() const { return type() == kTypeMap; }
  bool is_static_string() const { return type() == kTypeStaticString; }
  bool is_mutable_string() const { return type() == kTypeMutableString; }
  bool is_string() const { return is_static_string() || is_mutable_string(); }
  bool is_static_blob() const { return type() == kTypeStaticBlob; }
  bool is_mutable_blob() const { return type() == kTypeMutableBlob; }
  bool is_blob() const { return is_static_blob() || is_mutable_blob(); }
  bool is_numeric() const { return is_int64() || is_double(); }
  bool is_fundamental_type() const {
    return is_int64() || is_double() || is_string() || is_bool() || is_null();
  }
  bool is_container_type() const { return is_vector() || is_map(); }

  Variant AsString() const;
  Variant AsInt64() const;
  Variant AsDouble() const;
  Variant AsBool() const;

  std::string& mutable_string() {
    if (type_ == kInternalTypeStaticString ||
        type_ == kInternalTypeSmallString) {
      set_mutable_string(string_value(), false);
    }
    assert_is_type(kTypeMutableString);
    return *value_.mutable_string_value;
  }

  size_t blob_size() const {
    assert_is_blob();
    return value_.blob_value.size;
  }

  const uint8_t* blob_data() const {
    assert_is_blob();
    return value_.blob_value.ptr != nullptr ? value_.blob_value.ptr
                                            : EmptyBlobData();
  }

  uint8_t* mutable_blob_data() {
    if (type_ == kInternalTypeStaticBlob) {
      set_mutable_blob(blob_data(), blob_size());
    }
    assert_is_type(kTypeMutableBlob);
    return const_cast<uint8_t*>(value_.blob_value.ptr);
  }

  uint8_t* mutable_blob_data() const {
    assert_is_type(kTypeMutableBlob);
    return const_cast<uint8_t*>(value_.blob_value.ptr);
  }

  std::vector<Variant>& vector() {
    assert_is_type(kTypeVector);
    return *value_.vector_value;
  }

  std::map<Variant, Variant>& map() {
    assert_is_type(kTypeMap);
    return *value_.map_value;
  }

  int64_t int64_value() const {
    assert_is_type(kTypeInt64);
    return value_.int64_value;
  }

  double double_value() const {
    assert_is_type(kTypeDouble);
    return value_.double_value;
  }

  bool bool_value() const {
    assert_is_type(kTypeBool);
    return value_.bool_value;
  }

  const char* string_value() const {
    assert_is_string();
    if (type_ == kInternalTypeMutableString)
      return value_.mutable_string_value->c_str();
    if (type_ == kInternalTypeStaticString) return value_.static_string_value;
    return value_.small_string;
  }

  std::string mutable_string() const {
    assert_is_string();
    return type_ == kInternalTypeMutableString ? *value_.mutable_string_value
                                               : std::string(string_value());
  }

  const std::vector<Variant>& vector() const {
    assert_is_type(kTypeVector);
    return *value_.vector_value;
  }

  const std::map<Variant, Variant>& map() const {
    assert_is_type(kTypeMap);
    return *value_.map_value;
  }

  void set_null() { Clear(kTypeNull); }
  void set_int64_value(int64_t value) {
    Clear(kTypeInt64);
    value_.int64_value = value;
  }
  void set_double_value(double value) {
    Clear(kTypeDouble);
    value_.double_value = value;
  }
  void set_bool_value(bool value) {
    Clear(kTypeBool);
    value_.bool_value = value;
  }

  void set_string_value(const char* value) {
    const char* safe_value = value != nullptr ? value : "";
    if (type_ == kInternalTypeMutableString ||
        type_ == kInternalTypeSmallString) {
      const char* current = string_value();
      size_t len = type_ == kInternalTypeMutableString
                       ? value_.mutable_string_value->size()
                       : strlen(value_.small_string);
      if (safe_value >= current && safe_value <= current + len) {
        set_mutable_string(std::string(safe_value));
        return;
      }
    }
    Clear(kTypeStaticString);
    value_.static_string_value = safe_value;
  }

  void set_string_value(char* value) {
    const char* safe_value = value != nullptr ? value : "";
    size_t len = strlen(safe_value);
    if (len < kMaxSmallStringSize) {
      char buffer[kMaxSmallStringSize];
      memcpy(buffer, safe_value, len + 1);
      Clear(kTypeNull);
      type_ = kInternalTypeSmallString;
      memcpy(value_.small_string, buffer, len + 1);
    } else {
      set_mutable_string(std::string(safe_value, len), false);
    }
  }

  void set_string_value(const std::string& value) { set_mutable_string(value); }

#if !defined(SWIG)
  void set_string_value(std::string&& value) {
    set_mutable_string(std::move(value));
  }
#endif  // !defined(SWIG)

  void set_mutable_string(const std::string& value,
                          bool use_small_string = true) {
    if (use_small_string && value.size() < kMaxSmallStringSize &&
        value.find('\0') == std::string::npos) {
      char buffer[kMaxSmallStringSize];
      size_t len = value.size();
      memcpy(buffer, value.data(), len);
      buffer[len] = '\0';
      Clear(kTypeNull);
      type_ = kInternalTypeSmallString;
      memcpy(value_.small_string, buffer, len + 1);
    } else {
      if (type_ == kInternalTypeMutableString &&
          value_.mutable_string_value == &value) {
        return;
      }
      std::string copy(value);
      Clear(kTypeMutableString);
      *value_.mutable_string_value = std::move(copy);
    }
  }

#if !defined(SWIG)
  void set_mutable_string(std::string&& value, bool use_small_string = true) {
    if (use_small_string && value.size() < kMaxSmallStringSize &&
        value.find('\0') == std::string::npos) {
      char buffer[kMaxSmallStringSize];
      size_t len = value.size();
      memcpy(buffer, value.data(), len);
      buffer[len] = '\0';
      Clear(kTypeNull);
      type_ = kInternalTypeSmallString;
      memcpy(value_.small_string, buffer, len + 1);
    } else {
      if (type_ == kInternalTypeMutableString &&
          value_.mutable_string_value == &value) {
        return;
      }
      std::string moved(std::move(value));
      Clear(kTypeMutableString);
      *value_.mutable_string_value = std::move(moved);
    }
  }
#endif  // !defined(SWIG)

  void set_mutable_blob(const void* src_data, size_t size_bytes) {
    size_t alloc_size = size_bytes > 0 ? size_bytes : 1;
    uint8_t* dest_data = nullptr;
    if (size_bytes > 0 && src_data != nullptr) {
      dest_data = new uint8_t[alloc_size];
      memcpy(dest_data, src_data, size_bytes);
    } else {
      dest_data = new uint8_t[alloc_size]();
    }
    Clear(kTypeNull);
    type_ = kInternalTypeMutableBlob;
    set_blob_pointer(dest_data, size_bytes);
  }

  void set_static_blob(const void* static_data, size_t size_bytes) {
    size_t safe_size = static_data != nullptr ? size_bytes : 0;
    Clear(kTypeStaticBlob);
    set_blob_pointer(static_data, safe_size);
  }

  void set_vector(const std::vector<Variant>& value) {
    if (type_ == kInternalTypeVector && value_.vector_value == &value) return;
    std::vector<Variant> copy(value);
    Clear(kTypeVector);
    *value_.vector_value = std::move(copy);
  }

#if !defined(SWIG)
  void set_vector(std::vector<Variant>&& value) {
    if (type_ == kInternalTypeVector && value_.vector_value == &value) return;
    std::vector<Variant> moved(std::move(value));
    Clear(kTypeVector);
    *value_.vector_value = std::move(moved);
  }
#endif  // !defined(SWIG)

  void set_map(const std::map<Variant, Variant>& value) {
    if (type_ == kInternalTypeMap && value_.map_value == &value) return;
    std::map<Variant, Variant> copy(value);
    Clear(kTypeMap);
    *value_.map_value = std::move(copy);
  }

#if !defined(SWIG)
  void set_map(std::map<Variant, Variant>&& value) {
    if (type_ == kInternalTypeMap && value_.map_value == &value) return;
    std::map<Variant, Variant> moved(std::move(value));
    Clear(kTypeMap);
    *value_.map_value = std::move(moved);
  }
#endif  // !defined(SWIG)

  void AssignMutableString(std::string** str) {
    if (str != nullptr && *str != nullptr) {
      std::string* taken = *str;
      *str = nullptr;
      if (type_ == kInternalTypeMutableString &&
          value_.mutable_string_value == taken) {
        return;
      }
      Clear(kTypeNull);
      type_ = kInternalTypeMutableString;
      value_.mutable_string_value = taken;
    } else {
      Clear(kTypeMutableString);
    }
  }

  void AssignVector(std::vector<Variant>** vect) {
    if (vect != nullptr && *vect != nullptr) {
      std::vector<Variant>* taken = *vect;
      *vect = nullptr;
      if (type_ == kInternalTypeVector && value_.vector_value == taken) return;
      Clear(kTypeNull);
      type_ = kInternalTypeVector;
      value_.vector_value = taken;
    } else {
      Clear(kTypeVector);
    }
  }

  void AssignMap(std::map<Variant, Variant>** map) {
    if (map != nullptr && *map != nullptr) {
      std::map<Variant, Variant>* taken = *map;
      *map = nullptr;
      if (type_ == kInternalTypeMap && value_.map_value == taken) return;
      Clear(kTypeNull);
      type_ = kInternalTypeMap;
      value_.map_value = taken;
    } else {
      Clear(kTypeMap);
    }
  }

  static Variant FromInt64(int64_t value) { return Variant(value); }
  static Variant FromDouble(double value) { return Variant(value); }
  static Variant FromBool(bool value) { return Variant(value); }
  static Variant FromStaticString(const char* value) { return Variant(value); }
  static Variant FromMutableString(const std::string& value) {
    return Variant(value);
  }
  static Variant FromVector(const std::vector<Variant>& value) {
    return Variant(value);
  }
  static Variant FromMap(const std::map<Variant, Variant>& value) {
    return Variant(value);
  }

#if !defined(SWIG)
  static Variant FromMutableString(std::string&& value) {
    return Variant(std::move(value));
  }
  static Variant FromVector(std::vector<Variant>&& value) {
    return Variant(std::move(value));
  }
  static Variant FromMap(std::map<Variant, Variant>&& value) {
    return Variant(std::move(value));
  }
#endif  // !defined(SWIG)

  static Variant FromStaticBlob(const void* static_data, size_t size_bytes) {
    Variant v;
    v.set_static_blob(static_data, size_bytes);
    return v;
  }

  static Variant FromMutableBlob(const void* src_data, size_t size_bytes) {
    Variant v;
    v.set_mutable_blob(src_data, size_bytes);
    return v;
  }

#if !defined(SWIG)
  static Variant FromMutableBlob(const std::vector<uint8_t>& blob) {
    return FromMutableBlob(blob.data(), blob.size());
  }
#endif  // !defined(SWIG)

  static Variant MutableStringFromStaticString(const char* value) {
    const char* safe_value = value != nullptr ? value : "";
    size_t len = strlen(safe_value);
    Variant v;
    if (len < kMaxSmallStringSize) {
      v.type_ = kInternalTypeSmallString;
      memcpy(v.value_.small_string, safe_value, len + 1);
    } else {
      std::string* str = new std::string(safe_value, len);
      v.AssignMutableString(&str);
    }
    return v;
  }

  static const char* TypeName(Type type);

 private:
  enum InternalType {
    kInternalTypeNull = kTypeNull,
    kInternalTypeInt64 = kTypeInt64,
    kInternalTypeDouble = kTypeDouble,
    kInternalTypeBool = kTypeBool,
    kInternalTypeStaticString = kTypeStaticString,
    kInternalTypeMutableString = kTypeMutableString,
    kInternalTypeVector = kTypeVector,
    kInternalTypeMap = kTypeMap,
    kInternalTypeStaticBlob = kTypeStaticBlob,
    kInternalTypeMutableBlob = kTypeMutableBlob,
    kInternalTypeSmallString = kTypeMutableBlob + 1,
    kMaxTypeValue,
  };

  static const char* const kTypeNames[];

  void assert_is_type(Type type) const;
  void assert_is_not_type(Type type) const;
  void assert_is_string() const;
  void assert_is_blob() const;

  void set_blob_pointer(const void* blob_ptr, size_t size) {
    assert_is_blob();
    value_.blob_value.ptr = static_cast<const uint8_t*>(blob_ptr);
    value_.blob_value.size = blob_ptr != nullptr ? size : 0;
  }

  static const uint8_t* EmptyBlobData() noexcept {
    static constexpr uint8_t kEmptyBlob[1] = {0};
    return kEmptyBlob;
  }

  template <typename T>
  void set_value_t(T value) = delete;

  bool is_small_string() const { return type_ == kInternalTypeSmallString; }

  InternalType type_;

  typedef struct {
    const uint8_t* ptr;
    size_t size;
  } BlobValue;

  union Value {
    int64_t int64_value;
    double double_value;
    bool bool_value;
    const char* static_string_value;
    std::string* mutable_string_value;
    std::vector<Variant>* vector_value;
    std::map<Variant, Variant>* map_value;
    BlobValue blob_value;
    char small_string[sizeof(BlobValue)];
  } value_;

  static constexpr size_t kMaxSmallStringSize = sizeof(Value::small_string);

  friend class firebase::internal::VariantInternal;
};

template <>
inline void Variant::set_value_t<int64_t>(int64_t value) {
  set_int64_value(value);
}
template <>
inline void Variant::set_value_t<int>(int value) {
  set_int64_value(static_cast<int64_t>(value));
}
template <>
inline void Variant::set_value_t<int16_t>(int16_t value) {
  set_int64_value(static_cast<int64_t>(value));
}
template <>
inline void Variant::set_value_t<uint8_t>(uint8_t value) {
  set_int64_value(static_cast<int64_t>(value));
}
template <>
inline void Variant::set_value_t<int8_t>(int8_t value) {
  set_int64_value(static_cast<int64_t>(value));
}
template <>
inline void Variant::set_value_t<char>(char value) {
  set_int64_value(static_cast<int64_t>(value));
}
template <>
inline void Variant::set_value_t<double>(double value) {
  set_double_value(value);
}
template <>
inline void Variant::set_value_t<float>(float value) {
  set_double_value(static_cast<double>(value));
}
template <>
inline void Variant::set_value_t<bool>(bool value) {
  set_bool_value(value);
}
template <>
inline void Variant::set_value_t<const char*>(const char* value) {
  set_string_value(value);
}
template <>
inline void Variant::set_value_t<char*>(char* value) {
  set_string_value(value);
}

}  // namespace firebase

#endif  // FIREBASE_APP_SRC_INCLUDE_FIREBASE_VARIANT_H_
