/*
 * Copyright 2021 Google LLC
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

#ifndef FIREBASE_FIRESTORE_SRC_MAIN_FIELD_VALUE_MAIN_H_
#define FIREBASE_FIRESTORE_SRC_MAIN_FIELD_VALUE_MAIN_H_

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "Firestore/Protos/nanopb/google/firestore/v1/document.nanopb.h"
#include "Firestore/core/src/nanopb/message.h"
#include "absl/types/variant.h"
#include "app/src/assert.h"
#include "firebase/firestore/geo_point.h"
#include "firebase/firestore/timestamp.h"
#include "firestore/src/include/firebase/firestore/document_reference.h"
#include "firestore/src/include/firebase/firestore/field_value.h"
#include "firestore/src/main/firestore_main.h"

#if defined(__ANDROID__)
#error "This header should not be used on Android."
#endif

namespace firebase {
namespace firestore {

class FieldValueInternal {
 public:
  FieldValueInternal();

  explicit FieldValueInternal(bool value);
  explicit FieldValueInternal(int64_t value);
  explicit FieldValueInternal(double value);
  explicit FieldValueInternal(Timestamp value);
  explicit FieldValueInternal(std::string value);
  FieldValueInternal(const uint8_t* value, size_t size);
  explicit FieldValueInternal(DocumentReference value);
  explicit FieldValueInternal(GeoPoint value);
  explicit FieldValueInternal(std::vector<FieldValue> value);
  explicit FieldValueInternal(MapFieldValue value);

  FieldValue::Type type() const { return type_; }

  bool boolean_value() const;
  int64_t integer_value() const;
  double double_value() const;
  Timestamp timestamp_value() const;
  std::string string_value() const;
  const uint8_t* blob_value() const;
  size_t blob_size() const;
  DocumentReference reference_value() const;
  GeoPoint geo_point_value() const;

  std::vector<FieldValue> array_value() const;
  MapFieldValue map_value() const;

  std::vector<FieldValue> array_transform_value() const;
  double double_increment_value() const;
  std::int64_t integer_increment_value() const;

  static FieldValue Delete();
  static FieldValue ServerTimestamp();
  static FieldValue ArrayUnion(std::vector<FieldValue> elements);
  static FieldValue ArrayRemove(std::vector<FieldValue> elements);
  static FieldValue IntegerIncrement(std::int64_t by_value);
  static FieldValue DoubleIncrement(double by_value);

 private:
  friend class FirestoreInternal;
  friend bool operator==(const FieldValueInternal& lhs,
                         const FieldValueInternal& rhs);

  using ArrayT = std::vector<FieldValue>;
  using MapT = MapFieldValue;

  template <typename T>
  explicit FieldValueInternal(FieldValue::Type type, T value)
      : type_{type}, value_{std::move(value)} {}

  /** Returns the underlying value as a google_firestore_v1_Value proto. */
  const nanopb::SharedMessage<google_firestore_v1_Value>& GetProtoValue() const;

  /** Returns the underlying value as a google_firestore_v1_Value proto. */
  nanopb::SharedMessage<google_firestore_v1_Value>& GetProtoValue();

  FieldValue::Type type_ = FieldValue::Type::kNull;
  // Note: it's impossible to roundtrip between a `DocumentReference` and
  // `google_firestore_v1_ReferenceValue`, because the latter omits some
  // information from the former (`shared_ptr` to the Firestore instance). For
  // that reason, just store the `DocumentReference` directly in the `variant`.
  absl::variant<nanopb::SharedMessage<google_firestore_v1_Value>,
                DocumentReference,
                ArrayT,
                MapT>
      value_ = nanopb::MakeSharedMessage<google_firestore_v1_Value>({});
};

bool operator==(const FieldValueInternal& lhs, const FieldValueInternal& rhs);

std::string Describe(FieldValue::Type type);

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_SRC_MAIN_FIELD_VALUE_MAIN_H_
