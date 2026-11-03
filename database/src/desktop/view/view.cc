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

#include "database/src/desktop/view/view.h"

#include <algorithm>
#include <cassert>
#include <memory>
#include <utility>
#include <vector>

#include "app/src/assert.h"
#include "app/src/include/firebase/variant.h"
#include "app/src/path.h"
#include "database/src/common/query_spec.h"
#include "database/src/desktop/core/event_registration.h"
#include "database/src/desktop/core/indexed_variant.h"
#include "database/src/desktop/core/operation.h"
#include "database/src/desktop/query_params_comparator.h"
#include "database/src/desktop/util_desktop.h"
#include "database/src/desktop/view/change.h"
#include "database/src/desktop/view/event.h"
#include "database/src/desktop/view/event_generator.h"
#include "database/src/desktop/view/indexed_filter.h"
#include "database/src/desktop/view/variant_filter.h"

namespace firebase {
namespace database {
namespace internal {

namespace {

void GenerateEventsForSingleRegistrationAndType(
    const QuerySpec& query_spec, EventType event_type,
    const std::vector<Change>& changes, EventRegistration* registration,
    const IndexedVariant& event_cache, std::vector<Event>* events) {
  if (!registration->RespondsTo(event_type)) {
    return;
  }

  std::vector<const Change*> filtered_changes;
  filtered_changes.reserve(changes.size());
  for (const Change& change : changes) {
    if (change.event_type != kEventTypeValue) {
      FIREBASE_DEV_ASSERT_MESSAGE(!change.child_key.empty(),
                                  "Child changes must have a child_key");
    }
    if (change.event_type == event_type) {
      filtered_changes.push_back(&change);
    }
  }
  if (filtered_changes.empty()) {
    return;
  }

  QueryParamsComparator comparator(&query_spec.params);
  std::sort(filtered_changes.begin(), filtered_changes.end(),
            [&comparator](const Change* a, const Change* b) {
              return comparator.Compare(
                         a->child_key.c_str(), a->indexed_variant.variant(),
                         b->child_key.c_str(), b->indexed_variant.variant()) <
                     0;
            });

  if (event_type == kEventTypeValue) {
    FIREBASE_DEV_ASSERT_MESSAGE(filtered_changes.size() <= 1,
                                "Value changes must occur one at a time");
  }

  for (const Change* change : filtered_changes) {
    if (change->event_type == kEventTypeValue ||
        change->event_type == kEventTypeChildRemoved) {
      events->push_back(registration->GenerateEvent(*change, query_spec));
    } else {
      const char* prev_child_key = event_cache.GetPredecessorChildName(
          change->child_key, change->indexed_variant.variant());
      Change new_change =
          ChangeWithPrevName(*change, prev_child_key ? prev_child_key : "");
      events->push_back(registration->GenerateEvent(new_change, query_spec));
    }
  }
}

}  // namespace

View::View(const QuerySpec& query_spec, const ViewCache& initial_view_cache)
    : query_spec_(query_spec) {
  IndexedFilter index_filter(query_spec.params);
  std::unique_ptr<VariantFilter> filter =
      VariantFilterFromQueryParams(query_spec.params);
  const CacheNode& initial_server_cache = initial_view_cache.server_snap();
  const CacheNode& initial_event_cache = initial_view_cache.local_snap();

  // Don't filter server node with other filter than index, wait for tagged
  // listen.
  IndexedVariant empty_indexed_variant(Variant::Null(), query_spec.params);

  IndexedVariant server_snap = index_filter.UpdateFullVariant(
      empty_indexed_variant, initial_server_cache.indexed_variant(), nullptr);
  IndexedVariant local_snap = filter->UpdateFullVariant(
      empty_indexed_variant, initial_event_cache.indexed_variant(), nullptr);

  CacheNode new_server_cache(server_snap,
                             initial_server_cache.fully_initialized(),
                             index_filter.FiltersVariants());
  CacheNode new_event_cache(local_snap, initial_event_cache.fully_initialized(),
                            filter->FiltersVariants());

  view_cache_ = ViewCache(new_event_cache, new_server_cache);

  view_processor_ = std::make_unique<ViewProcessor>(std::move(filter));
}

View::View(const View& other)
    : query_spec_(other.query_spec_),
      view_processor_(std::make_unique<ViewProcessor>(
          VariantFilterFromQueryParams(other.query_spec_.params))),
      view_cache_(other.view_cache_),
      event_registrations_() {}

View& View::operator=(const View& other) {
  if (&other == this) return *this;
  query_spec_ = other.query_spec_;
  view_processor_ = std::make_unique<ViewProcessor>(
      VariantFilterFromQueryParams(other.query_spec_.params));
  view_cache_ = other.view_cache_;
  event_registrations_.clear();
  return *this;
}

View::View(View&& other) noexcept
    : query_spec_(std::move(other.query_spec_)),
      view_processor_(std::move(other.view_processor_)),
      view_cache_(std::move(other.view_cache_)),
      event_registrations_(std::move(other.event_registrations_)) {}

View& View::operator=(View&& other) noexcept {
  if (&other == this) return *this;
  query_spec_ = std::move(other.query_spec_);
  view_processor_ = std::move(other.view_processor_);
  view_cache_ = std::move(other.view_cache_);
  event_registrations_ = std::move(other.event_registrations_);
  return *this;
}

View::~View() = default;

const Variant* View::GetCompleteServerCache(const Path& path) const {
  const Variant* snap = view_cache_.GetCompleteServerSnap();
  if (snap != nullptr) {
    // If this isn't a "LoadsAllData" view, then cache isn't actually a complete
    // cache and we need to see if it contains the child we're interested in.
    if (QueryParamsLoadsAllData(query_spec_.params) || !path.empty()) {
      return GetInternalVariant(snap, path);
    }
  }
  return nullptr;
}

void View::AddEventRegistration(
    std::unique_ptr<EventRegistration> registration) {
  event_registrations_.emplace_back(std::move(registration));
}

std::vector<Event> View::RemoveEventRegistration(void* listener_ptr,
                                                 Error cancel_error) {
  // If there was an error, clear out all the registrations and generate the
  // proper events for each one.
  if (cancel_error != kErrorNone) {
    FIREBASE_DEV_ASSERT_MESSAGE(
        listener_ptr == nullptr,
        "A cancel should cancel all event registrations");
    std::vector<Event> cancel_events;
    Path path(query_spec_.path);
    for (auto iter = event_registrations_.begin();
         iter != event_registrations_.end(); ++iter) {
      std::unique_ptr<EventRegistration>& event_registration = *iter;
      cancel_events.push_back(
          Event(std::move(event_registration), cancel_error, path));
    }
    event_registrations_.clear();
    return cancel_events;
  }

  if (listener_ptr) {
    // If a specific listener is being removed, just find remove the one.
    for (auto iter = event_registrations_.begin();
         iter != event_registrations_.end(); ++iter) {
      std::unique_ptr<EventRegistration>& event_registration = *iter;
      if (event_registration->MatchesListener(listener_ptr)) {
        event_registrations_.erase(iter);
        break;
      }
    }
  } else {
    // If no specific listener was specified, remove all event registrations.
    event_registrations_.clear();
  }
  return std::vector<Event>();
}

std::vector<Event> View::ApplyOperation(
    const Operation& operation, const WriteTreeRef& writes_cache,
    const Variant* opt_complete_server_cache,
    std::vector<Change>* out_changes) {
  FIREBASE_DEV_ASSERT(out_changes != nullptr);
  if (operation.type == Operation::kTypeMerge &&
      operation.source.query_params.has_value()) {
    FIREBASE_DEV_ASSERT_MESSAGE(
        view_cache_.GetCompleteServerSnap() != nullptr,
        "We should always have a full cache before handling merges");
    FIREBASE_DEV_ASSERT_MESSAGE(
        view_cache_.GetCompleteLocalSnap() != nullptr,
        "Missing event cache, even though we have a server cache");
  }

  ViewCache old_view_cache = view_cache_;
  view_processor_->ApplyOperation(old_view_cache, operation, writes_cache,
                                  opt_complete_server_cache, &view_cache_,
                                  out_changes);

  FIREBASE_DEV_ASSERT_MESSAGE(
      (view_cache_.server_snap().fully_initialized() ||
       !old_view_cache.server_snap().fully_initialized()),
      "Once a server snap is complete, it should never go back");

  return GenerateEvents(*out_changes,
                        view_cache_.local_snap().indexed_variant(), nullptr);
}

std::vector<Event> View::GetInitialEvents(EventRegistration* registration) {
  const CacheNode& local_snap = view_cache_.local_snap();
  std::vector<Change> initial_changes;
  for (auto& child : local_snap.indexed_variant().index()) {
    Variant child_key = child.first.AsString();
    initial_changes.push_back(
        ChildAddedChange(child_key.string_value(), child.second));
  }
  if (local_snap.fully_initialized()) {
    initial_changes.push_back(ValueChange(local_snap.indexed_variant()));
  }
  return GenerateEvents(initial_changes, local_snap.indexed_variant(),
                        registration);
}

std::vector<Event> View::GenerateEvents(const std::vector<Change>& changes,
                                        const IndexedVariant& event_cache,
                                        EventRegistration* registration) {
  // If no single EventRegistration was specified, generate events across all
  // registrations owned by this View.
  if (registration == nullptr) {
    return GenerateEventsForChanges(query_spec_, changes, event_cache,
                                    event_registrations_);
  }

  std::vector<Event> events;
  std::vector<Change> moves;
  QueryParamsComparator comparator(&query_spec_.params);
  for (const Change& change : changes) {
    if (change.event_type == kEventTypeChildChanged) {
      const Variant& old_variant = change.old_indexed_variant.variant();
      const Variant& variant = change.indexed_variant.variant();
      if (comparator.Compare(QueryParamsComparator::kMinKey, old_variant,
                             QueryParamsComparator::kMinKey, variant) != 0) {
        moves.push_back(
            ChildMovedChange(change.child_key, change.indexed_variant));
      }
    }
  }

  GenerateEventsForSingleRegistrationAndType(
      query_spec_, kEventTypeChildRemoved, changes, registration, event_cache,
      &events);
  GenerateEventsForSingleRegistrationAndType(query_spec_, kEventTypeChildAdded,
                                             changes, registration, event_cache,
                                             &events);
  GenerateEventsForSingleRegistrationAndType(query_spec_, kEventTypeChildMoved,
                                             moves, registration, event_cache,
                                             &events);
  GenerateEventsForSingleRegistrationAndType(
      query_spec_, kEventTypeChildChanged, changes, registration, event_cache,
      &events);
  GenerateEventsForSingleRegistrationAndType(query_spec_, kEventTypeValue,
                                             changes, registration, event_cache,
                                             &events);
  return events;
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
