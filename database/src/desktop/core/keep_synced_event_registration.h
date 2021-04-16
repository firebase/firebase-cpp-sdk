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

#ifndef FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_CORE_KEEP_SYNCED_EVENT_REGISTRATION_H_
#define FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_CORE_KEEP_SYNCED_EVENT_REGISTRATION_H_

#include "app/src/path.h"
#include "database/src/common/query_spec.h"
#include "database/src/desktop/core/event_registration.h"
#include "database/src/desktop/view/change.h"
#include "database/src/desktop/view/event.h"
#include "firebase/database/common.h"

namespace firebase {
namespace database {
namespace internal {

class SyncTree;

// A KeepSyncedEventRegistration is a registration that has no listener attached
// to it. It exists to be used as a placeholder registration on SyncPoints so
// that they can keep the databsae data in memory locally.
// KeepSyncedEventRegistrations do not fire events (neither normal events nor
// cancel events).
class KeepSyncedEventRegistration : public EventRegistration {
 public:
  explicit KeepSyncedEventRegistration(SyncTree* sync_tree,
                                       const QuerySpec& query_spec)
      : EventRegistration(query_spec), sync_tree_(sync_tree) {}

  virtual ~KeepSyncedEventRegistration();

  // KeepSyncedEventRegistrations never respond to any events of any type.
  bool RespondsTo(EventType event_type) override;

  // GenerateEvent should only be called when this object RespondsTo the type.
  // Since this is never true for a KeepSyncedEventRegistrations, this function
  // should never be called.
  Event GenerateEvent(const Change& change,
                      const QuerySpec& query_spec) override;

  // Would normally fire an event, but KeepSyncedEventRegistrations don't
  // actually fire events.
  void FireEvent(const Event& event) override;

  // Would normally fire a cancel event, but KeepSyncedEventRegistrations don't
  // actually fire events.
  void FireCancelEvent(Error error) override;

  // Returns true if this EventRegistration contains the given listener.
  // Notes: This takes a void* because ValueListener and ChildListener do not
  // share a common base class.
  bool MatchesListener(const void* listener_ptr) const override;

 private:
  // Instead of a listener, this keeps a pointer to the SyncTree. Since all
  // KeepSyncedEventRegistrations are the same, we just need a pointer that is
  // common to all KeepSyncedEventRegistrations that is guarenteed to not be
  // mistaken for a listener pointer.
  SyncTree* sync_tree_;
};

}  // namespace internal
}  // namespace database
}  // namespace firebase
#endif  // FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_CORE_KEEP_SYNCED_EVENT_REGISTRATION_H_
