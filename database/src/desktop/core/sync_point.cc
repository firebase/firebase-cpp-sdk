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

#include "database/src/desktop/core/sync_point.h"
#include <set>
#include <vector>
#include "database/src/common/query_spec.h"
#include "database/src/desktop/core/event_registration.h"
#include "database/src/desktop/core/operation.h"
#include "database/src/desktop/persistence/persistence_manager.h"
#include "database/src/desktop/util_desktop.h"
#include "database/src/desktop/view/event.h"
#include "database/src/desktop/view/view.h"
#include "database/src/include/firebase/database/common.h"

#include "app/src/log.h"

namespace firebase {
namespace database {
namespace internal {

SyncPoint::SyncPoint(const SyncPoint& other)
    : views_(std::move(const_cast<SyncPoint*>(&other)->views_)) {}

SyncPoint& SyncPoint::operator=(const SyncPoint& other) {
  views_ = std::move(const_cast<SyncPoint*>(&other)->views_);
  return *this;
}

SyncPoint::SyncPoint(SyncPoint&& other) : views_(std::move(other.views_)) {}

SyncPoint& SyncPoint::operator=(SyncPoint&& other) {
  views_ = std::move(other.views_);
  return *this;
}

bool SyncPoint::IsEmpty() const { return views_.empty(); }

std::vector<Event> SyncPoint::ApplyOperation(
    const Operation& operation, const WriteTreeRef& writes_cache,
    const Variant* opt_complete_server_cache,
    PersistenceManagerInterface* persistence_manager) {
  const Optional<QueryParams>& query_params = operation.source.query_params;
  if (query_params.has_value()) {
    auto iter = views_.find(*query_params);
    assert(iter != views_.end());
    return ApplyOperationToView(&iter->second, operation, writes_cache,
                                opt_complete_server_cache, persistence_manager);
  } else {
    std::vector<Event> result;
    for (auto& query_spec_view_pair : views_) {
      View& view = query_spec_view_pair.second;
      Extend(&result, ApplyOperationToView(&view, operation, writes_cache,
                                           opt_complete_server_cache,
                                           persistence_manager));
    }
    return result;
  }
}

std::vector<Event> SyncPoint::AddEventRegistration(
    UniquePtr<EventRegistration> event_registration,
    const WriteTreeRef& writes_cache, const CacheNode& server_cache,
    PersistenceManagerInterface* persistence_manager) {
  const QuerySpec& query_spec = event_registration->query_spec();
  View* view = MapGet(&views_, query_spec.params);
  if (view == nullptr) {
    Optional<Variant> event_cache =
        writes_cache.CalcCompleteEventCache(server_cache.GetCompleteSnap());
    bool event_cache_complete;
    if (event_cache.has_value()) {
      event_cache_complete = true;
    } else {
      event_cache =
          writes_cache.CalcCompleteEventChildren(server_cache.variant());
      event_cache_complete = false;
    }
    IndexedVariant indexed(*event_cache, query_spec.params);
    ViewCache view_cache(CacheNode(indexed, event_cache_complete, false),
                         server_cache);
    auto iter = views_.insert(
        std::make_pair(query_spec.params, View(query_spec, view_cache)));
    view = &iter.first->second;

    // If this is a non-default query we need to tell persistence our current
    // view of the data.
    if (!QuerySpecLoadsAllData(query_spec)) {
      std::set<std::string> all_children;
      const Variant* value = GetVariantValue(&view->GetLocalCache());
      if (value->is_map()) {
        for (const auto& key_value_pair : value->map()) {
          all_children.insert(key_value_pair.first.string_value());
        }
      }
      persistence_manager->SetTrackedQueryKeys(query_spec, all_children);
    }
  }

  // The view is guaranteed to exist now.
  std::vector<Event> results = view->GetInitialEvents(event_registration.get());
  view->AddEventRegistration(Move(event_registration));
  return results;
}

std::vector<Event> SyncPoint::RemoveEventRegistration(
    const QuerySpec& query_spec, void* listener_ptr, Error cancel_error,
    std::vector<QuerySpec>* out_removed) {
  std::vector<Event> cancel_events;

  bool had_complete_view = HasCompleteView();
  if (QuerySpecIsDefault(query_spec)) {
    for (auto iter = views_.begin(); iter != views_.end();) {
      View& view = iter->second;
      Extend(&cancel_events,
             view.RemoveEventRegistration(listener_ptr, cancel_error));
      if (view.IsEmpty()) {
        // We'll deal with complete views later.
        if (!QuerySpecLoadsAllData(view.query_spec())) {
          out_removed->push_back(view.query_spec());
        }
        iter = views_.erase(iter);
      } else {
        ++iter;
      }
    }
  } else {
    // Remove the callback from the specific view.
    View* view = MapGet(&views_, query_spec.params);
    if (view) {
      Extend(&cancel_events,
             view->RemoveEventRegistration(listener_ptr, cancel_error));
      if (view->IsEmpty()) {
        // We'll deal with complete views later.
        if (!QuerySpecLoadsAllData(query_spec)) {
          out_removed->push_back(query_spec);
        }
        views_.erase(query_spec.params);
      }
    }
  }

  if (had_complete_view && !HasCompleteView()) {
    // We removed our last complete view.
    out_removed->push_back(MakeDefaultQuerySpec(query_spec));
  }
  return cancel_events;
}

std::vector<const View*> SyncPoint::GetIncompleteQueryViews() const {
  std::vector<const View*> result;
  for (auto& query_spec_view_pair : views_) {
    const View& view = query_spec_view_pair.second;
    if (!QuerySpecLoadsAllData(view.query_spec())) {
      result.push_back(&view);
    }
  }
  return result;
}

const Variant* SyncPoint::GetCompleteServerCache(const Path& path) const {
  for (auto& query_spec_view_pair : views_) {
    const View& view = query_spec_view_pair.second;
    const Variant* result = view.GetCompleteServerCache(path);
    if (result != nullptr) {
      return result;
    }
  }
  return nullptr;
}

const View* SyncPoint::ViewForQuery(const QuerySpec& query_spec) const {
  if (QuerySpecLoadsAllData(query_spec)) {
    return GetCompleteView();
  } else {
    return MapGet(&views_, query_spec.params);
  }
}

bool SyncPoint::ViewExistsForQuery(const QuerySpec& query_spec) const {
  return ViewForQuery(query_spec) != nullptr;
}

bool SyncPoint::HasCompleteView() const { return GetCompleteView() != nullptr; }

const View* SyncPoint::GetCompleteView() const {
  for (auto& query_spec_view_pair : views_) {
    const View& view = query_spec_view_pair.second;
    if (QuerySpecLoadsAllData(view.query_spec())) {
      return &view;
    }
  }
  return nullptr;
}

std::vector<Event> SyncPoint::ApplyOperationToView(
    View* view, const Operation& operation, const WriteTreeRef& writes,
    const Variant* opt_complete_server_cache,
    PersistenceManagerInterface* persistence_manager) {
  std::vector<Change> changes;
  std::vector<Event> events = view->ApplyOperation(
      operation, writes, opt_complete_server_cache, &changes);
  // Not a default query, track active children
  if (!QuerySpecLoadsAllData(view->query_spec())) {
    std::set<std::string> added;
    std::set<std::string> removed;
    for (const Change& change : changes) {
      if (change.event_type == kEventTypeChildAdded) {
        added.insert(change.child_key);
      } else if (change.event_type == kEventTypeChildRemoved) {
        removed.insert(change.child_key);
      }
    }
    if (!added.empty() || !removed.empty()) {
      persistence_manager->UpdateTrackedQueryKeys(view->query_spec(), added,
                                                  removed);
    }
  }
  return events;
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
