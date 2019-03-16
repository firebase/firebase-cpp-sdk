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

#ifndef FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_CORE_SYNC_POINT_H_
#define FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_CORE_SYNC_POINT_H_

#include <set>
#include <vector>
#include "database/src/common/query_spec.h"
#include "database/src/desktop/core/event_registration.h"
#include "database/src/desktop/core/operation.h"
#include "database/src/desktop/persistence/persistence_manager_interface.h"
#include "database/src/desktop/view/event.h"
#include "database/src/desktop/view/view.h"

namespace firebase {
namespace database {
namespace internal {

// SyncPoint represents a single location in a SyncTree with 1 or more event
// registrations, meaning we need to maintain 1 or more Views at this location
// to cache server data and raise appropriate events for server changes and user
// writes (set, transaction, update).
//
// It's responsible for:
//   - Maintaining the set of 1 or more views necessary at this location (a
//     SyncPoint with 0 views should be removed).
//   - Proxying user / server operations to the views as appropriate (i.e.
//     apply_server_overwrite, apply_user_overwrite, etc.)
class SyncPoint {
 public:
  SyncPoint() = default;

  // Copy that actually performs a move.  This is useful for STL implementations
  // that do not support emplace (e.g stlport).
  SyncPoint(const SyncPoint& other);
  SyncPoint& operator=(const SyncPoint& other);

  // Allow moving.
  SyncPoint(SyncPoint&& other);
  SyncPoint& operator=(SyncPoint&& other);

  // Return true if there are no views on this sync point.
  bool IsEmpty() const;

  // Apply the given operation to the sync point, taking ito account the data in
  // the pending write tree and the server cache.
  std::vector<Event> ApplyOperation(
      const Operation& operation, const WriteTreeRef& writes_cache,
      const Variant* opt_complete_server_cache,
      PersistenceManagerInterface* persistence_manager);

  // Add an event callback for the specified query.
  std::vector<Event> AddEventRegistration(
      UniquePtr<EventRegistration> event_registration,
      const WriteTreeRef& writes_cache, const CacheNode& server_cache,
      PersistenceManagerInterface* persistence_manager);

  // Remove event callback(s). Return cancel_events if a cancel_error is
  // specified.
  //
  // If query is the default query, we'll check all views for the specified
  // event_registration. If event_registration is , we'll remove all callbacks
  // for the specified view(s).
  //
  // If event_registration is null, remove all callbacks.
  // cancel_error If a cancel_error is provided, appropriate cancel events will
  // be returned.
  std::vector<Event> RemoveEventRegistration(
      const QuerySpec& query_spec, void* listener_ptr, Error cancel_error,
      std::vector<QuerySpec>* out_removed);

  // Return the list of views that have and incomplete (i.e. filtered) view of
  // the data.
  std::vector<const View*> GetIncompleteQueryViews() const;

  // Return a variant that represents the complete server cache, if available.
  // If there are no views in this sync point with a complete server cache,
  // returns nullptr.
  const Variant* GetCompleteServerCache(const Path& path) const;

  // Return the pointer to a view that corresponds to the given QuerySpec.
  const View* ViewForQuery(const QuerySpec& query_spec) const;

  // Return whether there is a view that corresponds to the given QuerySpec.
  bool ViewExistsForQuery(const QuerySpec& query_spec) const;

  // Get the pointer to the view that represents the complete view (i.e. the
  // unfiltered view) of this location.
  const View* GetCompleteView() const;

  // Return whether there is a complete view of this location.
  bool HasCompleteView() const;

 private:
  // Apply an operation to a given view of the database, and return the
  // resulting events.
  std::vector<Event> ApplyOperationToView(
      View* view, const Operation& operation, const WriteTreeRef& writes,
      const Variant* opt_complete_server_cache,
      PersistenceManagerInterface* persistence_manager);

  // The Views being tracked at this location in the tree, stored as a map where
  // the key is a QueryParams and the value is the View for that query.
  //
  // NOTE: This list will be quite small (usually 1, but perhaps 2 or 3; any
  // more is an odd use case).
  std::map<QueryParams, View> views_;
};

}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_CORE_SYNC_POINT_H_
