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

#if defined(__ANDROID__)
#error "This header should not be used on Android."
#endif

#include "firestore/src/main/composite_filter_main.h"
#include "Firestore/core/src/core/composite_filter.h"

namespace firebase {
namespace firestore {

template<typename ...Ts>
CompositeFilterInternal::CompositeFilterInternal(
    core::CompositeFilter::Operator op,
    const Filter& filter, Ts... filters)
    : FilterInternal(FilterType::Composite), op_(op), filters_(filter, filters...) {}

CompositeFilterInternal* CompositeFilterInternal::clone() {
  return new CompositeFilterInternal(*this);
}

core::Filter CompositeFilterInternal::filter_core(
    const api::Query& query,
    const firebase::firestore::UserDataConverter& user_data_converter) const {
  std::vector<core::Filter> core_filters;
  for (auto& filter : filters_) {
    core_filters.push_back(filter->filter_core(query, user_data_converter));
  }
  return core::CompositeFilter::Create(std::move(core_filters), op_);
}

bool operator==(const CompositeFilterInternal& lhs, const CompositeFilterInternal& rhs) {
  return lhs.op_ == rhs.op_ && lhs.filters_ == rhs.filters_;
}

} // namespace firestore
}  // namespace firebase
