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
#include <vector>
#include "app/memory/unique_ptr.h"
#include "app/src/assert.h"
#include "app/src/include/firebase/variant.h"
#include "app/src/path.h"
#include "database/src/common/query_spec.h"
#include "database/src/desktop/core/event_registration.h"
#include "database/src/desktop/core/indexed_variant.h"
#include "database/src/desktop/core/operation.h"
#include "database/src/desktop/util_desktop.h"
#include "database/src/desktop/view/event.h"
#include "database/src/desktop/view/event_generator.h"
#include "database/src/desktop/view/indexed_filter.h"
#include "database/src/desktop/view/variant_filter.h"

namespace firebase {
namespace database {
namespace internal {

View::View(const QuerySpec& query_spec, const ViewCache& initial_view_cache)
    : query_spec_(query_spec) {
  IndexedFilter index_filter(query_spec.params);
  UniquePtr<VariantFilter> filter =
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

  view_processor_ = MakeUnique<ViewProcessor>(std::move(filter));
}

View::View(const View& other)
    : query_spec_(std::move(const_cast<View*>(&other)->query_spec_)),
      view_processor_(const_cast<View*>(&other)->view_processor_),
      view_cache_(std::move(const_cast<View*>(&other)->view_cache_)),
      event_registrations_(
          std::move(const_cast<View*>(&other)->event_registrations_)) {}

View& View::operator=(const View& other) {
  query_spec_ = std::move(const_cast<View*>(&other)->query_spec_);
  view_processor_ = std::move(const_cast<View*>(&other)->view_processor_);
  view_cache_ = std::move(const_cast<View*>(&other)->view_cache_);
  event_registrations_ =
      std::move(const_cast<View*>(&other)->event_registrations_);
  return *this;
}

View::View(View&& other)
    : query_spec_(std::move(other.query_spec_)),
      view_processor_(other.view_processor_),
      view_cache_(std::move(other.view_cache_)),
      event_registrations_(std::move(other.event_registrations_)) {}

View& View::operator=(View&& other) {
  query_spec_ = std::move(other.query_spec_);
  view_processor_ = std::move(other.view_processor_);
  view_cache_ = std::move(other.view_cache_);
  event_registrations_ = std::move(other.event_registrations_);
  return *this;
}

View::~View() {}

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

void View::AddEventRegistration(UniquePtr<EventRegistration> registration) {
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
      UniquePtr<EventRegistration>& event_registration = *iter;
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
      UniquePtr<EventRegistration>& event_registration = *iter;
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
  if (operation.type == Operation::kTypeMerge &&
      !operation.source.query_params.has_value()) {
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
    initial_changes.push_back(
        ChildAddedChange(child.first.AsString().string_value(), child.second));
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
  // If we have a single event registration, we generate the events for just
  // that registration. If the registration is null, we instead use all the
  // registrations on this view.
  //
  // In the case where we're passing along a single registration, we have to
  // manually set up a temporary vector to hold the registration, making sure to
  // release it at the end without deallocating it.
  std::vector<UniquePtr<EventRegistration>> temp_registrations;
  if (registration) {
    temp_registrations.emplace_back(registration);
  }

  // Decide if we're using the individual registration that was passed in, or
  // the entire list of registrations.
  std::vector<UniquePtr<EventRegistration>>& registrations =
      registration ? temp_registrations : event_registrations_;

  std::vector<Event> results = GenerateEventsForChanges(
      query_spec_, changes, event_cache, registrations);

  // Clean up the temporary vector we filled in initially.
  for (auto temp_registration : temp_registrations) {
    temp_registration.release();
  }
  return results;
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
