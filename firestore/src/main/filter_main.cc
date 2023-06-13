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

#include "firestore/src/main/filter_main.h"
#include "firestore/src/main/composite_filter_main.h"
#include "firestore/src/main/converter_main.h"
#include "firestore/src/main/unary_filter_main.h"

namespace firebase {
namespace firestore {

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

template <typename... Filters>
Filter FilterInternal::CompositeFilter(core::CompositeFilter::Operator op,
                                       const Filter& filter,
                                       const Filters&... filters) {
  return MakePublic(CompositeFilterInternal(op, filter, filters...));
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
