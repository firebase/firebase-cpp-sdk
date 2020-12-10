#include "firestore/src/ios/field_value_ios.h"

#include <utility>

#include "firestore/src/common/macros.h"
#include "firestore/src/include/firebase/firestore/map_field_value.h"
#include "firestore/src/ios/converter_ios.h"
#include "firestore/src/ios/hard_assert_ios.h"
#include "Firestore/core/src/nanopb/byte_string.h"

namespace firebase {
namespace firestore {

namespace {

using nanopb::ByteString;

using Type = FieldValue::Type;

}  // namespace

// Constructors

FieldValueInternal::FieldValueInternal(bool value)
    : type_{Type::kBoolean}, value_{model::FieldValue::FromBoolean(value)} {}

FieldValueInternal::FieldValueInternal(int64_t value)
    : type_{Type::kInteger}, value_{model::FieldValue::FromInteger(value)} {}

FieldValueInternal::FieldValueInternal(double value)
    : type_{Type::kDouble}, value_{model::FieldValue::FromDouble(value)} {}

FieldValueInternal::FieldValueInternal(Timestamp value)
    : type_{Type::kTimestamp},
      value_{model::FieldValue::FromTimestamp(value)} {}

FieldValueInternal::FieldValueInternal(std::string value)
    : type_{Type::kString},
      value_{model::FieldValue::FromString(std::move(value))} {}

FieldValueInternal::FieldValueInternal(const uint8_t* value, size_t size)
    : type_{Type::kBlob},
      value_{model::FieldValue::FromBlob(ByteString{value, size})} {}

FieldValueInternal::FieldValueInternal(DocumentReference value)
    : type_{Type::kReference}, value_{std::move(value)} {}

FieldValueInternal::FieldValueInternal(GeoPoint value)
    : type_{Type::kGeoPoint}, value_{model::FieldValue::FromGeoPoint(value)} {}

FieldValueInternal::FieldValueInternal(std::vector<FieldValue> value)
    : type_{Type::kArray}, value_{std::move(value)} {}

FieldValueInternal::FieldValueInternal(MapFieldValue value)
    : type_{Type::kMap}, value_{std::move(value)} {}

// Accessors

bool FieldValueInternal::boolean_value() const {
  HARD_ASSERT_IOS(type_ == Type::kBoolean);
  return absl::get<model::FieldValue>(value_).boolean_value();
}

int64_t FieldValueInternal::integer_value() const {
  HARD_ASSERT_IOS(type_ == Type::kInteger);
  return absl::get<model::FieldValue>(value_).integer_value();
}

double FieldValueInternal::double_value() const {
  HARD_ASSERT_IOS(type_ == Type::kDouble);
  return absl::get<model::FieldValue>(value_).double_value();
}

Timestamp FieldValueInternal::timestamp_value() const {
  HARD_ASSERT_IOS(type_ == Type::kTimestamp);
  return absl::get<model::FieldValue>(value_).timestamp_value();
}

std::string FieldValueInternal::string_value() const {
  HARD_ASSERT_IOS(type_ == Type::kString);
  return absl::get<model::FieldValue>(value_).string_value();
}

const uint8_t* FieldValueInternal::blob_value() const {
  HARD_ASSERT_IOS(type_ == Type::kBlob);
  return absl::get<model::FieldValue>(value_).blob_value().data();
}

size_t FieldValueInternal::blob_size() const {
  HARD_ASSERT_IOS(type_ == Type::kBlob);
  return absl::get<model::FieldValue>(value_).blob_value().size();
}

DocumentReference FieldValueInternal::reference_value() const {
  HARD_ASSERT_IOS(type_ == Type::kReference);
  return absl::get<DocumentReference>(value_);
}

GeoPoint FieldValueInternal::geo_point_value() const {
  HARD_ASSERT_IOS(type_ == Type::kGeoPoint);
  return absl::get<model::FieldValue>(value_).geo_point_value();
}

std::vector<FieldValue> FieldValueInternal::array_value() const {
  HARD_ASSERT_IOS(type_ == Type::kArray);
  return absl::get<ArrayT>(value_);
}

MapFieldValue FieldValueInternal::map_value() const {
  HARD_ASSERT_IOS(type_ == Type::kMap);
  return absl::get<MapT>(value_);
}

std::vector<FieldValue> FieldValueInternal::array_transform_value() const {
  HARD_ASSERT_IOS(type_ == Type::kArrayUnion || type_ == Type::kArrayRemove);
  return absl::get<ArrayT>(value_);
}

std::int64_t FieldValueInternal::integer_increment_value() const {
  HARD_ASSERT_IOS(type_ == Type::kIncrementInteger);
  return absl::get<model::FieldValue>(value_).integer_value();
}

double FieldValueInternal::double_increment_value() const {
  HARD_ASSERT_IOS(type_ == Type::kIncrementDouble);
  return absl::get<model::FieldValue>(value_).double_value();
}

// Creating sentinels

FieldValue FieldValueInternal::Delete() {
  return MakePublic(
      FieldValueInternal{Type::kDelete, model::FieldValue::Null()});
}

FieldValue FieldValueInternal::ServerTimestamp() {
  return MakePublic(
      FieldValueInternal{Type::kServerTimestamp, model::FieldValue::Null()});
}

FieldValue FieldValueInternal::ArrayUnion(std::vector<FieldValue> elements) {
  return MakePublic(FieldValueInternal{Type::kArrayUnion, std::move(elements)});
}

FieldValue FieldValueInternal::ArrayRemove(std::vector<FieldValue> elements) {
  return MakePublic(
      FieldValueInternal{Type::kArrayRemove, std::move(elements)});
}

FieldValue FieldValueInternal::IntegerIncrement(std::int64_t by_value) {
  return MakePublic(FieldValueInternal{
      Type::kIncrementInteger, model::FieldValue::FromInteger(by_value)});
}

FieldValue FieldValueInternal::DoubleIncrement(double by_value) {
  return MakePublic(FieldValueInternal{
      Type::kIncrementDouble, model::FieldValue::FromDouble(by_value)});
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
      return absl::get<model::FieldValue>(lhs.value_) ==
             absl::get<model::FieldValue>(rhs.value_);

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
      HARD_FAIL_IOS(message.c_str());
    }
  }
}

}  // namespace firestore
}  // namespace firebase
