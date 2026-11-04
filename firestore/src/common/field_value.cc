// Copyright 2020 Google LLC
#include "firestore/src/include/firebase/firestore/field_value.h"

#include <cstdint>
#include <iomanip>
#include <ostream>
#include <sstream>
#include <utility>
#include "firebase/firestore/geo_point.h"
#include "firebase/firestore/timestamp.h"
#include "firestore/src/common/hard_assert_common.h"
#include "firestore/src/common/to_string.h"
#include "firestore/src/common/util.h"
#include "firestore/src/include/firebase/firestore/document_reference.h"
#if defined(__ANDROID__)
#include "firestore/src/android/field_value_android.h"
#else
#include "firestore/src/main/field_value_main.h"
#endif

namespace firebase::firestore {
namespace {

using Type = FieldValue::Type;

template <typename T>
std::string ValueToString(const T& value) {
  std::ostringstream stream;
  stream << value;
  return stream.str();
}

std::string ValueToString(const std::vector<FieldValue>& value) {
  std::string result = "[";
  bool is_first = true;
  for (const FieldValue& e : value) {
    if (!std::exchange(is_first, false)) result += ", ";
    result += e.ToString();
  }
  result += ']';
  return result;
}

std::string ValueToString(const uint8_t* blob, size_t size) {
  std::ostringstream stream;
  stream << "Blob(";
  if (blob != nullptr && size > 0) {
    stream << std::hex << std::setfill('0');
    for (size_t i = 0; i < size; ++i) {
      if (i > 0) stream << " ";
      stream << std::setw(2) << static_cast<int>(blob[i]);
    }
  }
  stream << ")";
  return stream.str();
}

}  // namespace

FieldValue::FieldValue() {}
FieldValue::FieldValue(const FieldValue& value)
    : internal_(value.internal_ ? new FieldValueInternal(*value.internal_) : nullptr) {}
FieldValue::FieldValue(FieldValue&& value) noexcept
    : internal_(std::exchange(value.internal_, nullptr)) {}
FieldValue::FieldValue(FieldValueInternal* internal) : internal_(internal) {
  SIMPLE_HARD_ASSERT(internal != nullptr);
}
FieldValue::~FieldValue() { delete std::exchange(internal_, nullptr); }

FieldValue& FieldValue::operator=(const FieldValue& value) {
  FieldValue tmp(value);
  std::swap(internal_, tmp.internal_);
  return *this;
}

FieldValue& FieldValue::operator=(FieldValue&& value) noexcept {
  std::swap(internal_, value.internal_);
  return *this;
}

FieldValue FieldValue::Boolean(bool value) { return FieldValue{new FieldValueInternal(value)}; }
FieldValue FieldValue::Integer(int64_t value) { return FieldValue{new FieldValueInternal(value)}; }
FieldValue FieldValue::Double(double value) { return FieldValue{new FieldValueInternal(value)}; }
FieldValue FieldValue::Timestamp(firebase::Timestamp value) {
  return FieldValue{new FieldValueInternal(value)};
}
FieldValue FieldValue::String(std::string value) {
  return FieldValue{new FieldValueInternal(std::move(value))};
}
FieldValue FieldValue::Blob(const uint8_t* value, size_t size) {
  return FieldValue{new FieldValueInternal(value, size)};
}
FieldValue FieldValue::Reference(firebase::firestore::DocumentReference value) {
  return FieldValue{new FieldValueInternal(std::move(value))};
}
FieldValue FieldValue::GeoPoint(firebase::firestore::GeoPoint value) {
  return FieldValue{new FieldValueInternal(value)};
}
FieldValue FieldValue::Array(std::vector<FieldValue> value) {
  return FieldValue{new FieldValueInternal(std::move(value))};
}
FieldValue FieldValue::Map(MapFieldValue value) {
  return FieldValue{new FieldValueInternal(std::move(value))};
}

Type FieldValue::type() const { return internal_ ? internal_->type() : Type::kNull; }
bool FieldValue::boolean_value() const { return internal_ ? internal_->boolean_value() : false; }
int64_t FieldValue::integer_value() const { return internal_ ? internal_->integer_value() : 0; }
double FieldValue::double_value() const { return internal_ ? internal_->double_value() : 0.0; }
Timestamp FieldValue::timestamp_value() const {
  return internal_ ? internal_->timestamp_value() : firebase::Timestamp{};
}
std::string FieldValue::string_value() const {
  return internal_ ? internal_->string_value() : "";
}
const uint8_t* FieldValue::blob_value() const {
  return internal_ ? internal_->blob_value() : nullptr;
}
size_t FieldValue::blob_size() const { return internal_ ? internal_->blob_size() : 0; }
DocumentReference FieldValue::reference_value() const {
  return internal_ ? internal_->reference_value() : DocumentReference{};
}
GeoPoint FieldValue::geo_point_value() const {
  return internal_ ? internal_->geo_point_value() : firebase::firestore::GeoPoint{};
}
std::vector<FieldValue> FieldValue::array_value() const {
  return internal_ ? internal_->array_value() : std::vector<FieldValue>{};
}
MapFieldValue FieldValue::map_value() const {
  return internal_ ? internal_->map_value() : MapFieldValue{};
}

FieldValue FieldValue::Null() { return FieldValue{new FieldValueInternal()}; }
FieldValue FieldValue::Delete() { return FieldValueInternal::Delete(); }
FieldValue FieldValue::ServerTimestamp() { return FieldValueInternal::ServerTimestamp(); }
FieldValue FieldValue::ArrayUnion(std::vector<FieldValue> elements) {
  return FieldValueInternal::ArrayUnion(std::move(elements));
}
FieldValue FieldValue::ArrayRemove(std::vector<FieldValue> elements) {
  return FieldValueInternal::ArrayRemove(std::move(elements));
}
FieldValue FieldValue::IntegerIncrement(int64_t by_value) {
  return FieldValueInternal::IntegerIncrement(by_value);
}
FieldValue FieldValue::DoubleIncrement(double by_value) {
  return FieldValueInternal::DoubleIncrement(by_value);
}

std::string FieldValue::ToString() const {
  if (!is_valid()) return "<invalid>";
  switch (type()) {
    case Type::kNull: return "null";
    case Type::kBoolean: return boolean_value() ? "true" : "false";
    case Type::kInteger: return ValueToString(integer_value());
    case Type::kDouble: return ValueToString(double_value());
    case Type::kTimestamp: return timestamp_value().ToString();
    case Type::kString: return std::string("'") + string_value() + "'";
    case Type::kBlob: return ValueToString(blob_value(), blob_size());
    case Type::kReference: return reference_value().ToString();
    case Type::kGeoPoint: return geo_point_value().ToString();
    case Type::kArray: return ValueToString(array_value());
    case Type::kMap: return ::firebase::firestore::ToString(map_value());
    case Type::kDelete: return "FieldValue::Delete()";
    case Type::kServerTimestamp: return "FieldValue::ServerTimestamp()";
    case Type::kArrayUnion: return "FieldValue::ArrayUnion()";
    case Type::kArrayRemove: return "FieldValue::ArrayRemove()";
    case Type::kIncrementInteger:
    case Type::kIncrementDouble:
      return "FieldValue::Increment()";
  }
  SIMPLE_HARD_FAIL("Unexpected FieldValue type");
}

bool operator==(const FieldValue& lhs, const FieldValue& rhs) {
  return EqualityCompare(lhs.internal_, rhs.internal_);
}

std::ostream& operator<<(std::ostream& out, const FieldValue& value) {
  return out << value.ToString();
}

}  // namespace firebase::firestore
