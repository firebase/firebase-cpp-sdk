// Copyright 2023 Google LLC

#ifndef FIREBASE_FIRESTORE_SRC_ANDROID_FILTER_ANDROID_H_
#define FIREBASE_FIRESTORE_SRC_ANDROID_FILTER_ANDROID_H_

#include <vector>

#include "firestore/src/include/firebase/firestore/field_path.h"
#include "firestore/src/include/firebase/firestore/field_value.h"
#include "firestore/src/include/firebase/firestore/filter.h"
#include "firestore/src/jni/arena_ref.h"
#include "firestore/src/jni/object.h"

namespace firebase::firestore {

class FilterInternal final {
 public:
  static void Initialize(jni::Loader& loader);

  FilterInternal(const jni::Object& object, bool is_empty);

  static Filter ArrayContains(const FieldPath& field, const FieldValue& value);
  static Filter ArrayContainsAny(const FieldPath& field,
                                 const std::vector<FieldValue>& values);
  static Filter EqualTo(const FieldPath& field, const FieldValue& value);
  static Filter NotEqualTo(const FieldPath& field, const FieldValue& value);
  static Filter GreaterThan(const FieldPath& field, const FieldValue& value);
  static Filter GreaterThanOrEqualTo(const FieldPath& field,
                                     const FieldValue& value);
  static Filter LessThan(const FieldPath& field, const FieldValue& value);
  static Filter LessThanOrEqualTo(const FieldPath& field,
                                  const FieldValue& value);
  static Filter In(const FieldPath& field,
                   const std::vector<FieldValue>& values);
  static Filter NotIn(const FieldPath& field,
                      const std::vector<FieldValue>& values);
  static Filter Or(const std::vector<Filter>& filters);
  static Filter And(const std::vector<Filter>& filters);

  jni::Local<jni::Object> ToJava() const;

 private:
  friend class Filter;
  friend class FirestoreInternal;
  friend class QueryInternal;

  FilterInternal* clone() const { return new FilterInternal(*this); }
  bool IsEmpty() const { return is_empty_; }
  static jni::Env GetEnv();

  jni::ArenaRef obj_;
  const bool is_empty_;

  static Filter Where(const FieldPath& field,
                      const jni::StaticMethod<jni::Object>& method,
                      const FieldValue& value);
  static Filter Where(const FieldPath& field,
                      const jni::StaticMethod<jni::Object>& method,
                      const std::vector<FieldValue>& values);
  static Filter Where(const jni::StaticMethod<jni::Object>& method,
                      const std::vector<Filter>& filters);
};

bool operator==(const FilterInternal& lhs, const FilterInternal& rhs);
inline bool operator!=(const FilterInternal& lhs, const FilterInternal& rhs) {
  return !(lhs == rhs);
}

}  // namespace firebase::firestore

#endif  // FIREBASE_FIRESTORE_SRC_ANDROID_FILTER_ANDROID_H_
