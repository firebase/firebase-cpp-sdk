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

#include "app/src/assert.h"
#include "app/src/include/firebase/variant.h"
#include "database/src/common/query_spec.h"
#include "database/src/desktop/query_params_comparator.h"
#include "database/src/desktop/util_desktop.h"

namespace firebase {
namespace database {
namespace internal {

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

IndexedVariant::IndexedVariant(const IndexedVariant& other)
    : variant_(other.variant_),
      query_params_(other.query_params_),
      index_(QueryParamsLesser(&query_params_)) {
  EnsureIndexed();
}

IndexedVariant& IndexedVariant::operator=(const IndexedVariant& other) {
  variant_ = other.variant_;
  query_params_ = other.query_params_;
  index_ = Index(QueryParamsLesser(&query_params_));
  EnsureIndexed();
  return *this;
}

const char* IndexedVariant::GetPredecessorChildName(
    const std::string& child_key, const Variant& child_value) const {
  Variant key = child_key.c_str();
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
                                                 const Variant& value) {
  switch (query_params_.order_by) {
    case QueryParams::kOrderByPriority: {
      return &GetVariantPriority(value);
    }
    case QueryParams::kOrderByChild: {
      if (value.is_map()) {
        auto iter = value.map().find(query_params_.order_by_child);
        if (iter != value.map().end()) {
          return GetVariantValue(&iter->second);
        }
      }
      return nullptr;
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

static bool IsDefinedOn(const Variant& variant, const QueryParams& params) {
  switch (params.order_by) {
    case QueryParams::kOrderByPriority: {
      return !VariantIsEmpty(GetVariantPriority(variant));
    }
    case QueryParams::kOrderByKey: {
      return true;
    }
    case QueryParams::kOrderByChild: {
      return !VariantIsEmpty(
          VariantGetChild(&variant, Path(params.order_by_child)));
    }
    case QueryParams::kOrderByValue: {
      return true;
    }
  }
}

void IndexedVariant::EnsureIndexed() {
  // If this isn't a map, there's no index to build.
  if (!variant_.is_map()) {
    return;
  }

  PruneNulls(&variant_);

  for (const auto& entry : variant_.map()) {
    index_.insert(entry);
  }
}

IndexedVariant IndexedVariant::UpdateChild(const std::string& key,
                                           const Variant& child) const {
  // Remove element.
  Variant result = variant_.is_map() ? variant_ : Variant::EmptyMap();
  if (child.is_null()) {
    result.map().erase(key);
  } else {
    result.map()[key] = child;
  }
  return IndexedVariant(result, query_params_);
}

IndexedVariant IndexedVariant::UpdatePriority(const Variant& priority) const {
  return IndexedVariant(CombineValueAndPriority(variant_, priority),
                        query_params_);
}

Optional<std::pair<Variant, Variant>> IndexedVariant::GetFirstChild() const {
  if (index().empty()) {
    return Optional<std::pair<Variant, Variant>>();
  } else {
    auto front = index().begin();
    return Optional<std::pair<Variant, Variant>>(
        std::make_pair(front->first, front->second));
  }
}

Optional<std::pair<Variant, Variant>> IndexedVariant::GetLastChild() const {
  if (index().empty()) {
    return Optional<std::pair<Variant, Variant>>();
  } else {
    auto back = --index().end();
    return Optional<std::pair<Variant, Variant>>(
        std::make_pair(back->first, back->second));
  }
}

bool operator==(const IndexedVariant& lhs, const IndexedVariant& rhs) {
  return lhs.variant() == rhs.variant() &&
         lhs.query_params() == rhs.query_params();
}

bool operator!=(const IndexedVariant& lhs, const IndexedVariant& rhs) {
  return !(lhs == rhs);
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
