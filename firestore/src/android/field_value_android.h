// Copyright 2020 Google LLC

#ifndef FIREBASE_FIRESTORE_SRC_ANDROID_FIELD_VALUE_ANDROID_H_
#define FIREBASE_FIRESTORE_SRC_ANDROID_FIELD_VALUE_ANDROID_H_

#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "firebase/firestore/geo_point.h"
#include "firebase/firestore/timestamp.h"
#include "firestore/src/include/firebase/firestore/document_reference.h"
#include "firestore/src/include/firebase/firestore/field_value.h"
#include "firestore/src/jni/arena_ref.h"
#include "firestore/src/jni/jni_fwd.h"
#include "firestore/src/jni/object.h"
#include "firestore/src/jni/ownership.h"

namespace firebase::firestore {

class FirestoreInternal;

class FieldValueInternal {
 public:
  using Type = FieldValue::Type;

  static void Initialize(jni::Loader& loader);
  static FieldValue Create(jni::Env& env, const jni::Object& object);
  static FieldValue Create(jni::Env& env, Type type, const jni::Object& object);

  FieldValueInternal();
  FieldValueInternal(const FieldValueInternal& other);
  FieldValueInternal(FieldValueInternal&& other) noexcept;
  FieldValueInternal& operator=(const FieldValueInternal& other);
  FieldValueInternal& operator=(FieldValueInternal&& other) noexcept;

  explicit FieldValueInternal(const jni::Object& object);
  FieldValueInternal(Type type, const jni::Object& object);

  explicit FieldValueInternal(bool value);
  explicit FieldValueInternal(int64_t value);
  explicit FieldValueInternal(double value);
  explicit FieldValueInternal(Timestamp value);
  explicit FieldValueInternal(std::string value);
  FieldValueInternal(const uint8_t* value, size_t size);
  explicit FieldValueInternal(DocumentReference value);
  explicit FieldValueInternal(GeoPoint value);
  explicit FieldValueInternal(const std::vector<FieldValue>& value);
  explicit FieldValueInternal(const MapFieldValue& value);

  Type type() const;

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

  jni::Local<jni::Object> ToJava() const;

  static FieldValue Delete();
  static FieldValue ServerTimestamp();
  static FieldValue ArrayUnion(std::vector<FieldValue> elements);
  static FieldValue ArrayRemove(std::vector<FieldValue> elements);
  static FieldValue IntegerIncrement(int64_t by_value);
  static FieldValue DoubleIncrement(double by_value);

  static jni::Local<jni::Object> ToJava(const FieldValue& value);
  static jni::Local<jni::Array<jni::Object>> MakeArray(
      jni::Env& env, const std::vector<FieldValue>& elements);

 private:
  friend class FirestoreInternal;
  friend bool operator==(const FieldValueInternal& lhs,
                         const FieldValueInternal& rhs);

  template <typename T>
  jni::Local<T> Cast(jni::Env& env, Type type) const;

  void EnsureCachedBlob(jni::Env& env) const;
  static jni::Env GetEnv();

  jni::ArenaRef object_;
  mutable std::mutex mutex_;
  mutable std::optional<Type> cached_type_;
  mutable std::shared_ptr<std::vector<uint8_t>> cached_blob_;
};

}  // namespace firebase::firestore

#endif  // FIREBASE_FIRESTORE_SRC_ANDROID_FIELD_VALUE_ANDROID_H_
