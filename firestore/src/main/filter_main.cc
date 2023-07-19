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

#include <vector>

#include "firestore/src/main/composite_filter_main.h"
#include "firestore/src/main/converter_main.h"
#include "firestore/src/main/filter_main.h"
#include "firestore/src/main/unary_filter_main.h"

namespace firebase {
namespace firestore {

Filter FilterInternal::ArrayContains(const FieldPath& field,
                                     const FieldValue& value) {
  return UnaryFilter(field, FieldFilterOperator::ArrayContains, value);
}

Filter FilterInternal::ArrayContainsAny(const FieldPath& field,
                                        const std::vector<FieldValue>& values) {
  return UnaryFilter(field, FieldFilterOperator::ArrayContainsAny, values);
}

Filter FilterInternal::EqualTo(const FieldPath& field,
                               const FieldValue& value) {
  return UnaryFilter(field, FieldFilterOperator::Equal, value);
}

Filter FilterInternal::NotEqualTo(const FieldPath& field,
                                  const FieldValue& value) {
  return UnaryFilter(field, FieldFilterOperator::NotEqual, value);
}

Filter FilterInternal::GreaterThan(const FieldPath& field,
                                   const FieldValue& value) {
  return UnaryFilter(field, FieldFilterOperator::GreaterThan, value);
}

Filter FilterInternal::GreaterThanOrEqualTo(const FieldPath& field,
                                            const FieldValue& value) {
  return UnaryFilter(field, FieldFilterOperator::GreaterThanOrEqual, value);
}

Filter FilterInternal::LessThan(const FieldPath& field,
                                const FieldValue& value) {
  return UnaryFilter(field, FieldFilterOperator::LessThan, value);
}

Filter FilterInternal::LessThanOrEqualTo(const FieldPath& field,
                                         const FieldValue& value) {
  return UnaryFilter(field, FieldFilterOperator::LessThanOrEqual, value);
}

Filter FilterInternal::In(const FieldPath& field,
                          const std::vector<FieldValue>& values) {
  return UnaryFilter(field, FieldFilterOperator::In, values);
}

Filter FilterInternal::NotIn(const FieldPath& field,
                             const std::vector<FieldValue>& values) {
  return UnaryFilter(field, FieldFilterOperator::NotIn, values);
}

Filter FilterInternal::Or(const std::vector<Filter>& filters) {
  return CompositeFilter(CompositeOperator::Or, filters);
}

Filter FilterInternal::And(const std::vector<Filter>& filters) {
  return CompositeFilter(CompositeOperator::And, filters);
}

FilterInternal::FilterInternal(FilterInternal::FilterType filter_type)
    : filter_type_(filter_type) {}

Filter FilterInternal::UnaryFilter(const FieldPath& field_path,
                                   FieldFilterOperator op,
                                   const FieldValue& value) {
  return MakePublic(UnaryFilterInternal(field_path, op, value));
}

Filter FilterInternal::UnaryFilter(const FieldPath& field_path,
                                   FieldFilterOperator op,
                                   const std::vector<FieldValue>& values) {
  return MakePublic(UnaryFilterInternal(field_path, op, values));
}

Filter FilterInternal::CompositeFilter(core::CompositeFilter::Operator op,
                                       const std::vector<Filter>& filters) {
  std::vector<FilterInternal*> nonEmptyFilters{};
  for (const Filter& filter : filters) {
    FilterInternal* filterInternal = GetInternal(&filter);
    if (!filterInternal->IsEmpty()) {
      nonEmptyFilters.push_back(filterInternal->clone());
    }
  }
  if (nonEmptyFilters.size() == 1) {
    return Filter(nonEmptyFilters[0]);
  }
  return MakePublic(CompositeFilterInternal(op, nonEmptyFilters));
}

bool operator==(const FilterInternal& lhs, const FilterInternal& rhs) {
  if (lhs.filter_type_ == rhs.filter_type_) {
    switch (lhs.filter_type_) {
      case FilterInternal::Composite:
        return *static_cast<const CompositeFilterInternal*>(&lhs) ==
               *static_cast<const CompositeFilterInternal*>(&rhs);
      case FilterInternal::Unary:
        return *static_cast<const UnaryFilterInternal*>(&lhs) ==
               *static_cast<const UnaryFilterInternal*>(&rhs);
    }
  }
  return false;
}

}  // namespace firestore
}  // namespace firebase
