/*
 * Copyright 2018 Google
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

#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_INCLUDE_FIREBASE_FIRESTORE_FIELD_VALUE_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_INCLUDE_FIREBASE_FIRESTORE_FIELD_VALUE_H_

#include <cstdint>
#include <iosfwd>
#include <string>
#include <vector>

#include "firebase/firestore/map_field_value.h"

namespace firebase {

class Timestamp;

namespace firestore {

class DocumentReference;
class FieldValueInternal;
class GeoPoint;

/**
 * @brief A field value represents variant datatypes as stored by Firestore.
 *
 * FieldValue can be used when reading a particular field with
 * DocumentSnapshot::Get() or fields with DocumentSnapshot::GetData(). When
 * writing document fields with DocumentReference::Set() or
 * DocumentReference::Update(), it can also represent sentinel values in
 * addition to real data values.
 *
 * For a non-sentinel instance, you can check whether it is of a particular type
 * with is_foo() and get the value with foo_value(), where foo can be one of
 * null, boolean, integer, double, timestamp, string, blob, reference,
 * geo_point, array or map. If the instance is not of type foo, the call to
 * foo_value() will fail (and cause a crash).
 */
class FieldValue final {
 public:
  /**
   * The enumeration of all valid runtime types of FieldValue.
   */
  enum class Type {
    kNull,
    kBoolean,
    kInteger,
    kDouble,
    kTimestamp,
    kString,
    kBlob,
    kReference,
    kGeoPoint,
    kArray,
    kMap,
    // Below are sentinel types. Sentinel types can be passed to Firestore
    // methods as arguments, but are never returned from Firestore.
    kDelete,
    kServerTimestamp,
    kArrayUnion,
    kArrayRemove,
    kIncrementInteger,
    kIncrementDouble,
  };

  /**
   * @brief Creates an invalid FieldValue that has to be reassigned before it
   * can be used.
   *
   * Calling any member function on an invalid FieldValue will be a no-op. If
   * the function returns a value, it will return a zero, empty, or invalid
   * value, depending on the type of the value.
   */
  FieldValue();

  /**
   * @brief Copy constructor.
   *
   * `FieldValue` is immutable and can be efficiently copied (no deep copy is
   * performed).
   *
   * @param[in] other `FieldValue` to copy from.
   */
  FieldValue(const FieldValue& other);

  /**
   * @brief Move constructor.
   *
   * Moving is more efficient than copying for a `FieldValue`. After being moved
   * from, a `FieldValue` is equivalent to its default-constructed state.
   *
   * @param[in] other `FieldValue` to move data from.
   */
  FieldValue(FieldValue&& other) noexcept;

  ~FieldValue();

  /**
   * @brief Copy assignment operator.
   *
   * `FieldValue` is immutable and can be efficiently copied (no deep copy is
   * performed).
   *
   * @param[in] other `FieldValue` to copy from.
   *
   * @return Reference to the destination `FieldValue`.
   */
  FieldValue& operator=(const FieldValue& other);

  /**
   * @brief Move assignment operator.
   *
   * Moving is more efficient than copying for a `FieldValue`. After being moved
   * from, a `FieldValue` is equivalent to its default-constructed state.
   *
   * @param[in] other `FieldValue` to move data from.
   *
   * @return Reference to the destination `FieldValue`.
   */
  FieldValue& operator=(FieldValue&& other) noexcept;

  /**
   * @brief Constructs a FieldValue containing the given boolean value.
   */
  static FieldValue FromBoolean(bool value);

  /**
   * @brief Constructs a FieldValue containing the given 64-bit integer value.
   */
  static FieldValue FromInteger(int64_t value);

  /**
   * @brief Constructs a FieldValue containing the given double-precision
   * floating point value.
   */
  static FieldValue FromDouble(double value);

  /**
   * @brief Constructs a FieldValue containing the given Timestamp value.
   */
  static FieldValue FromTimestamp(Timestamp value);

  /**
   * @brief Constructs a FieldValue containing the given std::string value.
   */
  static FieldValue FromString(std::string value);

  /**
   * @brief Constructs a FieldValue containing the given blob value of given
   * size. `value` is copied into the returned FieldValue.
   */
  static FieldValue FromBlob(const uint8_t* value, size_t size);

  /**
   * @brief Constructs a FieldValue containing the given reference value.
   */
  static FieldValue FromReference(DocumentReference value);

  /**
   * @brief Constructs a FieldValue containing the given GeoPoint value.
   */
  static FieldValue FromGeoPoint(GeoPoint value);

  /**
   * @brief Constructs a FieldValue containing the given FieldValue vector
   * value.
   */
  static FieldValue FromArray(std::vector<FieldValue> value);

  /**
   * @brief Constructs a FieldValue containing the given FieldValue map value.
   */
  static FieldValue FromMap(MapFieldValue value);

  /** @brief Gets the current type contained in this FieldValue. */
  Type type() const;

  /** @brief Gets whether this FieldValue is currently null. */
  bool is_null() const { return type() == Type::kNull; }

  /** @brief Gets whether this FieldValue contains a boolean value. */
  bool is_boolean() const { return type() == Type::kBoolean; }

  /** @brief Gets whether this FieldValue contains an integer value. */
  bool is_integer() const { return type() == Type::kInteger; }

  /** @brief Gets whether this FieldValue contains a double value. */
  bool is_double() const { return type() == Type::kDouble; }

  /** @brief Gets whether this FieldValue contains a timestamp. */
  bool is_timestamp() const { return type() == Type::kTimestamp; }

  /** @brief Gets whether this FieldValue contains a string. */
  bool is_string() const { return type() == Type::kString; }

  /** @brief Gets whether this FieldValue contains a blob. */
  bool is_blob() const { return type() == Type::kBlob; }

  /**
   * @brief Gets whether this FieldValue contains a reference to a document in
   * the same Firestore.
   */
  bool is_reference() const { return type() == Type::kReference; }

  /** @brief Gets whether this FieldValue contains a GeoPoint. */
  bool is_geo_point() const { return type() == Type::kGeoPoint; }

  /** @brief Gets whether this FieldValue contains an array of FieldValues. */
  bool is_array() const { return type() == Type::kArray; }

  /** @brief Gets whether this FieldValue contains a map of std::string to
   * FieldValue. */
  bool is_map() const { return type() == Type::kMap; }

  /** @brief Gets the boolean value contained in this FieldValue. */
  bool boolean_value() const;

  /** @brief Gets the integer value contained in this FieldValue. */
  int64_t integer_value() const;

  /** @brief Gets the double value contained in this FieldValue. */
  double double_value() const;

  /** @brief Gets the timestamp value contained in this FieldValue. */
  Timestamp timestamp_value() const;

  /** @brief Gets the string value contained in this FieldValue. */
  std::string string_value() const;

  /** @brief Gets the blob value contained in this FieldValue. */
  const uint8_t* blob_value() const;

  /** @brief Gets the blob size contained in this FieldValue. */
  size_t blob_size() const;

  /** @brief Gets the DocumentReference contained in this FieldValue. */
  DocumentReference reference_value() const;

  /** @brief Gets the GeoPoint value contained in this FieldValue. */
  GeoPoint geo_point_value() const;

  /** @brief Gets the vector of FieldValues contained in this FieldValue. */
  std::vector<FieldValue> array_value() const;

  /**
   * @brief Gets the map of string to FieldValue contained in this FieldValue.
   */
  MapFieldValue map_value() const;

  /**
   * @brief Returns `true` if this `FieldValue` is valid, `false` if it is not
   * valid. An invalid `FieldValue` could be the result of:
   *   - Creating a `FieldValue` using the default constructor.
   *   - Calling `DocumentSnapshot::Get(field)` for a field that does not exist
   * in the document.
   *
   * @return `true` if this `FieldValue` is valid, `false` if this `FieldValue`
   * is invalid.
   */
  bool is_valid() const { return internal_ != nullptr; }

  /** @brief Constructs a null. */
  static FieldValue Null();

  /**
   * @brief Returns a sentinel for use with Update() to mark a field for
   * deletion.
   */
  static FieldValue Delete();

  /**
   * Returns a sentinel that can be used with Set() or Update() to include
   * a server-generated timestamp in the written data.
   */
  static FieldValue ServerTimestamp();

  /**
   * Returns a special value that can be used with Set() or Update() that tells
   * the server to union the given elements with any array value that already
   * exists on the server. Each specified element that doesn't already exist in
   * the array will be added to the end. If the field being modified is not
   * already an array, it will be overwritten with an array containing exactly
   * the specified elements.
   *
   * @param elements The elements to union into the array.
   * @return The FieldValue sentinel for use in a call to Set() or Update().
   */
  static FieldValue ArrayUnion(std::vector<FieldValue> elements);

  /**
   * Returns a special value that can be used with Set() or Update() that tells
   * the server to remove the given elements from any array value that already
   * exists on the server. All instances of each element specified will be
   * removed from the array. If the field being modified is not already an
   * array, it will be overwritten with an empty array.
   *
   * @param elements The elements to remove from the array.
   * @return The FieldValue sentinel for use in a call to Set() or Update().
   */
  static FieldValue ArrayRemove(std::vector<FieldValue> elements);

#if defined(INTERNAL_EXPERIMENTAL) || defined(SWIG)
  /**
   * Returns a special value that can be used with `Set()` or `Update()` that
   * tells the server to increment the field's current value by the given value.
   *
   * If the current field value is an integer, possible integer overflows are
   * resolved to `LONG_MAX` or `LONG_MIN`. If the current field value is a
   * double, both values will be interpreted as doubles and the arithmetic will
   * follow IEEE 754 semantics.
   *
   * If field is not an integer or a double, or if the field does not yet exist,
   * the transformation will set the field to the given value.
   *
   * @param by_value The integer value to increment by.
   * @return The FieldValue sentinel for use in a call to `Set()` or `Update().`
   */
  static FieldValue IntegerIncrement(int64_t by_value);
#endif  // if defined(INTERNAL_EXPERIMENTAL) || defined(SWIG)

#if defined(INTERNAL_EXPERIMENTAL) || defined(SWIG)
  /**
   * Returns a special value that can be used with `Set()` or `Update()` that
   * tells the server to increment the field's current value by the given value.
   *
   * If the current field value is an integer, possible integer overflows are
   * resolved to `LONG_MAX` or `LONG_MIN`. If the current field value is a
   * double, both values will be interpreted as doubles and the arithmetic will
   * follow IEEE 754 semantics.
   *
   * If field is not an integer or a double, or if the field does not yet exist,
   * the transformation will set the field to the given value.
   *
   * @param by_value The double value to increment by.
   * @return The FieldValue sentinel for use in a call to `Set()` or `Update().`
   */
  static FieldValue DoubleIncrement(double by_value);
#endif  // if defined(INTERNAL_EXPERIMENTAL) || defined(SWIG)

  /**
   * Returns a string representation of this `FieldValue` for logging/debugging
   * purposes.
   *
   * @note the exact string representation is unspecified and subject to
   * change; don't rely on the format of the string.
   */
  std::string ToString() const;

  /**
   * Outputs the string representation of this `FieldValue` to the given stream.
   *
   * @see `ToString()` for comments on the representation format.
   */
  friend std::ostream& operator<<(std::ostream& out, const FieldValue& value);

 private:
  friend class DocumentReferenceInternal;
  friend class DocumentSnapshotInternal;
  friend class FieldValueInternal;
  friend class FirestoreInternal;
  friend class QueryInternal;
  friend class TransactionInternal;
  friend class Wrapper;
  friend class WriteBatchInternal;
  friend struct ConverterImpl;
  friend bool operator==(const FieldValue& lhs, const FieldValue& rhs);

  explicit FieldValue(FieldValueInternal* internal);

  FieldValueInternal* internal_ = nullptr;
};

/** Checks `lhs` and `rhs` for equality. */
bool operator==(const FieldValue& lhs, const FieldValue& rhs);

/** Checks `lhs` and `rhs` for inequality. */
inline bool operator!=(const FieldValue& lhs, const FieldValue& rhs) {
  return !(lhs == rhs);
}

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_INCLUDE_FIREBASE_FIRESTORE_FIELD_VALUE_H_
