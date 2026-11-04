// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "database/src/desktop/core/indexed_variant.h"

#include <algorithm>
#include <cassert>
#include <string>
#include <utility>

#include "app/src/assert.h"
#include "app/src/include/firebase/variant.h"
#include "app/src/path.h"
#include "database/src/common/query_spec.h"
#include "database/src/desktop/query_params_comparator.h"
#include "database/src/desktop/util_desktop.h"

namespace firebase::database::internal {

IndexedVariant::IndexedVariant()
    : variant_(), query_params_(), index_(QueryParamsLesser(&query_params_)) {
  EnsureIndexed();
}

IndexedVariant::IndexedVariant(const Variant& variant)
    : variant_(variant),
      query_params_(),
      index_(QueryParamsLesser(&query_params_)) {
  EnsureIndexed();
}

IndexedVariant::IndexedVariant(const Variant& variant,
                               const QueryParams& query_params)
    : variant_(variant),
      query_params_(query_params),
      index_(QueryParamsLesser(&query_params_)) {
  EnsureIndexed();
}

IndexedVariant::IndexedVariant(Variant&& variant,
                               const QueryParams& query_params)
    : variant_(std::move(variant)),
      query_params_(query_params),
      index_(QueryParamsLesser(&query_params_)) {
  EnsureIndexed();
}

IndexedVariant::IndexedVariant(const IndexedVariant& other)
    : variant_(other.variant_),
      query_params_(other.query_params_),
      index_(QueryParamsLesser(&query_params_)) {
  EnsureIndexed();
}

IndexedVariant& IndexedVariant::operator=(const IndexedVariant& other) {
  if (&other == this) return *this;
  variant_ = other.variant_;
  query_params_ = other.query_params_;
  index_ = Index(QueryParamsLesser(&query_params_));
  EnsureIndexed();
  return *this;
}

IndexedVariant::IndexedVariant(IndexedVariant&& other) noexcept
    : variant_(std::move(other.variant_)),
      query_params_(std::move(other.query_params_)),
      index_(QueryParamsLesser(&query_params_)) {
  while (!other.index_.empty()) {
    index_.insert(index_.end(), other.index_.extract(other.index_.begin()));
  }
}

IndexedVariant& IndexedVariant::operator=(IndexedVariant&& other) noexcept {
  if (&other == this) return *this;
  variant_ = std::move(other.variant_);
  query_params_ = std::move(other.query_params_);
  index_ = Index(QueryParamsLesser(&query_params_));
  while (!other.index_.empty()) {
    index_.insert(index_.end(), other.index_.extract(other.index_.begin()));
  }
  return *this;
}

const char* IndexedVariant::GetPredecessorChildName(
    const std::string& child_key, const Variant& child_value) const {
  Variant key(child_key);
  auto iter = index_.find(std::make_pair(key, child_value));

  if (iter == index_.end()) {
    return nullptr;
  }
  if (iter == index_.begin()) {
    return nullptr;
  }

  // Move back to predecessor.
  --iter;

  if (!iter->first.is_string()) {
    return nullptr;
  }

  return iter->first.string_value();
}

IndexedVariant::Index::const_iterator IndexedVariant::Find(
    const Variant& key) const {
  return std::find_if(index_.begin(), index_.end(),
                      [&key](const Index::value_type& element) {
                        return key == element.first;
                      });
}

const Variant* IndexedVariant::GetOrderByVariant(const Variant& key,
                                                 const Variant& value) const {
  switch (query_params_.order_by) {
    case QueryParams::kOrderByPriority: {
      return &GetVariantPriority(value);
    }
    case QueryParams::kOrderByChild: {
      Path child_path(query_params_.order_by_child);
      const Variant& child = VariantGetChild(&value, child_path);
      return child.is_null() ? nullptr : GetVariantValue(&child);
    }
    case QueryParams::kOrderByKey: {
      return &key;
    }
    case QueryParams::kOrderByValue: {
      return GetVariantValue(&value);
    }
  }
  FIREBASE_DEV_ASSERT_MESSAGE(false, "Invalid QueryParams::OrderBy");
  return nullptr;
}

void IndexedVariant::EnsureIndexed() {
  // If this isn't a map, there's no index to build.
  if (!variant_.is_map()) {
    return;
  }

  PruneNulls(&variant_);

  for (const auto& entry : variant_.map()) {
    if (entry.first.is_string() && entry.first.string_value()[0] == '.') {
      // Do not index pseudo-keys.
      continue;
    }
    index_.insert(entry);
  }
}

IndexedVariant IndexedVariant::UpdateChild(const std::string& key,
                                           const Variant& child) const {
  Variant result = variant_;
  VariantUpdateChild(&result, key, child);
  return IndexedVariant(std::move(result), query_params_);
}

IndexedVariant IndexedVariant::UpdatePriority(const Variant& priority) const {
  return IndexedVariant(CombineValueAndPriority(variant_, priority),
                        query_params_);
}

std::optional<std::pair<Variant, Variant>> IndexedVariant::GetFirstChild() const {
  if (index().empty()) {
    return std::optional<std::pair<Variant, Variant>>();
  }
  return std::optional<std::pair<Variant, Variant>>(*index().begin());
}

std::optional<std::pair<Variant, Variant>> IndexedVariant::GetLastChild() const {
  if (index().empty()) {
    return std::optional<std::pair<Variant, Variant>>();
  }
  return std::optional<std::pair<Variant, Variant>>(*index().rbegin());
}

bool operator==(const IndexedVariant& lhs, const IndexedVariant& rhs) {
  return lhs.variant() == rhs.variant() &&
         lhs.query_params() == rhs.query_params();
}

bool operator!=(const IndexedVariant& lhs, const IndexedVariant& rhs) {
  return !(lhs == rhs);
}

}  // namespace firebase::database::internal
