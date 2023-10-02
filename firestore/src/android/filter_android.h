/*
 * Copyright 2023 Google LLC
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

#ifndef FIREBASE_FIRESTORE_SRC_ANDROID_FILTER_ANDROID_H_
#define FIREBASE_FIRESTORE_SRC_ANDROID_FILTER_ANDROID_H_

#include <vector>

#include "firestore/src/android/wrapper.h"
#include "firestore/src/include/firebase/firestore/field_path.h"
#include "firestore/src/include/firebase/firestore/field_value.h"
#include "firestore/src/include/firebase/firestore/filter.h"

namespace firebase {
namespace firestore {

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

  FilterInternal* clone() { return new FilterInternal(*this); }

  bool IsEmpty() const { return is_empty_; }

  static jni::Env GetEnv();

  jni::ArenaRef obj_;
  const bool is_empty_;

  // A generalized function for all WhereFoo calls.
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

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_SRC_ANDROID_FILTER_ANDROID_H_
