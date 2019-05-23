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

#include "database/src/desktop/core/value_event_registration.h"

#include "database/src/desktop/data_snapshot_desktop.h"
#include "database/src/include/firebase/database/common.h"
#include "firebase/database/data_snapshot.h"

namespace firebase {
namespace database {
namespace internal {

ValueEventRegistration::~ValueEventRegistration() {}

bool ValueEventRegistration::RespondsTo(EventType event_type) {
  return event_type == kEventTypeValue;
}

Event ValueEventRegistration::GenerateEvent(const Change& change,
                                            const QuerySpec& query_spec) {
  // return Event(kEventTypeValue, this,
  //              DataSnapshotInternal(database_,
  //                                   query_spec.path.GetChild(change.child_key),
  //                                   change.indexed_variant.variant()));
  return Event(
      kEventTypeValue, this,
      DataSnapshotInternal(database_, change.indexed_variant.variant(),
                           QuerySpec(query_spec.path.GetChild(change.child_key),
                                     change.indexed_variant.query_params())));
}

void ValueEventRegistration::FireEvent(const Event& event) {
  listener_->OnValueChanged(
      DataSnapshot(new DataSnapshotInternal(*event.snapshot)));
}

void ValueEventRegistration::FireCancelEvent(Error error) {
  listener_->OnCancelled(error, GetErrorMessage(error));
}

bool ValueEventRegistration::MatchesListener(const void* listener_ptr) const {
  return static_cast<const void*>(listener_) == listener_ptr;
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
