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

#include "database/src/desktop/core/keep_synced_event_registration.h"

#include "app/src/assert.h"
#include "database/src/common/query_spec.h"
#include "database/src/include/firebase/database/common.h"

namespace firebase {
namespace database {
namespace internal {

KeepSyncedEventRegistration::~KeepSyncedEventRegistration() {}

bool KeepSyncedEventRegistration::RespondsTo(EventType event_type) {
  (void)event_type;
  return false;
}

Event KeepSyncedEventRegistration::GenerateEvent(const Change& change,
                                                 const QuerySpec& query_spec) {
  (void)change;
  (void)query_spec;
  FIREBASE_DEV_ASSERT_MESSAGE(
      false,
      "GenerateEvent should never be reached on KeepSyncedEventRegistration");
  return Event(UniquePtr<EventRegistration>(nullptr), kErrorUnknownError,
               Path());
}

void KeepSyncedEventRegistration::FireEvent(const Event& event) { (void)event; }

void KeepSyncedEventRegistration::FireCancelEvent(Error error) { (void)error; }

bool KeepSyncedEventRegistration::MatchesListener(
    const void* listener_ptr) const {
  return static_cast<const void*>(sync_tree_) == listener_ptr;
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
