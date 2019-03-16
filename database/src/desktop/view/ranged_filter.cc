// Copyright 2019 Google LLC
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

#include "database/src/desktop/view/ranged_filter.h"
#include <utility>
#include "app/src/assert.h"
#include "app/src/path.h"
#include "database/src/common/query_spec.h"
#include "database/src/desktop/query_params_comparator.h"
#include "database/src/desktop/util_desktop.h"
#include "database/src/desktop/view/variant_filter.h"

namespace firebase {
namespace database {
namespace internal {

RangedFilter::RangedFilter(const QueryParams& params)
    : VariantFilter(params),
      indexed_filter_(MakeUnique<IndexedFilter>(params)),
      start_post_(GetStartPost(params)),
      end_post_(GetEndPost(params)) {}

RangedFilter::~RangedFilter() {}

IndexedVariant RangedFilter::UpdateChild(
    const IndexedVariant& indexed_variant, const std::string& key,
    const Variant& new_child, const Path& affected_path,
    const CompleteChildSource* source,
    ChildChangeAccumulator* opt_change_accumulator) const {
  const Variant& variant = Matches(key, new_child) ? new_child : kNullVariant;
  return indexed_filter_->UpdateChild(indexed_variant, key, variant,
                                      affected_path, source,
                                      opt_change_accumulator);
}

IndexedVariant RangedFilter::UpdateFullVariant(
    const IndexedVariant& old_snap, const IndexedVariant& new_snap,
    ChildChangeAccumulator* opt_change_accumulator) const {
  IndexedVariant filtered;
  if (VariantIsLeaf(new_snap.variant())) {
    // Make sure we have a children node with the correct index, not an empty or
    // leaf node;
    filtered = IndexedVariant(Variant::Null(), query_params());
  } else {
    // Don't support priorities on queries.
    filtered = new_snap.UpdatePriority(Variant::Null());
    if (new_snap.variant().is_map()) {
      for (const auto& child : new_snap.variant().map()) {
        if (!Matches(child)) {
          filtered = filtered.UpdateChild(child.first.AsString().string_value(),
                                          Variant::Null());
        }
      }
    }
  }
  return indexed_filter_->UpdateFullVariant(old_snap, filtered,
                                            opt_change_accumulator);
}

IndexedVariant RangedFilter::UpdatePriority(const IndexedVariant& old_snap,
                                            const Variant& new_priority) const {
  // Don't support priorities on queries.
  return old_snap;
}

const VariantFilter* RangedFilter::GetIndexedFilter() const { return this; }

bool RangedFilter::FiltersVariants() const { return true; }

bool RangedFilter::Matches(const Variant& key, const Variant& value) const {
  QueryParamsComparator comp(&query_params());
  return comp.Compare(start_post_.first, start_post_.second, key, value) <= 0 &&
         comp.Compare(key, value, end_post_.first, end_post_.second) <= 0;
}

bool RangedFilter::Matches(const std::pair<Variant, Variant>& node) const {
  return Matches(node.first, node.second);
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
