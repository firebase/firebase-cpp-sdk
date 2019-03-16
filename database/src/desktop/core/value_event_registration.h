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

#ifndef FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_CORE_VALUE_EVENT_REGISTRATION_H_
#define FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_CORE_VALUE_EVENT_REGISTRATION_H_

#include "app/src/path.h"
#include "database/src/common/query_spec.h"
#include "database/src/desktop/core/event_registration.h"
#include "database/src/desktop/view/event.h"
#include "database/src/include/firebase/database/common.h"
#include "database/src/include/firebase/database/listener.h"

namespace firebase {
namespace database {
namespace internal {

class DatabaseInternal;

class ValueEventRegistration : public EventRegistration {
 public:
  ValueEventRegistration(DatabaseInternal* database, ValueListener* listener,
                         const QuerySpec& query_spec)
      : EventRegistration(query_spec),
        database_(database),
        listener_(listener) {}

  ~ValueEventRegistration() override;

  bool RespondsTo(EventType event_type) override;

  Event GenerateEvent(const Change& change,
                      const QuerySpec& query_spec) override;

  void FireEvent(const Event& event) override;

  void FireCancelEvent(Error error) override;

  bool MatchesListener(const void* listener_ptr) const override;

 private:
  DatabaseInternal* database_;
  ValueListener* listener_;
};

}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_CORE_VALUE_EVENT_REGISTRATION_H_
