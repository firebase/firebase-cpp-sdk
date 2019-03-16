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

#include "database/src/desktop/view/limited_filter.h"
#include "app/src/assert.h"
#include "app/src/path.h"
#include "database/src/common/query_spec.h"
#include "database/src/desktop/core/write_tree.h"
#include "database/src/desktop/query_params_comparator.h"
#include "database/src/desktop/util_desktop.h"
#include "database/src/desktop/view/variant_filter.h"

namespace firebase {
namespace database {
namespace internal {

LimitedFilter::LimitedFilter(const QueryParams& params)
    : VariantFilter(params),
      ranged_filter_(MakeUnique<RangedFilter>(params)),
      limit_(params.limit_first ? params.limit_first : params.limit_last),
      reverse_(!!params.limit_last) {}

LimitedFilter::~LimitedFilter() {}

IndexedVariant LimitedFilter::UpdateChild(
    const IndexedVariant& indexed_variant, const std::string& key,
    const Variant& new_child, const Path& affected_path,
    const CompleteChildSource* source,
    ChildChangeAccumulator* opt_change_accumulator) const {
  const Variant& variant =
      ranged_filter_->Matches(std::make_pair(key, new_child)) ? new_child
                                                              : kNullVariant;

  const Variant* child = GetInternalVariant(&indexed_variant.variant(), key);
  if (child && (*child == variant)) {
    // No change.
    return indexed_variant;
  }

  std::size_t size = indexed_variant.variant().is_map()
                         ? indexed_variant.variant().map().size()
                         : 0;
  if (size < limit_) {
    return ranged_filter_->GetIndexedFilter()->UpdateChild(
        indexed_variant, key, variant, affected_path, source,
        opt_change_accumulator);
  } else {
    return FullLimitUpdateChild(indexed_variant, key, variant, source,
                                opt_change_accumulator);
  }
}

template <typename IteratorType>
IndexedVariant UpdateFullVariantHelper(
    IndexedVariant filtered, int limit, IteratorType iter,
    IteratorType iter_end, const std::pair<Variant, Variant>& start_post,
    const std::pair<Variant, Variant>& end_post, int sign,
    const QueryParams& params) {
  int count = 0;
  bool found_start_post = false;
  QueryParamsComparator comp(&params);
  for (; iter != iter_end; ++iter) {
    std::pair<Variant, Variant> next = *iter;
    if (!found_start_post && comp.Compare(start_post, next) * sign <= 0) {
      // start adding
      found_start_post = true;
    }
    bool in_range = found_start_post && count < limit &&
                    comp.Compare(next, end_post) * sign <= 0;
    if (in_range) {
      count++;
    } else {
      filtered =
          filtered.UpdateChild(next.first.string_value(), Variant::Null());
    }
  }
  return filtered;
}

IndexedVariant LimitedFilter::UpdateFullVariant(
    const IndexedVariant& old_snap, const IndexedVariant& new_snap,
    ChildChangeAccumulator* opt_change_accumulator) const {
  IndexedVariant filtered;
  if (VariantIsLeaf(new_snap.variant()) || VariantIsEmpty(new_snap.variant())) {
    // Make sure we have a children node with the correct index, not an empty or
    // leaf node;
    filtered = IndexedVariant(Variant::Null(), query_params());
  } else {
    // Don't support priorities on queries
    filtered = new_snap.UpdatePriority(Variant::Null());
    if (reverse_) {
      filtered = UpdateFullVariantHelper(
          filtered, limit_, new_snap.index().rbegin(), new_snap.index().rend(),
          ranged_filter_->end_post(), ranged_filter_->start_post(), -1,
          query_params());
    } else {
      filtered = UpdateFullVariantHelper(
          filtered, limit_, new_snap.index().begin(), new_snap.index().end(),
          ranged_filter_->start_post(), ranged_filter_->end_post(), 1,
          query_params());
    }
  }
  return ranged_filter_->GetIndexedFilter()->UpdateFullVariant(
      old_snap, filtered, opt_change_accumulator);
}

IndexedVariant LimitedFilter::UpdatePriority(
    const IndexedVariant& old_snap, const Variant& new_priority) const {
  return old_snap;
}

const VariantFilter* LimitedFilter::GetIndexedFilter() const { return this; }

bool LimitedFilter::FiltersVariants() const { return true; }

IndexedVariant LimitedFilter::FullLimitUpdateChild(
    const IndexedVariant& old_indexed, const std::string& child_key,
    const Variant& child_snap, const CompleteChildSource* source,
    ChildChangeAccumulator* opt_change_accumulator) const {
  std::pair<Variant, Variant> new_child_node(child_key, child_snap);
  Optional<std::pair<Variant, Variant>> window_boundary =
      reverse_ ? old_indexed.GetFirstChild() : old_indexed.GetLastChild();
  bool in_range = ranged_filter_->Matches(new_child_node);

  IterationDirection direction = reverse_ ? kIterateReverse : kIterateForward;
  int coefficient = (direction == kIterateReverse) ? -1 : 1;
  QueryParamsComparator comp(&query_params());

  const Variant* old_child_snap =
      GetInternalVariant(&old_indexed.variant(), child_key);
  if (old_child_snap) {
    Optional<std::pair<Variant, Variant>> next_child =
        source->GetChildAfterChild(query_params(), *window_boundary, direction);
    while (next_child.has_value() &&
           (next_child->first == child_key ||
            GetInternalVariant(&old_indexed.variant(), next_child->first))) {
      // There is a weird edge case where a node is updated as part of a merge
      // in the write tree, but hasn't been applied to the limited filter yet.
      // Ignore this next child which will be updated later in the limited
      // filter...
      next_child =
          source->GetChildAfterChild(query_params(), *next_child, direction);
    }

    int compare_next =
        next_child.has_value()
            ? comp.Compare(*next_child, new_child_node) * coefficient
            : 1;
    bool remains_in_window =
        in_range && !VariantIsEmpty(child_snap) && compare_next >= 0;
    if (remains_in_window) {
      if (opt_change_accumulator) {
        TrackChildChange(
            ChildChangedChange(child_key, child_snap, *old_child_snap),
            opt_change_accumulator);
      }
      return old_indexed.UpdateChild(child_key, child_snap);
    } else {
      if (opt_change_accumulator) {
        TrackChildChange(ChildRemovedChange(child_key, *old_child_snap),
                         opt_change_accumulator);
      }
      IndexedVariant new_indexed =
          old_indexed.UpdateChild(child_key, Variant::Null());
      bool next_child_in_range =
          next_child.has_value() && ranged_filter_->Matches(*next_child);
      if (next_child_in_range) {
        if (opt_change_accumulator) {
          TrackChildChange(ChildAddedChange(next_child->first.string_value(),
                                            next_child->second),
                           opt_change_accumulator);
        }
        return new_indexed.UpdateChild(next_child->first.string_value(),
                                       next_child->second);
      } else {
        return new_indexed;
      }
    }
  } else if (VariantIsEmpty(child_snap)) {
    // We're deleting a node, but it was not in the window, so ignore it.
    return old_indexed;
  } else if (in_range) {
    if (comp.Compare(*window_boundary, new_child_node) * coefficient >= 0) {
      if (opt_change_accumulator) {
        TrackChildChange(
            ChildRemovedChange(window_boundary->first.string_value(),
                               window_boundary->second),
            opt_change_accumulator);
        TrackChildChange(ChildAddedChange(child_key, child_snap),
                         opt_change_accumulator);
      }
      return old_indexed.UpdateChild(child_key, child_snap)
          .UpdateChild(window_boundary->first.string_value(), Variant::Null());
    } else {
      return old_indexed;
    }
  } else {
    return old_indexed;
  }
  return old_indexed;
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
