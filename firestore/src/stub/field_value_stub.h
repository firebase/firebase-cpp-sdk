#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_STUB_FIELD_VALUE_STUB_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_STUB_FIELD_VALUE_STUB_H_

#include <cstdint>
#include <string>

#include "firestore/src/include/firebase/firestore/document_reference.h"
#include "firestore/src/include/firebase/firestore/field_value.h"
#include "firestore/src/stub/firestore_stub.h"
#include "firebase/firestore/geo_point.h"
#include "firebase/firestore/timestamp.h"

namespace firebase {
namespace firestore {

// This is the stub implementation of FieldValue.
class FieldValueInternal {
 public:
  using ApiType = FieldValue;
  FieldValueInternal() {}
  explicit FieldValueInternal(bool value) {}
  explicit FieldValueInternal(int64_t value) {}
  explicit FieldValueInternal(double value) {}
  // NOLINTNEXTLINE (performance-unnecessary-value-param)
  explicit FieldValueInternal(Timestamp value) {}
  // NOLINTNEXTLINE (performance-unnecessary-value-param)
  explicit FieldValueInternal(std::string value) {}
  FieldValueInternal(const uint8_t* value, size_t size) {}
  // NOLINTNEXTLINE (performance-unnecessary-value-param)
  explicit FieldValueInternal(DocumentReference value) {}
  // NOLINTNEXTLINE (performance-unnecessary-value-param)
  explicit FieldValueInternal(GeoPoint value) {}
  // NOLINTNEXTLINE (performance-unnecessary-value-param)
  explicit FieldValueInternal(std::vector<FieldValue> value) {}
  // NOLINTNEXTLINE (performance-unnecessary-value-param)
  explicit FieldValueInternal(MapFieldValue value) {}
  FirestoreInternal* firestore_internal() const { return nullptr; }
  FieldValue::Type type() const { return FieldValue::Type::kNull; }

  // The stub implemantion of _value() methods just return default values. We
  // could FIREBASE_ASSERT(false), since technically the caller shouldn't call
  // these when the type() is kNull, but for no-op desktop support, it's
  // probably more helpful to return default values so if the developer is
  // assuming some schema for their data, it behaves better.
  bool boolean_value() const {
    return false;
  }
  int64_t integer_value() const {
    return 0;
  }
  double double_value() const { return 0; }
  Timestamp timestamp_value() const {
    return Timestamp{};
  }
  std::string string_value() const {
    return "";
  }
  const uint8_t* blob_value() const {
    return nullptr;
  }
  size_t blob_size() const {
    return 0;
  }
  DocumentReference reference_value() const {
    return DocumentReference{};
  }
  GeoPoint geo_point_value() const {
    return GeoPoint{};
  }
  std::vector<FieldValue> array_value() const {
    return {};
  }
  MapFieldValue map_value() const {
    return MapFieldValue{};
  }
  static FieldValue Delete() {
    return FieldValue{};
  }
  static FieldValue ServerTimestamp() {
    return FieldValue{};
  }
  // NOLINTNEXTLINE (performance-unnecessary-value-param)
  static FieldValue ArrayUnion(std::vector<FieldValue> elements) {
    return FieldValue{};
  }
  // NOLINTNEXTLINE (performance-unnecessary-value-param)
  static FieldValue ArrayRemove(std::vector<FieldValue> elements) {
    return FieldValue{};
  }
  static FieldValue IntegerIncrement(std::int64_t) { return FieldValue{}; }
  static FieldValue DoubleIncrement(double) { return FieldValue{}; }
};

inline bool operator==(const FieldValueInternal& lhs,
                       const FieldValueInternal& rhs) {
  return false;
}

}  // namespace firestore
}  // namespace firebase
#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_STUB_FIELD_VALUE_STUB_H_
