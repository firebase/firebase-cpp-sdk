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

#ifndef FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_VIEW_VIEW_H_
#define FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_VIEW_VIEW_H_

#include <vector>
#include "app/memory/unique_ptr.h"
#include "app/src/include/firebase/variant.h"
#include "app/src/optional.h"
#include "app/src/path.h"
#include "database/src/common/query_spec.h"
#include "database/src/desktop/core/event_registration.h"
#include "database/src/desktop/core/indexed_variant.h"
#include "database/src/desktop/core/operation.h"
#include "database/src/desktop/core/write_tree.h"
#include "database/src/desktop/view/event.h"
#include "database/src/desktop/view/event_generator.h"
#include "database/src/desktop/view/view_cache.h"
#include "database/src/desktop/view/view_processor.h"

namespace firebase {
namespace database {
namespace internal {

// A view object represents what the database looks like at a given location
// according to a given queryspec. There can be multiple views per location as
// long as they have a different sent of queryspec parameters sepecified, and
// can thus have different subsets of the data at that location in the database.
class View {
 public:
  View(const QuerySpec& query_spec, const ViewCache& initial_view_cache);

  // Copy that actually performs a move.  This is useful for STL implementations
  // that do not support emplace (e.g stlport).
  View(const View&);
  View& operator=(const View&);

  View(View&& other);
  View& operator=(View&& other);

  ~View();

  // Get the complete server cache at the given path relative to this view.
  // This will return a nullptr if there is no cached data at the given
  // location.
  const Variant* GetCompleteServerCache(const Path& path) const;

  // Returns true if there are no event registrations at this location.
  bool IsEmpty() { return event_registrations_.empty(); }

  // Adds the given registration to the list of registrations this view manages.
  // Adding a registration gives ownership of the registration to the view.
  void AddEventRegistration(UniquePtr<EventRegistration> registration);

  // Removes an EventRegistration given the pointer to its listener. If no
  // listener_ptr is supplied, all registrations are removed.
  std::vector<Event> RemoveEventRegistration(void* listener_ptr,
                                             Error cancel_error);

  // Apply an operation to the view. If available, you may specify a complete
  // server cache, otherwise the operation will only be applied to the data
  // visible to the View.
  // This returns the vector of Changes that were generated, as well as the
  // Events that need to be applied.
  std::vector<Event> ApplyOperation(const Operation& operation,
                                    const WriteTreeRef& writes_cache,
                                    const Variant* opt_complete_server_cache,
                                    std::vector<Change>* out_changes);

  // Get the events that will be fired upon initializing a registration on this
  // View.
  std::vector<Event> GetInitialEvents(EventRegistration* registration);

  // Return the QuerySpec associated this this View of the database.
  const QuerySpec& query_spec() const { return query_spec_; }

  // Return the ViewCache representing the data in this View of the database.
  const ViewCache& view_cache() const { return view_cache_; }

  // The EventRegistrations owned by this View of the database.
  const std::vector<UniquePtr<EventRegistration>>& event_registrations() const {
    return event_registrations_;
  }

  // A convenience function to get the local cache from the ViewCache.
  const Variant& GetLocalCache() const {
    return view_cache_.local_snap().variant();
  }

 private:
  View() = delete;

  // Generate events from a list of changes for an EventRegistration. If no
  // EventRegistration is provided, Events for all EventRegistrations in this
  // View.
  std::vector<Event> GenerateEvents(const std::vector<Change>& changes,
                                    const IndexedVariant& event_cache,
                                    EventRegistration* registration);

  QuerySpec query_spec_;
  UniquePtr<ViewProcessor> view_processor_;
  ViewCache view_cache_;
  std::vector<UniquePtr<EventRegistration>> event_registrations_;
};

}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_VIEW_VIEW_H_
