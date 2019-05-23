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

#include "database/src/desktop/view/event_generator.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <vector>

#include "app/memory/unique_ptr.h"
#include "app/src/assert.h"
#include "app/src/variant_util.h"
#include "database/src/common/query_spec.h"
#include "database/src/desktop/core/event_registration.h"
#include "database/src/desktop/query_params_comparator.h"
#include "database/src/desktop/view/change.h"
#include "database/src/desktop/view/event.h"

namespace firebase {
namespace database {
namespace internal {

static void GenerateEventsForType(
    const QuerySpec& query_spec, EventType event_type,
    const std::vector<Change>& changes,
    const std::vector<UniquePtr<EventRegistration>>& event_registrations,
    const IndexedVariant& event_cache, std::vector<Event>* events);

static Event GenerateEvent(const QuerySpec& query_spec, const Change& change,
                           EventRegistration* registration,
                           const IndexedVariant& event_cache);

std::vector<Event> GenerateEventsForChanges(
    const QuerySpec& query_spec, const std::vector<Change>& changes,
    const IndexedVariant& event_cache,
    const std::vector<UniquePtr<EventRegistration>>& event_registrations) {
  std::vector<Event> events;
  std::vector<Change> moves;
  QueryParamsComparator comparator(&query_spec.params);
  for (auto iter = changes.begin(); iter != changes.end(); ++iter) {
    const Change& change = *iter;
    if (change.event_type == kEventTypeChildChanged) {
      const Variant& old_variant = change.old_indexed_variant.variant();
      const Variant& variant = change.indexed_variant.variant();
      bool different =
          comparator.Compare(QueryParamsComparator::kMinKey, old_variant,
                             QueryParamsComparator::kMinKey, variant) != 0;
      if (different) {
        moves.push_back(
            ChildMovedChange(change.child_key, change.indexed_variant));
      }
    }
  }

  GenerateEventsForType(query_spec, kEventTypeChildRemoved, changes,
                        event_registrations, event_cache, &events);
  GenerateEventsForType(query_spec, kEventTypeChildAdded, changes,
                        event_registrations, event_cache, &events);
  GenerateEventsForType(query_spec, kEventTypeChildMoved, moves,
                        event_registrations, event_cache, &events);
  GenerateEventsForType(query_spec, kEventTypeChildChanged, changes,
                        event_registrations, event_cache, &events);
  GenerateEventsForType(query_spec, kEventTypeValue, changes,
                        event_registrations, event_cache, &events);

  return events;
}

class ChangeLesser {
 public:
  ChangeLesser(const QueryParams* query_params) : comparator_(query_params) {}

  bool operator()(const Change* a, const Change* b) const {
    return comparator_.Compare(
               a->child_key.c_str(), a->indexed_variant.variant(),
               b->child_key.c_str(), b->indexed_variant.variant()) < 0;
  }

 private:
  QueryParamsComparator comparator_;
};

void GenerateEventsForType(
    const QuerySpec& query_spec, EventType event_type,
    const std::vector<Change>& changes,
    const std::vector<UniquePtr<EventRegistration>>& event_registrations,
    const IndexedVariant& event_cache, std::vector<Event>* events) {
  std::vector<const Change*> filtered_changes;
  filtered_changes.reserve(changes.size());
  for (auto iter = changes.begin(); iter != changes.end(); ++iter) {
    const Change& change = *iter;
    if (change.event_type != kEventTypeValue) {
      FIREBASE_DEV_ASSERT_MESSAGE(!change.child_key.empty(),
                                  "Child changes must have a child_key");
    }
    if (change.event_type == event_type) {
      filtered_changes.push_back(&change);
    }
  }

  std::sort(filtered_changes.begin(), filtered_changes.end(),
            ChangeLesser(&query_spec.params));

  if (event_type == kEventTypeValue) {
    FIREBASE_DEV_ASSERT_MESSAGE(filtered_changes.size() <= 1,
                                "Value changes must occur one at a time");
  }

  for (auto change_iter = filtered_changes.begin();
       change_iter != filtered_changes.end(); ++change_iter) {
    const Change* change = *change_iter;
    for (auto registration_iter = event_registrations.begin();
         registration_iter != event_registrations.end(); ++registration_iter) {
      const UniquePtr<EventRegistration>& registration = *registration_iter;
      if (registration->RespondsTo(event_type)) {
        events->push_back(GenerateEvent(query_spec, *change, registration.get(),
                                        event_cache));
      }
    }
  }
}

Event GenerateEvent(const QuerySpec& query_spec, const Change& change,
                    EventRegistration* registration,
                    const IndexedVariant& event_cache) {
  if (change.event_type == kEventTypeValue ||
      change.event_type == kEventTypeChildRemoved) {
    return registration->GenerateEvent(change, query_spec);
  } else {
    const char* prev_child_key = event_cache.GetPredecessorChildName(
        change.child_key, change.indexed_variant.variant());
    Change new_change =
        ChangeWithPrevName(change, prev_child_key ? prev_child_key : "");
    return registration->GenerateEvent(new_change, query_spec);
  }
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
