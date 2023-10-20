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
#include "firestore/src/android/field_path_android.h"
#include "firestore/src/android/field_value_android.h"
#include "firestore/src/android/firestore_android.h"

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
using jni::Object;
using jni::StaticMethod;

jclass filter_class = nullptr;
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
StaticMethod<Object> kIn(
    "inArray",
    "(Lcom/google/firebase/firestore/FieldPath;Ljava/util/List;)"
    "Lcom/google/firebase/firestore/Filter;");
StaticMethod<Object> kNotIn(
    "notInArray",
    "(Lcom/google/firebase/firestore/FieldPath;Ljava/util/List;)"
    "Lcom/google/firebase/firestore/Filter;");
StaticMethod<Object> kAnd("and",
                          "([Lcom/google/firebase/firestore/Filter;)"
                          "Lcom/google/firebase/firestore/Filter;");
StaticMethod<Object> kOr("or",
                         "([Lcom/google/firebase/firestore/Filter;)"
                         "Lcom/google/firebase/firestore/Filter;");
}  // namespace

void FilterInternal::Initialize(jni::Loader& loader) {
  filter_class = loader.LoadClass(kClassName);
  loader.LoadClass(kClassName, kEqualTo, kNotEqualTo, kLessThan,
                   kLessThanOrEqualTo, kGreaterThan, kGreaterThanOrEqualTo,
                   kArrayContains, kArrayContainsAny, kIn, kNotIn, kAnd, kOr);
}

FilterInternal::FilterInternal(const jni::Object& obj, bool is_empty)
    : is_empty_(is_empty) {
  Env env = GetEnv();
  obj_.reset(env, obj);
}

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

Filter FilterInternal::ArrayContainsAny(const FieldPath& field,
                                        const std::vector<FieldValue>& values) {
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

Filter FilterInternal::And(const std::vector<Filter>& filters) {
  return Where(kAnd, filters);
}

Filter FilterInternal::Or(const std::vector<Filter>& filters) {
  return Where(kOr, filters);
}

Env FilterInternal::GetEnv() { return FirestoreInternal::GetEnv(); }

Filter FilterInternal::Where(const FieldPath& field,
                             const StaticMethod<Object>& method,
                             const FieldValue& value) {
  Env env = GetEnv();
  Local<Object> java_field = FieldPathConverter::Create(env, field);
  Local<Object> filter =
      env.Call(method, java_field, FieldValueInternal::ToJava(value));
  return Filter(new FilterInternal(filter, false));
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
  Local<Object> filter = env.Call(method, java_field, java_values);
  return Filter(new FilterInternal(filter, false));
}

Filter FilterInternal::Where(const StaticMethod<Object>& method,
                             const std::vector<Filter>& filters) {
  Env env = GetEnv();
  std::vector<int> non_empty_indexes;
  size_t filters_size = filters.size();
  for (int i = 0; i < filters_size; ++i) {
    if (!filters[i].internal_->IsEmpty()) {
      non_empty_indexes.push_back(i);
    }
  }
  size_t non_empty_size = non_empty_indexes.size();
  Local<Array<Object>> java_filters =
      env.NewArray(non_empty_size, filter_class);
  for (int i = 0; i < non_empty_size; ++i) {
    java_filters.Set(env, i, filters[non_empty_indexes[i]].internal_->ToJava());
  }
  Local<Object> filter = env.Call(method, java_filters);
  return Filter(new FilterInternal(filter, non_empty_size == 0));
}

Local<Object> FilterInternal::ToJava() const {
  Env env = GetEnv();
  return obj_.get(env);
}

bool operator==(const FilterInternal& lhs, const FilterInternal& rhs) {
  return jni::EqualityCompareJni(lhs, rhs);
}

}  // namespace firestore
}  // namespace firebase
