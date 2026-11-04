// Copyright 2023 Google LLC
#include "firestore/src/main/composite_filter_main.h"

#include <utility>
#include <vector>
#include "Firestore/core/src/core/composite_filter.h"
#include "firestore/src/common/util.h"
#include "firestore/src/main/converter_main.h"

namespace firebase::firestore {

CompositeFilterInternal::CompositeFilterInternal(
    core::CompositeFilter::Operator op, std::vector<std::unique_ptr<FilterInternal>> filters)
    : FilterInternal(FilterType::Composite),
      op_(op),
      filters_(std::make_move_iterator(filters.begin()), std::make_move_iterator(filters.end())) {}

CompositeFilterInternal* CompositeFilterInternal::clone() const {
  return new CompositeFilterInternal(*this);
}

bool CompositeFilterInternal::IsEmpty() const { return filters_.empty(); }

core::Filter CompositeFilterInternal::ToCoreFilter(
    const api::Query& query, const UserDataConverter& user_data_converter) const {
  std::vector<core::Filter> core_filters;
  core_filters.reserve(filters_.size());
  for (const auto& filter : filters_) {
    core_filters.push_back(filter->ToCoreFilter(query, user_data_converter));
  }
  return core::CompositeFilter::Create(std::move(core_filters), op_);
}

bool operator==(const CompositeFilterInternal& lhs, const CompositeFilterInternal& rhs) {
  return lhs.op_ == rhs.op_ && lhs.filters_.size() == rhs.filters_.size() &&
         std::equal(lhs.filters_.begin(), lhs.filters_.end(), rhs.filters_.begin(),
                    rhs.filters_.end(),
                    [](const std::shared_ptr<FilterInternal>& lhs_filter,
                       const std::shared_ptr<FilterInternal>& rhs_filter) {
                      return EqualityCompare(lhs_filter.get(), rhs_filter.get());
                    });
}

}  // namespace firebase::firestore
