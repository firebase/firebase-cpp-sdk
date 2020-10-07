#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_FIELD_VALUE_ANDROID_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_FIELD_VALUE_ANDROID_H_

#include <cstdint>
#include <string>

#include "app/memory/shared_ptr.h"
#include "app/src/assert.h"
#include "firestore/src/android/wrapper.h"
#include "firestore/src/include/firebase/firestore/document_reference.h"
#include "firestore/src/include/firebase/firestore/field_value.h"
#include "firestore/src/jni/jni_fwd.h"
#include "firebase/firestore/geo_point.h"
#include "firebase/firestore/timestamp.h"

namespace firebase {
namespace firestore {

class FieldValueInternal : public Wrapper {
 public:
  // Wrapper does not allow passing in Java Null object. So we need to deal that
  // case separately. Generally Java Null is not a valid instance of any
  // Firestore types except here Null may represent a valid FieldValue of Null
  // type.
  FieldValueInternal(FirestoreInternal* firestore, jobject obj);
  FieldValueInternal(FirestoreInternal* firestore, const jni::Object& obj);
  FieldValueInternal(const FieldValueInternal& wrapper);
  FieldValueInternal(FieldValueInternal&& wrapper);

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

  FieldValue::Type type() const;

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

  static FieldValue Delete();
  static FieldValue ServerTimestamp();
  static FieldValue ArrayUnion(std::vector<FieldValue> elements);
  static FieldValue ArrayRemove(std::vector<FieldValue> elements);
  static FieldValue IntegerIncrement(int64_t by_value);
  static FieldValue DoubleIncrement(double by_value);

 private:
  friend class FirestoreInternal;
  friend bool operator==(const FieldValueInternal& lhs,
                         const FieldValueInternal& rhs);

  static bool Initialize(App* app);
  static void Terminate(App* app);

  void EnsureCachedBlob(jni::Env& env) const;

  static jobject TryGetJobject(const FieldValue& value);

  static jobject delete_;
  static jobject server_timestamp_;

  // Below are cached type information. It is costly to get type info from
  // jobject of Object type. So we cache it if we have already known.
  mutable FieldValue::Type cached_type_ = FieldValue::Type::kNull;
  mutable SharedPtr<std::vector<uint8_t>> cached_blob_;
};

bool operator==(const FieldValueInternal& lhs, const FieldValueInternal& rhs);

inline jobject ToJni(const FieldValueInternal* value) {
  return value->java_object();
}

inline jobject ToJni(const FieldValueInternal& value) {
  return value.java_object();
}

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_FIELD_VALUE_ANDROID_H_
