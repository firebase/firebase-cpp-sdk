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

#include "database/src/desktop/view/indexed_filter.h"

#include "app/src/assert.h"
#include "app/src/path.h"
#include "database/src/common/query_spec.h"
#include "database/src/desktop/util_desktop.h"
#include "database/src/desktop/view/variant_filter.h"

namespace firebase {
namespace database {
namespace internal {

IndexedFilter::~IndexedFilter() {}

IndexedVariant IndexedFilter::UpdateChild(
    const IndexedVariant& indexed_variant, const std::string& key,
    const Variant& new_child, const Path& affected_path,
    const CompleteChildSource* source,
    ChildChangeAccumulator* opt_change_accumulator) const {
  FIREBASE_DEV_ASSERT_MESSAGE(
      indexed_variant.query_params().order_by == query_params().order_by,
      "The index must match the filter");
  const Variant& snap = indexed_variant.variant();
  const Variant& old_child = VariantGetChild(&snap, key);
  // Check if anything actually changed.
  const Variant& old_descendant = VariantGetChild(&old_child, affected_path);
  const Variant& new_descendant = VariantGetChild(&new_child, affected_path);
  if (VariantsAreEquivalent(old_descendant, new_descendant)) {
    // There's an edge case where a child can enter or leave the view because
    // affected_path was set to null. In this case, affected_path will appear
    // null in both the old and new snapshots. So we need to avoid treating
    // these cases as "nothing changed."
    if (VariantIsEmpty(old_child) == VariantIsEmpty(new_child)) {
      // Nothing changed.
      return indexed_variant;
    }
  }
  // If we have a ChangeAccumulator, accumulate the changes.
  if (opt_change_accumulator != nullptr) {
    if (VariantIsEmpty(new_child)) {
      // If the new child is null, something was removed. Track the removal.
      TrackChildChange(ChildRemovedChange(key, old_child),
                       opt_change_accumulator);
    } else if (VariantIsEmpty(old_child)) {
      // If the old child was null, something was added. Track the addition.
      TrackChildChange(ChildAddedChange(key, new_child),
                       opt_change_accumulator);
    } else {
      // Otherwise, something was changed. Track the change.
      TrackChildChange(ChildChangedChange(key, new_child, old_child),
                       opt_change_accumulator);
    }
  }
  if (VariantIsLeaf(snap) && VariantIsEmpty(new_child)) {
    return indexed_variant;
  } else {
    // Make sure the variant is indexed.
    return indexed_variant.UpdateChild(key, new_child);
  }
}

IndexedVariant IndexedFilter::UpdateFullVariant(
    const IndexedVariant& old_snap, const IndexedVariant& new_snap,
    ChildChangeAccumulator* opt_change_accumulator) const {
  FIREBASE_DEV_ASSERT_MESSAGE(
      new_snap.query_params().order_by == query_params().order_by,
      "Can't use IndexedVariant that doesn't have filter's ordering");
  // If we have a ChangeAccumulator, accumulate the changes.
  if (opt_change_accumulator != nullptr) {
    // Check which elements were removed.
    const Variant* old_value = GetVariantValue(&old_snap.variant());
    const Variant* new_value = GetVariantValue(&new_snap.variant());
    if (old_value->is_map()) {
      for (auto& key_value_pair : old_value->map()) {
        const Variant& key = key_value_pair.first;
        const Variant& value = key_value_pair.second;
        if (!GetInternalVariant(new_value, key)) {
          TrackChildChange(ChildRemovedChange(key.string_value(), value),
                           opt_change_accumulator);
        }
      }
    }
    if (new_value->is_map()) {
      // Check which elements were changed or added.
      for (auto& key_value_pair : new_value->map()) {
        std::string key = key_value_pair.first.string_value();
        const Variant& value = key_value_pair.second;
        const Variant* old_child = GetInternalVariant(old_value, key);
        // If there is an old_child, we should track it if it is different.
        // If there is not an old child, we should track that something was
        // added.
        if (old_child) {
          if (*old_child != value) {
            TrackChildChange(ChildChangedChange(key, value, *old_child),
                             opt_change_accumulator);
          }
        } else {
          TrackChildChange(ChildAddedChange(key, value),
                           opt_change_accumulator);
        }
      }
    }
  }
  return new_snap;
}

IndexedVariant IndexedFilter::UpdatePriority(
    const IndexedVariant& old_snap, const Variant& new_priority) const {
  if (old_snap.variant().is_null()) {
    return old_snap;
  } else {
    return old_snap.UpdatePriority(new_priority);
  }
}

const VariantFilter* IndexedFilter::GetIndexedFilter() const { return this; }

bool IndexedFilter::FiltersVariants() const { return false; }

}  // namespace internal
}  // namespace database
}  // namespace firebase
