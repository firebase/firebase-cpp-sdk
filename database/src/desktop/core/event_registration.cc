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

namespace firebase::database::internal {

EventRegistration::~EventRegistration() = default;

void EventRegistration::SafelyFireEvent(const Event& event) {
  // Hold status_mutex_ across the status check and callback execution so that
  // set_status(kRemoved) waits for any in-flight callback to complete and no
  // callback can fire after set_status(kRemoved) returns.
  MutexLock lock(status_mutex_);
  if (status_ == kRemoved) {
    return;
  }

  FireEvent(event);
}

void EventRegistration::SafelyFireCancelEvent(Error error) {
  // Hold status_mutex_ across the status check and callback execution so that
  // set_status(kRemoved) waits for any in-flight callback to complete and no
  // callback can fire after set_status(kRemoved) returns.
  MutexLock lock(status_mutex_);
  if (status_ == kRemoved) {
    return;
  }

  FireCancelEvent(error);
}

}  // namespace firebase::database::internal
