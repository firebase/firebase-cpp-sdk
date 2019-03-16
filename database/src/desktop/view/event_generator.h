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

#ifndef FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_VIEW_EVENT_GENERATOR_H_
#define FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_VIEW_EVENT_GENERATOR_H_

#include <vector>
#include "app/memory/unique_ptr.h"
#include "app/src/path.h"
#include "database/src/common/query_spec.h"
#include "database/src/desktop/core/event_registration.h"
#include "database/src/desktop/view/change.h"
#include "database/src/desktop/view/event.h"

namespace firebase {
namespace database {
namespace internal {

// Generates events from a list of changes and a list of event registrations.
// This will organize which events belong to which registrations and apply the
// appropriate sorting and filtering.
std::vector<Event> GenerateEventsForChanges(
    const QuerySpec& query_spec, const std::vector<Change>& changes,
    const IndexedVariant& event_cache,
    const std::vector<UniquePtr<EventRegistration>>& event_registrations);

}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_VIEW_EVENT_GENERATOR_H_
