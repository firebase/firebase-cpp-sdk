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

#include "database/src/desktop/core/child_event_registration.h"

#include "database/src/desktop/data_snapshot_desktop.h"
#include "database/src/desktop/view/event.h"
#include "database/src/desktop/view/event_type.h"
#include "database/src/include/firebase/database/common.h"
#include "firebase/database/data_snapshot.h"

namespace firebase {
namespace database {
namespace internal {

ChildEventRegistration::~ChildEventRegistration() {}

bool ChildEventRegistration::RespondsTo(EventType event_type) {
  return event_type == kEventTypeChildRemoved ||
         event_type == kEventTypeChildAdded ||
         event_type == kEventTypeChildMoved ||
         event_type == kEventTypeChildChanged;
}

Event ChildEventRegistration::GenerateEvent(const Change& change,
                                            const QuerySpec& query_spec) {
  // return Event(change.event_type, this,
  //              DataSnapshotInternal(database_,
  //                                   query_spec.path.GetChild(change.child_key),
  //                                   change.indexed_variant.variant()),
  //              change.prev_name);
  return Event(
      change.event_type, this,
      DataSnapshotInternal(database_, change.indexed_variant.variant(),
                           QuerySpec(query_spec.path.GetChild(change.child_key),
                                     change.indexed_variant.query_params())),
      change.prev_name);
}

void ChildEventRegistration::FireEvent(const Event& event) {
  DataSnapshot snapshot(new DataSnapshotInternal(*event.snapshot));
  switch (event.type) {
    case kEventTypeChildAdded: {
      listener_->OnChildAdded(snapshot, event.prev_name.c_str());
      break;
    }
    case kEventTypeChildChanged: {
      listener_->OnChildChanged(snapshot, event.prev_name.c_str());
      break;
    }
    case kEventTypeChildMoved: {
      listener_->OnChildMoved(snapshot, event.prev_name.c_str());
      break;
    }
    case kEventTypeChildRemoved: {
      listener_->OnChildRemoved(snapshot);
      break;
    }
    // These should never happen.
    case kEventTypeValue:
    case kEventTypeError:
    default: {
      assert(false);
    }
  }
}

void ChildEventRegistration::FireCancelEvent(Error error) {
  listener_->OnCancelled(error, GetErrorMessage(error));
}

bool ChildEventRegistration::MatchesListener(const void* listener_ptr) const {
  return static_cast<const void*>(listener_) == listener_ptr;
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
