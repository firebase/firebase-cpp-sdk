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

#include "database/src/desktop/core/event_registration.h"

namespace firebase {
namespace database {
namespace internal {

EventRegistration::~EventRegistration() {}

void EventRegistration::SafelyFireEvent(const Event& event) {
  // Ensure that the listener has not already been removed.
  //
  // The main thread may remove listeners at any time, so we should not call any
  // callbacks on a listener that has been removed. This does not protect
  // callbacks that are currently running when the listener is removed: those
  // must handled by a mutex from within the callback.
  if (status_ == kRemoved) {
    return;
  }

  FireEvent(event);
}

void EventRegistration::SafelyFireCancelEvent(Error error) {
  // Ensure that the listener has not already been removed.
  //
  // The main thread may remove listeners at any time, so we should not call any
  // callbacks on a listener that has been removed. This does not protect
  // callbacks that are currently running when the listener is removed: those
  // must handled by a mutex from within the callback.
  if (status_ == kRemoved) {
    return;
  }

  FireCancelEvent(error);
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
