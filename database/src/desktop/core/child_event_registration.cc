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

#include <cassert>

#include "database/src/desktop/data_snapshot_desktop.h"
#include "database/src/desktop/database_desktop.h"
#include "database/src/desktop/view/event.h"
#include "database/src/desktop/view/event_type.h"
#include "database/src/include/firebase/database/common.h"
#include "firebase/database/data_snapshot.h"

namespace firebase::database::internal {

ChildEventRegistration::ChildEventRegistration(DatabaseInternal* database,
                                               ChildListener* listener,
                                               const QuerySpec& query_spec)
    : EventRegistration(query_spec),
      database_ref_(
          database
              ? database->this_ref()
              : firebase::internal::SafeReference<DatabaseInternal>(nullptr)),
      listener_(listener) {}

ChildEventRegistration::~ChildEventRegistration() {
  // Only unregister if still active. When DatabaseInternal unregisters a
  // listener, RemoveActiveEventRegistrations marks status as kRemoved and
  // erases the lookup entry beforehand, avoiding redundant re-entrant calls.
  // Lock order is always SafeReferenceLock(database_ref_) -> listener_mutex_:
  // ~DatabaseInternal() calls safe_this_.ClearReference() before acquiring any
  // mutex or tearing down Repo, and DatabaseInternal never acquires safe_this_
  // or destroys EventRegistrations while holding listener_mutex_.
  if (status() == kActive) {
    set_status(kRemoved);
    firebase::internal::SafeReferenceLock<DatabaseInternal> lock(
        &database_ref_);
    DatabaseInternal* db = lock.GetReference();
    if (db != nullptr) {
      db->RemoveEventRegistration(query_spec(), listener_, this);
    }
  }
}

bool ChildEventRegistration::RespondsTo(EventType event_type) {
  return event_type == kEventTypeChildRemoved ||
         event_type == kEventTypeChildAdded ||
         event_type == kEventTypeChildMoved ||
         event_type == kEventTypeChildChanged;
}

Event ChildEventRegistration::GenerateEvent(const Change& change,
                                            const QuerySpec& query_spec) {
  firebase::internal::SafeReferenceLock<DatabaseInternal> lock(&database_ref_);
  return Event(
      change.event_type, this,
      DataSnapshotInternal(lock.GetReference(),
                           change.indexed_variant.variant(),
                           QuerySpec(query_spec.path.GetChild(change.child_key),
                                     change.indexed_variant.query_params())),
      change.prev_name);
}

void ChildEventRegistration::FireEvent(const Event& event) {
  if (!listener_ || !event.snapshot.has_value()) {
    return;
  }
  switch (event.type) {
    case kEventTypeChildAdded: {
      DataSnapshot snapshot(new DataSnapshotInternal(*event.snapshot));
      listener_->OnChildAdded(snapshot, event.prev_name.c_str());
      break;
    }
    case kEventTypeChildChanged: {
      DataSnapshot snapshot(new DataSnapshotInternal(*event.snapshot));
      listener_->OnChildChanged(snapshot, event.prev_name.c_str());
      break;
    }
    case kEventTypeChildMoved: {
      DataSnapshot snapshot(new DataSnapshotInternal(*event.snapshot));
      listener_->OnChildMoved(snapshot, event.prev_name.c_str());
      break;
    }
    case kEventTypeChildRemoved: {
      DataSnapshot snapshot(new DataSnapshotInternal(*event.snapshot));
      listener_->OnChildRemoved(snapshot);
      break;
    }
    // These should never happen.
    case kEventTypeValue:
    case kEventTypeError:
    default: {
      assert(false);
      break;
    }
  }
}

void ChildEventRegistration::FireCancelEvent(Error error) {
  if (!listener_) {
    return;
  }
  listener_->OnCancelled(error, GetErrorMessage(error));
}

bool ChildEventRegistration::MatchesListener(const void* listener_ptr) const {
  return static_cast<const void*>(listener_) == listener_ptr;
}

}  // namespace firebase::database::internal
