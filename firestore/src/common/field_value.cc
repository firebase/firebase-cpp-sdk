#include "firestore/src/include/firebase/firestore/field_value.h"

#include <iomanip>
#include <ostream>
#include <sstream>

#include "app/meta/move.h"
#include "app/src/assert.h"

#include "firestore/src/common/to_string.h"
#include "firestore/src/include/firebase/firestore/document_reference.h"
#include "firebase/firestore/geo_point.h"
#include "firebase/firestore/timestamp.h"
#if defined(__ANDROID__)
#include "firestore/src/android/field_value_android.h"
#elif defined(FIRESTORE_STUB_BUILD)
#include "firestore/src/stub/field_value_stub.h"
#else
#include "firestore/src/ios/field_value_ios.h"
#endif  // defined(__ANDROID__)

namespace firebase {
namespace firestore {

namespace {

using Type = FieldValue::Type;

template <typename T>
std::string ValueToString(const T& value) {
  // TODO(stlport): use `std::to_string` once possible.
  std::ostringstream stream;
  stream << value;
  return stream.str();
}

// TODO(varconst): optional indentation.
std::string ValueToString(const std::vector<FieldValue>& value) {
  std::string result = "[";

  bool is_first = true;
  for (const FieldValue& e : value) {
    if (!is_first) {
      result += ", ";
    }
    is_first = false;

    result += e.ToString();
  }

  result += ']';
  return result;
}

// The output will look like "00 0a"
std::string ValueToString(const uint8_t* blob, size_t size) {
  // TODO(varconst): not the most efficient implementation.
  std::ostringstream stream;
  stream << "Blob(";

  stream << std::hex << std::setfill('0');
  bool is_first = true;
  for (const auto* ptr = blob; ptr != blob + size; ++ptr) {
    if (!is_first) {
      stream << " ";
    }
    is_first = false;

    stream << std::setw(2) << static_cast<int>(*ptr);
  }

  stream << ")";
  return stream.str();
}

}  // namespace

FieldValue::FieldValue() {}

FieldValue::FieldValue(const FieldValue& value) {
  if (value.internal_) {
    internal_ = new FieldValueInternal(*value.internal_);
  }
}

FieldValue::FieldValue(FieldValue&& value) noexcept {
  std::swap(internal_, value.internal_);
}

FieldValue::FieldValue(FieldValueInternal* internal) : internal_(internal) {
  FIREBASE_ASSERT(internal != nullptr);
}

FieldValue::~FieldValue() {
  delete internal_;
  internal_ = nullptr;
}

FieldValue& FieldValue::operator=(const FieldValue& value) {
  if (this == &value) {
    return *this;
  }

  delete internal_;
  if (value.internal_) {
    internal_ = new FieldValueInternal(*value.internal_);
  } else {
    internal_ = nullptr;
  }
  return *this;
}

FieldValue& FieldValue::operator=(FieldValue&& value) noexcept {
  if (this == &value) {
    return *this;
  }

  delete internal_;
  internal_ = value.internal_;
  value.internal_ = nullptr;
  return *this;
}

/* static */
FieldValue FieldValue::Boolean(bool value) {
  return FieldValue{new FieldValueInternal(value)};
}

/* static */
FieldValue FieldValue::Integer(int64_t value) {
  return FieldValue{new FieldValueInternal(value)};
}

/* static */
FieldValue FieldValue::Double(double value) {
  return FieldValue{new FieldValueInternal(value)};
}

/* static */
FieldValue FieldValue::Timestamp(class Timestamp value) {
  return FieldValue{new FieldValueInternal(value)};
}

/* static */
FieldValue FieldValue::String(std::string value) {
  return FieldValue{new FieldValueInternal(firebase::Move(value))};
}

/* static */
FieldValue FieldValue::Blob(const uint8_t* value, size_t size) {
  return FieldValue{new FieldValueInternal(value, size)};
}

/* static */
FieldValue FieldValue::Reference(DocumentReference value) {
  return FieldValue{new FieldValueInternal(firebase::Move(value))};
}

/* static */
FieldValue FieldValue::GeoPoint(class GeoPoint value) {
  return FieldValue{new FieldValueInternal(value)};
}

/* static */
FieldValue FieldValue::Array(std::vector<FieldValue> value) {
  return FieldValue{new FieldValueInternal(firebase::Move(value))};
}

/* static */
FieldValue FieldValue::Map(MapFieldValue value) {
  return FieldValue{new FieldValueInternal(firebase::Move(value))};
}

Type FieldValue::type() const {
  if (!internal_) return {};
  return internal_->type();
}

bool FieldValue::boolean_value() const {
  if (!internal_) return {};
  return internal_->boolean_value();
}

int64_t FieldValue::integer_value() const {
  if (!internal_) return {};
  return internal_->integer_value();
}

double FieldValue::double_value() const {
  if (!internal_) return {};
  return internal_->double_value();
}

Timestamp FieldValue::timestamp_value() const {
  if (!internal_) return {};
  return internal_->timestamp_value();
}

std::string FieldValue::string_value() const {
  if (!internal_) return "";
  return internal_->string_value();
}

const uint8_t* FieldValue::blob_value() const {
  if (!internal_) return {};
  return internal_->blob_value();
}

size_t FieldValue::blob_size() const {
  if (!internal_) return {};
  return internal_->blob_size();
}

DocumentReference FieldValue::reference_value() const {
  if (!internal_) return {};
  return internal_->reference_value();
}

GeoPoint FieldValue::geo_point_value() const {
  if (!internal_) return {};
  return internal_->geo_point_value();
}

std::vector<FieldValue> FieldValue::array_value() const {
  if (!internal_) return std::vector<FieldValue>{};
  return internal_->array_value();
}

MapFieldValue FieldValue::map_value() const {
  if (!internal_) return MapFieldValue{};
  return internal_->map_value();
}

/* static */
FieldValue FieldValue::Null() {
  FieldValue result;
  result.internal_ = new FieldValueInternal();
  return result;
}

/* static */
FieldValue FieldValue::Delete() { return FieldValueInternal::Delete(); }

/* static */
FieldValue FieldValue::ServerTimestamp() {
  return FieldValueInternal::ServerTimestamp();
}

/* static */
FieldValue FieldValue::ArrayUnion(std::vector<FieldValue> elements) {
  return FieldValueInternal::ArrayUnion(firebase::Move(elements));
}

/* static */
FieldValue FieldValue::ArrayRemove(std::vector<FieldValue> elements) {
  return FieldValueInternal::ArrayRemove(firebase::Move(elements));
}

/* static */
FieldValue FieldValue::IntegerIncrement(int64_t by_value) {
  return FieldValueInternal::IntegerIncrement(by_value);
}

/* static */
FieldValue FieldValue::DoubleIncrement(double by_value) {
  return FieldValueInternal::DoubleIncrement(by_value);
}

// TODO(varconst): consider reversing the role of the two output functions, so
// that `ToString` delegates to `operator<<`, not the other way round.
std::string FieldValue::ToString() const {
  if (!is_valid()) return "<invalid>";

  switch (type()) {
    case Type::kNull:
      return "null";

    case Type::kBoolean:
      return boolean_value() ? "true" : "false";

    case Type::kInteger:
      return ValueToString(integer_value());

    case Type::kDouble:
      return ValueToString(double_value());

    case Type::kTimestamp:
      return timestamp_value().ToString();

    case Type::kString:
      return std::string("'") + string_value() + "'";

    case Type::kBlob:
      return ValueToString(blob_value(), blob_size());

    case Type::kReference:
      return reference_value().ToString();

    case Type::kGeoPoint:
      return geo_point_value().ToString();

    case Type::kArray:
      return ValueToString(array_value());

    case Type::kMap:
      return ::firebase::firestore::ToString(map_value());

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
  }

  FIREBASE_ASSERT_MESSAGE_RETURN("<invalid>", false,
                                 "Unexpected FieldValue type: %d",
                                 static_cast<int>(type()));
}

bool operator==(const FieldValue& lhs, const FieldValue& rhs) {
  return lhs.internal_ == rhs.internal_ ||
         (lhs.internal_ != nullptr && rhs.internal_ != nullptr &&
          *lhs.internal_ == *rhs.internal_);
}

std::ostream& operator<<(std::ostream& out, const FieldValue& value) {
  return out << value.ToString();
}

}  // namespace firestore
}  // namespace firebase
