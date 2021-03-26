#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_IOS_FIELD_VALUE_IOS_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_IOS_FIELD_VALUE_IOS_H_

#include <cstdint>
#include <string>
#include <vector>

#include "app/src/assert.h"
#include "firestore/src/include/firebase/firestore/document_reference.h"
#include "firestore/src/include/firebase/firestore/field_value.h"
#include "firestore/src/ios/firestore_ios.h"
#include "absl/types/variant.h"
#include "firebase/firestore/geo_point.h"
#include "firebase/firestore/timestamp.h"
#include "Firestore/core/src/model/field_value.h"

namespace firebase {
namespace firestore {

class FieldValueInternal {
 public:
  FieldValueInternal() = default;

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

  FieldValue::Type type_ = FieldValue::Type::kNull;
  // Note: it's impossible to roundtrip between a `DocumentReference` and
  // `model::FieldValue::reference_value`, because the latter omits some
  // information from the former (`shared_ptr` to the Firestore instance). For
  // that reason, just store the `DocumentReference` directly in the `variant`.
  absl::variant<model::FieldValue, DocumentReference, ArrayT, MapT> value_;
};

bool operator==(const FieldValueInternal& lhs, const FieldValueInternal& rhs);

std::string Describe(FieldValue::Type type);

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_IOS_FIELD_VALUE_IOS_H_
