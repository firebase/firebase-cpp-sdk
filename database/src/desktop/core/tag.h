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

#ifndef FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_CORE_TAG_H_
#define FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_CORE_TAG_H_

#include <cstdint>

#include "app/src/optional.h"

namespace firebase {
namespace database {
namespace internal {

// Tags are used when sending requests to the server that only ask for a subset
// of the data. Because the server can send back multiple different responses to
// the same location, the tag is used to differentiate which Query at the
// location the response is directed at.
typedef Optional<int64_t> Tag;

}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_CORE_TAG_H_
