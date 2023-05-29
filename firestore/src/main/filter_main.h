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

#ifndef FIREBASE_FIRESTORE_SRC_MAIN_FILTER_MAIN_H_
#define FIREBASE_FIRESTORE_SRC_MAIN_FILTER_MAIN_H_

#if defined(__ANDROID__)
#error "This header should not be used on Android."
#endif

#include "Firestore/core/src/core/composite_filter.h"
#include "Firestore/core/src/core/filter.h"
#include "Firestore/core/src/core/field_filter.h"
#include "firestore/src/include/firebase/firestore/filter.h"
#include "firestore/src/main/user_data_converter_main.h"

namespace firebase {
namespace firestore {

class FilterInternal {
 public:

  static Filter ArrayContains(const FieldPath &field, const FieldValue &value) {
    return UnaryFilter(field, UnaryOperator::ArrayContains, value);
  }

  static Filter ArrayContainsAny(const FieldPath &field, const std::vector<FieldValue> &values) {
    return UnaryFilter(field, UnaryOperator::ArrayContainsAny, values);
  }

  static Filter EqualTo(const FieldPath &field, const FieldValue &value) {
    return UnaryFilter(field, UnaryOperator::Equal, value);
  }

  static Filter NotEqualTo(const FieldPath &field, const FieldValue &value) {
    return UnaryFilter(field, UnaryOperator::NotEqual, value);
  }

  static Filter GreaterThan(const FieldPath &field, const FieldValue &value) {
    return UnaryFilter(field, UnaryOperator::GreaterThan, value);
  }

  static Filter GreaterThanOrEqualTo(const FieldPath &field, const FieldValue &value) {
    return UnaryFilter(field, UnaryOperator::GreaterThanOrEqual, value);
  }

  static Filter LessThan(const FieldPath &field, const FieldValue &value) {
    return UnaryFilter(field, UnaryOperator::LessThan, value);
  }

  static Filter LessThanOrEqualTo(const FieldPath &field, const FieldValue &value) {
    return UnaryFilter(field, UnaryOperator::LessThanOrEqual, value);
  }

  static Filter In(const FieldPath &field, const std::vector<FieldValue> &values) {
    return UnaryFilter(field, UnaryOperator::In, values);
  }

  static Filter NotIn(const FieldPath &field, const std::vector<FieldValue> &values) {
    return UnaryFilter(field, UnaryOperator::NotIn, values);
  }

  virtual core::Filter filter_core(const api::Query& query, const UserDataConverter& user_data_converter) const = 0;

  template <typename ... Filters>
  static Filter Or(const Filter& filter, const Filters& ... filters) {
    return CompositeFilter(CompositeOperator::Or, filter, filters...);
  }

  template <typename ... Filters>
  static Filter And(const Filter& filter, const Filters& ... filters) {
    return CompositeFilter(CompositeOperator::And, filter, filters...);
  };

  virtual ~FilterInternal() = default;

  friend bool operator==(const FilterInternal& lhs, const FilterInternal& rhs);

 protected:
  enum FilterType {
    Unary, Composite
  };

  explicit FilterInternal(FilterType filterType);

  const FilterType filter_type_;

 private:

  friend class Filter;
  friend class QueryInternal;

  virtual FilterInternal* clone() = 0;

  using UnaryOperator = core::FieldFilter::Operator;
  using CompositeOperator = core::CompositeFilter::Operator;

  static Filter UnaryFilter(const FieldPath& field_path,
                            UnaryOperator op, const FieldValue& value);
  static Filter UnaryFilter(const FieldPath& field_path,
                            UnaryOperator op, const std::vector<FieldValue>& values);

  static Filter CompositeFilter(CompositeOperator op, const Filter& filter) {
    return filter;
  }

  template <typename ... Filters>
  static Filter CompositeFilter(CompositeOperator op, const Filter& filter, const Filters& ... filters);
};

inline bool operator!=(const FilterInternal& lhs, const FilterInternal& rhs) {
  return !(lhs == rhs);
}

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_SRC_MAIN_FILTER_MAIN_H_
