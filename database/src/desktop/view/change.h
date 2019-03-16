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

#ifndef FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_VIEW_CHANGE_H_
#define FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_VIEW_CHANGE_H_

#include <string>
#include "app/src/include/firebase/variant.h"
#include "database/src/desktop/core/indexed_variant.h"
#include "database/src/desktop/view/event_type.h"

namespace firebase {
namespace database {
namespace internal {

// Represents a change that has occurred at a location in the database.
struct Change {
  Change()
      : event_type(),
        indexed_variant(),
        child_key(),
        prev_name(),
        old_indexed_variant() {}

  Change(EventType _event_type, const IndexedVariant& _indexed_variant)
      : event_type(_event_type),
        indexed_variant(_indexed_variant),
        child_key(),
        prev_name(),
        old_indexed_variant() {}

  Change(EventType _event_type, const IndexedVariant& _indexed_variant,
         const std::string& _child_key)
      : event_type(_event_type),
        indexed_variant(_indexed_variant),
        child_key(_child_key),
        prev_name(),
        old_indexed_variant() {}

  Change(EventType _event_type, const IndexedVariant& _indexed_variant,
         const std::string& _child_key, const std::string& _prev_name,
         const IndexedVariant& _old_indexed_variant)
      : event_type(_event_type),
        indexed_variant(_indexed_variant),
        child_key(_child_key),
        prev_name(_prev_name),
        old_indexed_variant(_old_indexed_variant) {}

  // The type of event that has occurred.
  EventType event_type;
  // The new value (including the new sorting order).
  IndexedVariant indexed_variant;
  // The key of the location that was changed.
  std::string child_key;
  // The previous name of this value, if this value was moved.
  std::string prev_name;
  // The previous value that is being overwritten.
  IndexedVariant old_indexed_variant;
};

bool operator==(const Change& lhs, const Change& rhs);
bool operator!=(const Change& lhs, const Change& rhs);

// Utility functions to create Changes.
Change ValueChange(const IndexedVariant& snapshot);
Change ChildAddedChange(const std::string& child_key, const Variant& snapshot);
Change ChildAddedChange(const std::string& child_key,
                        const IndexedVariant& snapshot);
Change ChildRemovedChange(const std::string& child_key,
                          const Variant& snapshot);
Change ChildRemovedChange(const std::string& child_key,
                          const IndexedVariant& snapshot);
Change ChildChangedChange(const std::string& child_key,
                          const Variant& newSnapshot,
                          const Variant& old_snapshot);
Change ChildChangedChange(const std::string& child_key,
                          const IndexedVariant& new_snapshot,
                          const IndexedVariant& old_snapshot);
Change ChildMovedChange(const std::string& child_key, const Variant& snapshot);
Change ChildMovedChange(const std::string& child_key,
                        const IndexedVariant& snapshot);
Change ChangeWithPrevName(const Change& change, const std::string& prev_name);

}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_VIEW_CHANGE_H_
