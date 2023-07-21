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

#include <vector>

#include "Firestore/core/src/api/query_core.h"
#include "Firestore/core/src/core/composite_filter.h"
#include "Firestore/core/src/core/filter.h"
#include "Firestore/core/src/model/field_path.h"
#include "firestore/src/include/firebase/firestore/filter.h"
#include "firestore/src/main/user_data_converter_main.h"

namespace firebase {
namespace firestore {

class Filter;

class FilterInternal {
 public:
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

  virtual core::Filter ToCoreFilter(
      const api::Query& query,
      const firebase::firestore::UserDataConverter& user_data_converter)
      const = 0;

  virtual ~FilterInternal() = default;

  friend bool operator==(const FilterInternal& lhs, const FilterInternal& rhs);

 protected:
  enum FilterType { Unary, Composite };

  explicit FilterInternal(FilterType filterType);

  const FilterType filter_type_;

  virtual bool IsEmpty() const = 0;

 private:
  friend class Filter;
  friend class QueryInternal;

  virtual FilterInternal* clone() = 0;

  using FieldFilterOperator = core::FieldFilter::Operator;
  using CompositeOperator = core::CompositeFilter::Operator;

  static Filter UnaryFilter(const FieldPath& field_path,
                            FieldFilterOperator op,
                            const FieldValue& value);
  static Filter UnaryFilter(const FieldPath& field_path,
                            FieldFilterOperator op,
                            const std::vector<FieldValue>& values);

  static Filter CompositeFilter(CompositeOperator op,
                                const std::vector<Filter>& filters);
};

bool operator==(const FilterInternal& lhs, const FilterInternal& rhs);

inline bool operator!=(const FilterInternal& lhs, const FilterInternal& rhs) {
  return !(lhs == rhs);
}

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_SRC_MAIN_FILTER_MAIN_H_
