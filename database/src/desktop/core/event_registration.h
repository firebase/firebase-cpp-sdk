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

#ifndef FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_CORE_EVENT_REGISTRATION_H_
#define FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_CORE_EVENT_REGISTRATION_H_

#include "app/src/path.h"
#include "database/src/common/query_spec.h"
#include "database/src/desktop/view/change.h"
#include "database/src/desktop/view/event_type.h"
#include "firebase/database/common.h"

namespace firebase {
namespace database {
namespace internal {

struct Event;

// An EventRegistration is an abstract class that can contain any kind of event
// listener, even no listener at all. Any time a change is made the change will
// be passed to all event registrations at that location to see which if any
// should respond to it, and if so, it will generate an appropriate event that
// can be used to fire the event later.
class EventRegistration {
 public:
  explicit EventRegistration(const QuerySpec& query_spec)
      : query_spec_(query_spec) {}

  virtual ~EventRegistration();

  // Determine if this event registration should respond to the given event
  // type.
  virtual bool RespondsTo(EventType event_type) = 0;

  // Create an event that we can later respond to with FireEvent.
  virtual Event GenerateEvent(const Change& change,
                              const QuerySpec& query_spec) = 0;

  // Execute the event. Generally this means consuming the Event data and using
  // it to trigger a Listener.
  virtual void FireEvent(const Event& event) = 0;

  // Cancel the event, passing along the given error code.
  virtual void FireCancelEvent(Error error) = 0;

  // Returns true if this EventRegistration contains the given listener.
  // Notes: This takes a void* because ValueListener and ChildListener do not
  // share a common base class.
  virtual bool MatchesListener(const void* listener_ptr) const = 0;

  const QuerySpec& query_spec() const { return query_spec_; }

  bool is_user_initiated() { return is_user_initiated_; }

  void set_is_user_initiated(bool is_user_initiated) {
    is_user_initiated_ = is_user_initiated;
  }

 private:
  QuerySpec query_spec_;

  bool is_user_initiated_;
};

}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_CORE_EVENT_REGISTRATION_H_
