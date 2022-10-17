/*
 * Copyright 2020 Google LLC
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

#ifndef FIREBASE_FIRESTORE_SRC_ANDROID_FIELD_VALUE_ANDROID_H_
#define FIREBASE_FIRESTORE_SRC_ANDROID_FIELD_VALUE_ANDROID_H_

#include <cstdint>
#include <memory>
#include <string>

#include "firebase/firestore/geo_point.h"
#include "firebase/firestore/timestamp.h"
#include "firestore/src/include/firebase/firestore/document_reference.h"
#include "firestore/src/include/firebase/firestore/field_value.h"
#include "firestore/src/jni/jni_fwd.h"
#include "firestore/src/jni/object.h"
#include "firestore/src/jni/ownership.h"

namespace firebase {
namespace firestore {

class FirestoreInternal;

class FieldValueInternal {
 public:
  using Type = FieldValue::Type;

  static void Initialize(jni::Loader& loader);

  static FieldValue Create(jni::Env& env, const jni::Object& object);
  static FieldValue Create(jni::Env& env, Type type, const jni::Object& object);

  FieldValueInternal();

  explicit FieldValueInternal(const jni::Object& object);
  FieldValueInternal(Type type, const jni::Object& object);

  // Constructs a FieldValueInternal from a value of a specific type. Note that
  // these constructors must match the equivalents in field_value_main.h.
  //
  // Of particular note is that these pass by value even though the Android
  // implementation does not retain the values. This is done to make the
  // interface consistent with the iOS version, which does move. Making these
  // value types instead of const references saves a copy in that
  // implementation.
  explicit FieldValueInternal(bool value);
  explicit FieldValueInternal(int64_t value);
  explicit FieldValueInternal(double value);
  explicit FieldValueInternal(Timestamp value);
  explicit FieldValueInternal(std::string value);
  FieldValueInternal(const uint8_t* value, size_t size);
  explicit FieldValueInternal(DocumentReference value);
  explicit FieldValueInternal(GeoPoint value);
  // Deviate from the iOS signatures of the following two constructors. The iOS
  // versions take values into which the caller moves, to elide a copy. In
  // Android, this actually *costs* an extra copy when calling from
  // `DocumentReferenceInternal::Set()`, doubling the number of global
  // references needed. Using const references in Android avoids this extra,
  // costly copy (https://github.com/firebase/quickstart-unity/issues/1303).
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

  const jni::Global<jni::Object>& ToJava() const { return object_; }

  static FieldValue Delete();
  static FieldValue ServerTimestamp();
  static FieldValue ArrayUnion(std::vector<FieldValue> elements);
  static FieldValue ArrayRemove(std::vector<FieldValue> elements);
  static FieldValue IntegerIncrement(int64_t by_value);
  static FieldValue DoubleIncrement(double by_value);

  static jni::Object ToJava(const FieldValue& value);

 private:
  friend class FirestoreInternal;
  friend bool operator==(const FieldValueInternal& lhs,
                         const FieldValueInternal& rhs);

  // Casts the internal Java Object reference to the given Java proxy type.
  // This performs a run-time `instanceof` check to verify that the object
  // has the type `T::GetClass()`.
  template <typename T>
  T Cast(jni::Env& env, Type type) const;

  static jni::Local<jni::Array<jni::Object>> MakeArray(
      jni::Env& env, const std::vector<FieldValue>& elements);

  void EnsureCachedBlob(jni::Env& env) const;

  static jni::Env GetEnv();

  jni::Global<jni::Object> object_;

  // Below are cached type information. It is costly to get type info from
  // jobject of Object type. So we cache it if we have already known.
  mutable Type cached_type_ = Type::kNull;
  mutable std::shared_ptr<std::vector<uint8_t>> cached_blob_;
};

inline jni::Object ToJava(const FieldValue& value) {
  // This indirection is required to make use of the `friend` in FieldValue.
  return FieldValueInternal::ToJava(value);
}

inline jobject ToJni(const FieldValueInternal* value) {
  return value->ToJava().get();
}

inline jobject ToJni(const FieldValueInternal& value) {
  return value.ToJava().get();
}

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_SRC_ANDROID_FIELD_VALUE_ANDROID_H_
