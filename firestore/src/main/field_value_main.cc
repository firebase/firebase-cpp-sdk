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

#include "firestore/src/main/field_value_main.h"

#include <utility>

#include "Firestore/core/src/nanopb/byte_string.h"
#include "Firestore/core/src/nanopb/nanopb_util.h"
#include "firestore/src/common/hard_assert_common.h"
#include "firestore/src/common/macros.h"
#include "firestore/src/include/firebase/firestore/map_field_value.h"
#include "firestore/src/main/converter_main.h"

namespace firebase {
namespace firestore {

namespace {

using nanopb::ByteString;
using nanopb::MakeBytesArray;
using nanopb::MakeMessage;
using nanopb::MakeString;
using nanopb::Message;
using nanopb::SharedMessage;

using Type = FieldValue::Type;

}  // namespace

// Constructors

FieldValueInternal::FieldValueInternal() {
  auto& value = absl::get<SharedMessage<google_firestore_v1_Value>>(value_);
  value->which_value_type = google_firestore_v1_Value_map_value_tag;
  value->map_value = {};
}

FieldValueInternal::FieldValueInternal(bool value) : type_{Type::kBoolean} {
  auto& proto = GetProtoValue();
  proto->which_value_type = google_firestore_v1_Value_boolean_value_tag;
  proto->boolean_value = value;
}

FieldValueInternal::FieldValueInternal(int64_t value) : type_{Type::kInteger} {
  auto& proto = GetProtoValue();
  proto->which_value_type = google_firestore_v1_Value_integer_value_tag;
  proto->integer_value = value;
}

FieldValueInternal::FieldValueInternal(double value) : type_{Type::kDouble} {
  auto& proto = GetProtoValue();
  proto->which_value_type = google_firestore_v1_Value_double_value_tag;
  proto->double_value = value;
}

FieldValueInternal::FieldValueInternal(Timestamp value)
    : type_{Type::kTimestamp} {
  auto& proto = GetProtoValue();
  proto->which_value_type = google_firestore_v1_Value_timestamp_value_tag;
  proto->timestamp_value.seconds = value.seconds();
  proto->timestamp_value.nanos = value.nanoseconds();
}

FieldValueInternal::FieldValueInternal(std::string value)
    : type_{Type::kString} {
  auto& proto = GetProtoValue();
  proto->which_value_type = google_firestore_v1_Value_string_value_tag;
  proto->string_value = MakeBytesArray(value);
}

FieldValueInternal::FieldValueInternal(const uint8_t* value, size_t size)
    : type_{Type::kBlob} {
  auto& proto = GetProtoValue();
  proto->which_value_type = google_firestore_v1_Value_bytes_value_tag;
  proto->bytes_value = MakeBytesArray(value, size);
}

FieldValueInternal::FieldValueInternal(DocumentReference value)
    : type_{Type::kReference}, value_{std::move(value)} {}

FieldValueInternal::FieldValueInternal(GeoPoint value)
    : type_{Type::kGeoPoint} {
  auto& proto = GetProtoValue();
  proto->which_value_type = google_firestore_v1_Value_geo_point_value_tag;
  proto->geo_point_value.latitude = value.latitude();
  proto->geo_point_value.longitude = value.longitude();
}

FieldValueInternal::FieldValueInternal(std::vector<FieldValue> value)
    : type_{Type::kArray}, value_{std::move(value)} {}

FieldValueInternal::FieldValueInternal(MapFieldValue value)
    : type_{Type::kMap}, value_{std::move(value)} {}

// Accessors

bool FieldValueInternal::boolean_value() const {
  SIMPLE_HARD_ASSERT(type_ == Type::kBoolean);
  return GetProtoValue()->boolean_value;
}

int64_t FieldValueInternal::integer_value() const {
  SIMPLE_HARD_ASSERT(type_ == Type::kInteger);
  return GetProtoValue()->integer_value;
}

double FieldValueInternal::double_value() const {
  SIMPLE_HARD_ASSERT(type_ == Type::kDouble);
  return GetProtoValue()->double_value;
}

Timestamp FieldValueInternal::timestamp_value() const {
  SIMPLE_HARD_ASSERT(type_ == Type::kTimestamp);
  auto& value = GetProtoValue()->timestamp_value;
  return Timestamp{value.seconds, value.nanos};
}

std::string FieldValueInternal::string_value() const {
  SIMPLE_HARD_ASSERT(type_ == Type::kString);
  return MakeString(GetProtoValue()->string_value);
}

const uint8_t* FieldValueInternal::blob_value() const {
  SIMPLE_HARD_ASSERT(type_ == Type::kBlob);
  auto* value = GetProtoValue()->bytes_value;
  return value ? value->bytes : nullptr;
}

size_t FieldValueInternal::blob_size() const {
  SIMPLE_HARD_ASSERT(type_ == Type::kBlob);
  auto* value = GetProtoValue()->bytes_value;
  return value ? value->size : 0;
}

DocumentReference FieldValueInternal::reference_value() const {
  SIMPLE_HARD_ASSERT(type_ == Type::kReference);
  return absl::get<DocumentReference>(value_);
}

GeoPoint FieldValueInternal::geo_point_value() const {
  SIMPLE_HARD_ASSERT(type_ == Type::kGeoPoint);
  auto& value = GetProtoValue()->geo_point_value;
  return GeoPoint{value.latitude, value.longitude};
}

std::vector<FieldValue> FieldValueInternal::array_value() const {
  SIMPLE_HARD_ASSERT(type_ == Type::kArray);
  return absl::get<ArrayT>(value_);
}

MapFieldValue FieldValueInternal::map_value() const {
  SIMPLE_HARD_ASSERT(type_ == Type::kMap);
  return absl::get<MapT>(value_);
}

std::vector<FieldValue> FieldValueInternal::array_transform_value() const {
  SIMPLE_HARD_ASSERT(type_ == Type::kArrayUnion || type_ == Type::kArrayRemove);
  return absl::get<ArrayT>(value_);
}

std::int64_t FieldValueInternal::integer_increment_value() const {
  SIMPLE_HARD_ASSERT(type_ == Type::kIncrementInteger);
  return GetProtoValue()->integer_value;
}

double FieldValueInternal::double_increment_value() const {
  SIMPLE_HARD_ASSERT(type_ == Type::kIncrementDouble);
  return GetProtoValue()->double_value;
}

// Creating sentinels

FieldValue FieldValueInternal::Delete() {
  Message<google_firestore_v1_Value> value;
  value->which_value_type = google_firestore_v1_Value_map_value_tag;
  value->map_value = {};
  return MakePublic(FieldValueInternal{Type::kDelete, std::move(value)});
}

FieldValue FieldValueInternal::ServerTimestamp() {
  Message<google_firestore_v1_Value> value;
  value->which_value_type = google_firestore_v1_Value_map_value_tag;
  value->map_value = {};
  return MakePublic(
      FieldValueInternal{Type::kServerTimestamp, std::move(value)});
}

FieldValue FieldValueInternal::ArrayUnion(std::vector<FieldValue> elements) {
  return MakePublic(FieldValueInternal{Type::kArrayUnion, std::move(elements)});
}

FieldValue FieldValueInternal::ArrayRemove(std::vector<FieldValue> elements) {
  return MakePublic(
      FieldValueInternal{Type::kArrayRemove, std::move(elements)});
}

FieldValue FieldValueInternal::IntegerIncrement(std::int64_t by_value) {
  Message<google_firestore_v1_Value> value;
  value->which_value_type = google_firestore_v1_Value_integer_value_tag;
  value->integer_value = by_value;
  return MakePublic(
      FieldValueInternal{Type::kIncrementInteger, std::move(value)});
}

FieldValue FieldValueInternal::DoubleIncrement(double by_value) {
  Message<google_firestore_v1_Value> value;
  value->which_value_type = google_firestore_v1_Value_double_value_tag;
  value->double_value = by_value;
  return MakePublic(
      FieldValueInternal{Type::kIncrementDouble, std::move(value)});
}

// Equality operator

bool operator==(const FieldValueInternal& lhs, const FieldValueInternal& rhs) {
  using ArrayT = FieldValueInternal::ArrayT;
  using MapT = FieldValueInternal::MapT;

  auto type = lhs.type();
  if (type != rhs.type()) {
    return false;
  }

  switch (type) {
    case Type::kNull:
    case Type::kBoolean:
    case Type::kInteger:
    case Type::kDouble:
    case Type::kTimestamp:
    case Type::kString:
    case Type::kBlob:
    case Type::kGeoPoint:
      // Sentinels
    case Type::kIncrementDouble:
    case Type::kIncrementInteger:
    case Type::kDelete:
    case Type::kServerTimestamp:
      return *(absl::get<SharedMessage<google_firestore_v1_Value>>(
                 lhs.value_)) ==
             *(absl::get<SharedMessage<google_firestore_v1_Value>>(rhs.value_));

    case Type::kReference:
      return absl::get<DocumentReference>(lhs.value_) ==
             absl::get<DocumentReference>(rhs.value_);

    case Type::kArray:
    case Type::kArrayRemove:
    case Type::kArrayUnion:
      return absl::get<ArrayT>(lhs.value_) == absl::get<ArrayT>(rhs.value_);

    case Type::kMap:
      return absl::get<MapT>(lhs.value_) == absl::get<MapT>(rhs.value_);
  }

  FIRESTORE_UNREACHABLE();
}

std::string Describe(Type type) {
  switch (type) {
    // Scalars
    case Type::kNull:
      return "FieldValue::Null()";
    case Type::kBoolean:
      return "FieldValue::Boolean()";
    case Type::kInteger:
      return "FieldValue::Integer()";
    case Type::kDouble:
      return "FieldValue::Double()";
    case Type::kTimestamp:
      return "FieldValue::Timestamp()";
    case Type::kString:
      return "FieldValue::String()";
    case Type::kBlob:
      return "FieldValue::Blob()";
    case Type::kReference:
      return "FieldValue::Reference()";
    case Type::kGeoPoint:
      return "FieldValue::GeoPoint()";
    // Containers
    case Type::kArray:
      return "FieldValue::Array()";
    case Type::kMap:
      return "FieldValue::Map()";
    // Sentinels
    case Type::kDelete:
      return "FieldValue::Delete()";
    case Type::kServerTimestamp:
      return "FieldValue::ServerTimestamp()";
    case Type::kArrayUnion:
      return "FieldValue::ArrayUnion()";
    case Type::kArrayRemove:
      return "FieldValue::ArrayRemove()";
    case Type::kIncrementInteger:
    case Type::kIncrementDouble:
      return "FieldValue::Increment()";
    default: {
      // TODO(b/147444199): use string formatting.
      // HARD_FAIL("Unexpected type '%s'", type);
      auto message = std::string("Unexpected type '") +
                     std::to_string(static_cast<int>(type)) + "'";
      SIMPLE_HARD_FAIL(message.c_str());
    }
  }
}

// Helpers

const SharedMessage<google_firestore_v1_Value>&
FieldValueInternal::GetProtoValue() const {
  return absl::get<SharedMessage<google_firestore_v1_Value>>(value_);
}

SharedMessage<google_firestore_v1_Value>& FieldValueInternal::GetProtoValue() {
  return absl::get<SharedMessage<google_firestore_v1_Value>>(value_);
}

}  // namespace firestore
}  // namespace firebase
