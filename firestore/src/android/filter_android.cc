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

#include <utility>

#include "firestore/src/android/filter_android.h"

#include "firestore/src/android/converter_android.h"
#include "firestore/src/android/firestore_android.h"
#include "firestore/src/android/field_path_android.h"
#include "firestore/src/android/field_value_android.h"

#include "firestore/src/jni/array.h"
#include "firestore/src/jni/array_list.h"
#include "firestore/src/jni/compare.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/loader.h"

namespace firebase {
namespace firestore {
namespace {

using jni::Array;
using jni::ArrayList;
using jni::Env;
using jni::Local;
using jni::StaticMethod;
using jni::Object;

constexpr char kClassName[] =
    PROGUARD_KEEP_CLASS "com/google/firebase/firestore/Filter";
StaticMethod<Object> kEqualTo(
    "equalTo",
    "(Lcom/google/firebase/firestore/FieldPath;Ljava/lang/Object;)"
    "Lcom/google/firebase/firestore/Filter;");
StaticMethod<Object> kNotEqualTo(
    "notEqualTo",
    "(Lcom/google/firebase/firestore/FieldPath;Ljava/lang/Object;)"
    "Lcom/google/firebase/firestore/Filter;");
StaticMethod<Object> kLessThan(
    "lessThan",
    "(Lcom/google/firebase/firestore/FieldPath;Ljava/lang/Object;)"
    "Lcom/google/firebase/firestore/Filter;");
StaticMethod<Object> kLessThanOrEqualTo(
    "lessThanOrEqualTo",
    "(Lcom/google/firebase/firestore/FieldPath;Ljava/lang/Object;)"
    "Lcom/google/firebase/firestore/Filter;");
StaticMethod<Object> kGreaterThan(
    "greaterThan",
    "(Lcom/google/firebase/firestore/FieldPath;Ljava/lang/Object;)"
    "Lcom/google/firebase/firestore/Filter;");
StaticMethod<Object> kGreaterThanOrEqualTo(
    "greaterThanOrEqualTo",
    "(Lcom/google/firebase/firestore/FieldPath;Ljava/lang/Object;)"
    "Lcom/google/firebase/firestore/Filter;");
StaticMethod<Object> kArrayContains(
    "arrayContains",
    "(Lcom/google/firebase/firestore/FieldPath;Ljava/lang/Object;)"
    "Lcom/google/firebase/firestore/Filter;");
StaticMethod<Object> kArrayContainsAny(
    "arrayContainsAny",
    "(Lcom/google/firebase/firestore/FieldPath;Ljava/util/List;)"
    "Lcom/google/firebase/firestore/Filter;");
StaticMethod<Object> kIn("in",
                   "(Lcom/google/firebase/firestore/FieldPath;Ljava/util/List;)"
                   "Lcom/google/firebase/firestore/Filter;");
StaticMethod<Object> kNotIn(
    "notIn",
    "(Lcom/google/firebase/firestore/FieldPath;Ljava/util/List;)"
    "Lcom/google/firebase/firestore/Filter;");
StaticMethod<Object> kAnd(
    "and",
    "([Lcom/google/firebase/firestore/Filter;)"
    "Lcom/google/firebase/firestore/Filter;");
StaticMethod<Object> kOr(
    "or",
    "([Lcom/google/firebase/firestore/Filter;)"
    "Lcom/google/firebase/firestore/Filter;");
}  // namespace

void FilterInternal::Initialize(jni::Loader& loader) {
  loader.LoadClass(
      kClassName, kEqualTo, kNotEqualTo, kLessThan, kLessThanOrEqualTo,
      kGreaterThan, kGreaterThanOrEqualTo, kArrayContains, kArrayContainsAny,
      kIn, kNotIn, kAnd, kOr);
}

FilterInternal::FilterInternal(jni::Object&& object, bool is_empty)
    : object_(object), is_empty_(is_empty) {}

Filter FilterInternal::EqualTo(const FieldPath& field,
                                  const FieldValue& value) {
  return Where(field, kEqualTo, value);
}

Filter FilterInternal::NotEqualTo(const FieldPath& field,
                                     const FieldValue& value) {
  return Where(field, kNotEqualTo, value);
}

Filter FilterInternal::LessThan(const FieldPath& field,
                                   const FieldValue& value) {
  return Where(field, kLessThan, value);
}

Filter FilterInternal::LessThanOrEqualTo(const FieldPath& field,
                                            const FieldValue& value) {
  return Where(field, kLessThanOrEqualTo, value);
}

Filter FilterInternal::GreaterThan(const FieldPath& field,
                                      const FieldValue& value) {
  return Where(field, kGreaterThan, value);
}

Filter FilterInternal::GreaterThanOrEqualTo(const FieldPath& field,
                                               const FieldValue& value) {
  return Where(field, kGreaterThanOrEqualTo, value);
}

Filter FilterInternal::ArrayContains(const FieldPath& field,
                                        const FieldValue& value) {
  return Where(field, kArrayContains, value);
}

Filter FilterInternal::ArrayContainsAny(
    const FieldPath& field, const std::vector<FieldValue>& values) {
  return Where(field, kArrayContainsAny, values);
}

Filter FilterInternal::In(const FieldPath& field,
                             const std::vector<FieldValue>& values) {
  return Where(field, kIn, values);
}

Filter FilterInternal::NotIn(const FieldPath& field,
                                const std::vector<FieldValue>& values) {
  return Where(field, kNotIn, values);
}

Filter FilterInternal::And(const std::vector<const Filter>& filters) {
  return Where(kAnd, filters);
}

Filter FilterInternal::Or(const std::vector<const Filter>& filters) {
  return Where(kOr, filters);
}

Env FilterInternal::GetEnv() { return FirestoreInternal::GetEnv(); }

Filter FilterInternal::Where(const FieldPath& field,
                             const StaticMethod<Object>& method,
                             const FieldValue& value) {
  Env env = GetEnv();
  Local<Object> java_field = FieldPathConverter::Create(env, field);
  Object filter = env.Call(method, java_field, FieldValueInternal::ToJava(value));
  return Filter(new FilterInternal(std::move(filter), false));
}

Filter FilterInternal::Where(const FieldPath& field,
                             const jni::StaticMethod<Object>& method,
                             const std::vector<FieldValue>& values) {
  Env env = GetEnv();
  size_t size = values.size();
  Local<ArrayList> java_values = ArrayList::Create(env, size);
  for (size_t i = 0; i < size; ++i) {
    java_values.Add(env, FieldValueInternal::ToJava(values[i]));
  }

  Local<Object> java_field = FieldPathConverter::Create(env, field);
  Object filter = env.Call(method, java_field, java_values);
  return Filter(new FilterInternal(std::move(filter), false));
}

Filter FilterInternal::Where(const StaticMethod<Object>& method,
                             const std::vector<const Filter>& filters) {
  Env env = GetEnv();
  size_t size = filters.size();
  Local<Array<Object>> java_filters = env.NewArray(size, Object::GetClass());
  bool is_empty = true;
  for (int i = 0; i < size; ++i) {
    FilterInternal* internal_filter = filters[i].internal_;
    if (!internal_filter->IsEmpty()) {
      is_empty = false;
    }
    java_filters.Set(env, i, internal_filter->object_);
  }
  Object filter = env.Call(method, java_filters);
  return Filter(new FilterInternal(std::move(filter), is_empty));
}

bool operator==(const FilterInternal& lhs, const FilterInternal& rhs) {
  return jni::EqualityCompareJni(lhs, rhs);
}

}  // namespace firestore
}  // namespace firebase
