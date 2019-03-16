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

#include "database/src/desktop/view/change.h"
#include <string>
#include "app/src/include/firebase/variant.h"
#include "database/src/desktop/core/indexed_variant.h"
#include "database/src/desktop/view/event_type.h"

namespace firebase {
namespace database {
namespace internal {

bool operator==(const Change& lhs, const Change& rhs) {
  return lhs.event_type == rhs.event_type &&
         lhs.indexed_variant == rhs.indexed_variant &&
         lhs.child_key == rhs.child_key && lhs.prev_name == rhs.prev_name &&
         lhs.old_indexed_variant == rhs.old_indexed_variant;
}

bool operator!=(const Change& lhs, const Change& rhs) { return !(lhs == rhs); }

Change ValueChange(const IndexedVariant& snapshot) {
  return Change(kEventTypeValue, snapshot);
}

Change ChildAddedChange(const std::string& child_key, const Variant& snapshot) {
  return ChildAddedChange(child_key, IndexedVariant(snapshot));
}

Change ChildAddedChange(const std::string& child_key,
                        const IndexedVariant& snapshot) {
  return Change(kEventTypeChildAdded, snapshot, child_key);
}

Change ChildRemovedChange(const std::string& child_key,
                          const Variant& snapshot) {
  return ChildRemovedChange(child_key, IndexedVariant(snapshot));
}

Change ChildRemovedChange(const std::string& child_key,
                          const IndexedVariant& snapshot) {
  return Change(kEventTypeChildRemoved, snapshot, child_key);
}

Change ChildChangedChange(const std::string& child_key,
                          const Variant& new_snapshot,
                          const Variant& old_snapshot) {
  return ChildChangedChange(child_key, IndexedVariant(new_snapshot),
                            IndexedVariant(old_snapshot));
}

Change ChildChangedChange(const std::string& child_key,
                          const IndexedVariant& new_snapshot,
                          const IndexedVariant& old_snapshot) {
  return Change(kEventTypeChildChanged, new_snapshot, child_key, std::string(),
                old_snapshot);
}

Change ChildMovedChange(const std::string& child_key, const Variant& snapshot) {
  return ChildMovedChange(child_key, IndexedVariant(snapshot));
}

Change ChildMovedChange(const std::string& child_key,
                        const IndexedVariant& snapshot) {
  return Change(kEventTypeChildMoved, snapshot, child_key);
}

Change ChangeWithPrevName(const Change& change, const std::string& prev_name) {
  return Change(change.event_type, change.indexed_variant, change.child_key,
                prev_name, change.old_indexed_variant);
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
