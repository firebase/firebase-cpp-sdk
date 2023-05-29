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

#ifndef FIREBASE_FIRESTORE_SRC_INCLUDE_FIREBASE_FIRESTORE_FILTER_H_
#define FIREBASE_FIRESTORE_SRC_INCLUDE_FIREBASE_FIRESTORE_FILTER_H_

#include <vector>

#include "firebase/firestore/field_value.h"

namespace firebase {
namespace firestore {

class FilterInternal;

class Filter {
  public:
    static Filter ArrayContains(const std::string &field, const FieldValue & value);
    static Filter ArrayContainsAny(const std::string &field, const std::vector<FieldValue> &values);
    static Filter EqualTo(const std::string &field, const FieldValue &value);
    static Filter NotEqualTo(const std::string &field, const FieldValue &value);
    static Filter GreaterThan(const std::string &field, const FieldValue &value);
    static Filter GreaterThanOrEqualTo(const std::string &field, const FieldValue &value);
    static Filter LessThan(const std::string &field, const FieldValue &value);
    static Filter LessThanOrEqualTo(const std::string &field, const FieldValue &value);
    static Filter In(const std::string &field, const std::vector<FieldValue> &values);
    static Filter NotIn(const std::string &field, const std::vector<FieldValue> &values);

    static Filter ArrayContains(const FieldPath &field, const FieldValue &value);
    static Filter ArrayContainsAny(const FieldPath &field, const std::vector<FieldValue> &values);
    static Filter EqualTo(const FieldPath &field, const FieldValue &value);
    static Filter NotEqualTo(const FieldPath &field, const FieldValue &value);
    static Filter GreaterThan(const FieldPath &field, const FieldValue &value);
    static Filter GreaterThanOrEqualTo(const FieldPath &field, const FieldValue &value);
    static Filter LessThan(const FieldPath &field, const FieldValue &value);
    static Filter LessThanOrEqualTo(const FieldPath &field, const FieldValue &value);
    static Filter In(const FieldPath &field, const std::vector<FieldValue> &values);
    static Filter NotIn(const FieldPath &field, const std::vector<FieldValue> &values);

    template <class ... Filters>
    static Filter Or(const Filter& filter, const Filters & ... filters);

    template <typename ... Filters>
    static Filter And(const Filter & filter, const Filters & ... filters);

    Filter(const Filter& other);
    Filter(Filter&& other) noexcept;
    Filter& operator=(const Filter& other);
    Filter& operator=(Filter&& other) noexcept;

    ~Filter();

  private:
    friend bool operator==(const Filter& lhs, const Filter& rhs);
    friend struct ConverterImpl;

    explicit Filter(FilterInternal* internal);
    FilterInternal* internal_;
};

/** Checks `lhs` and `rhs` for equality. */
bool operator==(const Filter& lhs, const Filter& rhs);

/** Checks `lhs` and `rhs` for inequality. */
inline bool operator!=(const Filter& lhs, const Filter& rhs) {
  return !(lhs == rhs);
}

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_SRC_INCLUDE_FIREBASE_FIRESTORE_FILTER_H_
