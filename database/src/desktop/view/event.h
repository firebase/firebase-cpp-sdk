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

#ifndef FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_VIEW_EVENT_H_
#define FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_VIEW_EVENT_H_

#include <string>
#include "app/memory/unique_ptr.h"
#include "app/src/optional.h"
#include "app/src/path.h"
#include "database/src/desktop/core/event_registration.h"
#include "database/src/desktop/data_snapshot_desktop.h"
#include "database/src/desktop/view/event_type.h"
#include "firebase/database/common.h"

namespace firebase {
namespace database {
namespace internal {
class EventRegistration;

// This class represents an event that has occurred that should eventually be
// acted upon by a listener.
struct Event {
  Event()
      : type(),
        event_registration(),
        snapshot(),
        prev_name(),
        error(),
        path() {}

  Event(EventType _type, EventRegistration* _event_registration,
        const DataSnapshotInternal& _snapshot)
      : type(_type),
        event_registration(_event_registration),
        snapshot(_snapshot),
        prev_name(),
        error(kErrorNone),
        path() {}

  Event(EventType _type, EventRegistration* _event_registration,
        const DataSnapshotInternal& _snapshot, const std::string& _prev_name)
      : type(_type),
        event_registration(_event_registration),
        snapshot(_snapshot),
        prev_name(_prev_name),
        error(kErrorNone),
        path() {}

  Event(UniquePtr<EventRegistration> _event_registration, Error _error,
        const Path& _path)
      : type(kEventTypeError),
        event_registration(_event_registration.get()),
        snapshot(),
        prev_name(),
        error(_error),
        path(_path),
        event_registration_ownership_ptr() {
    event_registration_ownership_ptr = std::move(_event_registration);
  }

  // The type of the event.
  EventType type;

  // The event registration that this event is associated with.
  EventRegistration* event_registration;

  // The snapshot that should be fed to the event registration.
  Optional<DataSnapshotInternal> snapshot;

  // The previous location name of this event, for use with ChildMoved events.
  std::string prev_name;

  // The error value, if this event represents an error.
  Error error;

  // The path associated with this error.
  Path path;

  // If the Event is a cancel event, the event registration is removed from the
  // View it is attached to. Since they are stored in UniquePtrs this would
  // normally mean they get deallocated when they're removed, but we need it to
  // live long enough to fire the Event. So instead, the event takes ownership
  // of the pointer.
  //
  // This field is not used to access any data, it's only used
  // to take ownership. To keep the code streamlined, any time the pointer is
  // needed, owned or not, it will go through the event_registration field
  // above.
  UniquePtr<EventRegistration> event_registration_ownership_ptr;
};

inline bool operator==(const Event& lhs, const Event& rhs) {
  return lhs.type == rhs.type &&
         lhs.event_registration == rhs.event_registration &&
         lhs.snapshot == rhs.snapshot && lhs.prev_name == rhs.prev_name &&
         lhs.error == rhs.error && lhs.path == rhs.path;
}
inline bool operator!=(const Event& lhs, const Event& rhs) {
  return !(lhs == rhs);
}

}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_VIEW_EVENT_H_
