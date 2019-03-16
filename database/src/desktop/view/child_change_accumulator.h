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

#ifndef FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_VIEW_CHILD_CHANGE_ACCUMULATOR_H_
#define FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_VIEW_CHILD_CHANGE_ACCUMULATOR_H_

#include <map>
#include <string>
#include "database/src/desktop/view/change.h"

namespace firebase {
namespace database {
namespace internal {

// A container to track all change to the immediate child node by its key.
// This should only store event with type of kTypeChildRemoved, kTypeChildAdded,
// or kTypeChildChanged.
typedef std::map<std::string, Change> ChildChangeAccumulator;

// Track changes at a certain child key..
// If the multiple changes are tracked at the same key, this function resolves
// changes based on the following rules:
// * Change from kTypeChildRemoved to kTypeChildAdded => kTypeChildChanged
// * Change from kTypeChildAdded to kTypeChildRemoved => delete data
// * Change from kTypeChildChanged to kTypeChildRemoved => kTypeChildRemoved
// * Change from kTypeChildAdded to kTypeChildChanged => kTypeChildAdded
// * Change from kTypeChildChanged to kTypeChildChanged => kTypeChildChanged
// * Assert for the other cases
void TrackChildChange(const Change& change,
                      ChildChangeAccumulator* accumulator);

}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_VIEW_CHILD_CHANGE_ACCUMULATOR_H_
