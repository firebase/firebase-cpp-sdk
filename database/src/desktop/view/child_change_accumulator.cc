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

#include "database/src/desktop/view/child_change_accumulator.h"
#include <cassert>
#include <map>
#include <string>
#include "app/src/assert.h"
#include "database/src/desktop/view/change.h"
#include "database/src/desktop/view/event.h"

namespace firebase {
namespace database {
namespace internal {

void TrackChildChange(const Change& change,
                      ChildChangeAccumulator* accumulator) {
  EventType type = change.event_type;

  // This function should only be used for ChildAdd, ChildChange and ChildRemove
  // changes.
  FIREBASE_DEV_ASSERT_MESSAGE(type == kEventTypeChildAdded ||
                                  type == kEventTypeChildChanged ||
                                  type == kEventTypeChildRemoved,
                              "Only child changes supported for tracking");

  const std::string& child_key = change.child_key;

  // Sanity check
  FIREBASE_DEV_ASSERT(change.child_key != ".priority");

  auto iter = accumulator->find(child_key);
  if (iter != accumulator->end()) {
    // If the change data exists for the given child, try to merge the existing
    // change data and the new one.
    const Change& old_change = iter->second;
    EventType old_type = old_change.event_type;

    if (type == kEventTypeChildAdded && old_type == kEventTypeChildRemoved) {
      // Change from kTypeChildRemoved to kTypeChildAdded => kTypeChildChanged
      iter->second = ChildChangedChange(child_key, change.indexed_variant,
                                        old_change.indexed_variant);
    } else if (type == kEventTypeChildRemoved &&
               old_type == kEventTypeChildAdded) {
      // Change from kTypeChildAdded to kTypeChildRemoved => delete data
      accumulator->erase(iter);
    } else if (type == kEventTypeChildRemoved &&
               old_type == kEventTypeChildChanged) {
      // Change from kTypeChildChanged to kTypeChildRemoved => kTypeChildRemoved
      iter->second = ChildRemovedChange(child_key, old_change.indexed_variant);
    } else if (type == kEventTypeChildChanged &&
               old_type == kEventTypeChildAdded) {
      // Change from kTypeChildAdded to kTypeChildChanged => kTypeChildAdded
      iter->second = ChildAddedChange(child_key, change.indexed_variant);
    } else if (type == kEventTypeChildChanged &&
               old_type == kEventTypeChildChanged) {
      // Change from kTypeChildChanged to kTypeChildChanged => kTypeChildChanged
      iter->second = ChildChangedChange(child_key, change.indexed_variant,
                                        old_change.old_indexed_variant);
    } else {
      // Assert for the other cases
      FIREBASE_DEV_ASSERT_MESSAGE(false, "Illegal combination of changes");
    }
  } else {
    // Insert the change data if it is a new change to the given child
    (*accumulator)[change.child_key] = change;
  }
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
