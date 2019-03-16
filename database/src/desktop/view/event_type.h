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

#ifndef FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_VIEW_EVENT_TYPE_H_
#define FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_VIEW_EVENT_TYPE_H_

namespace firebase {
namespace database {
namespace internal {

// An enumeration of the various kinds of events that can be raised on locations
// in the database.
enum EventType {
  // The order is important here and reflects the order events should be
  // raised in.
  kEventTypeChildRemoved,
  kEventTypeChildAdded,
  kEventTypeChildMoved,
  kEventTypeChildChanged,
  kEventTypeValue,
  kEventTypeError
};

}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_VIEW_EVENT_TYPE_H_
