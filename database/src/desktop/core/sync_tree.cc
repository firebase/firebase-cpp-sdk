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

#include "database/src/desktop/core/sync_tree.h"

#include <vector>

#include "app/memory/unique_ptr.h"
#include "app/src/assert.h"
#include "app/src/callback.h"
#include "app/src/include/firebase/variant.h"
#include "app/src/log.h"
#include "app/src/optional.h"
#include "app/src/path.h"
#include "app/src/variant_util.h"
#include "database/src/common/query_spec.h"
#include "database/src/desktop/core/event_registration.h"
#include "database/src/desktop/core/keep_synced_event_registration.h"
#include "database/src/desktop/core/listen_provider.h"
#include "database/src/desktop/core/server_values.h"
#include "database/src/desktop/core/sync_point.h"
#include "database/src/desktop/core/tag.h"
#include "database/src/desktop/core/tree.h"
#include "database/src/desktop/util_desktop.h"
#include "database/src/desktop/view/event.h"
#include "firebase/database/common.h"

namespace firebase {

using callback::NewCallback;

namespace database {
namespace internal {

bool SyncTree::IsEmpty() { return sync_point_tree_.IsEmpty(); }

std::vector<Event> SyncTree::AckUserWrite(WriteId write_id, AckStatus revert,
                                          Persist persist,
                                          int64_t server_time_offset) {
  std::vector<Event> results;
  persistence_manager_->RunInTransaction([&, this]() -> bool {
    if (persist) {
      this->persistence_manager_->RemoveUserWrite(write_id);
    }
    // Make a copy of the write, as it is about to be deleted.
    UserWriteRecord write = *pending_write_tree_->GetWrite(write_id);
    bool need_to_reevaluate = pending_write_tree_->RemoveWrite(write_id);
    if (write.visible) {
      if (!revert) {
        // This is useful to predict what the server value will be at any given
        // time. However, if a server value like {".sv": "timestamp"} is sent up
        // the server will still resolve that to the appropriate value (in this
        // case, the server timestamp).
        Variant server_values = GenerateServerValues(server_time_offset);
        if (write.is_overwrite) {
          Variant resolved_variant =
              ResolveDeferredValueSnapshot(write.overwrite, server_values);
          this->persistence_manager_->ApplyUserWriteToServerCache(
              write.path, resolved_variant);
        } else {
          CompoundWrite resolved_merge =
              ResolveDeferredValueMerge(write.merge, server_values);
          this->persistence_manager_->ApplyUserWriteToServerCache(
              write.path, resolved_merge);
        }
      }
    }
    if (!need_to_reevaluate) {
      results = std::vector<Event>();
      return true;
    } else {
      Tree<bool> affected_tree;
      if (write.is_overwrite) {
        affected_tree.SetValueAt(Path(), true);
      } else {
        for (const auto& entry : write.merge.ChildCompoundWrites()) {
          affected_tree.SetValueAt(Path(entry.first), true);
        }
      }
      results = ApplyOperationToSyncPoints(
          Operation::AckUserWrite(write.path, affected_tree, revert));
      return true;
    }
  });
  return results;
}

std::vector<Event> SyncTree::AddEventRegistration(
    UniquePtr<EventRegistration> event_registration) {
  std::vector<Event> events;

  persistence_manager_->RunInTransaction([&, this]() -> bool {
    const QuerySpec& query_spec = event_registration->query_spec();
    const Path& path = query_spec.path;
    const QueryParams& params = query_spec.params;

    Optional<Variant> server_cache_variant;
    bool found_ancestor_default_view = false;
    // Any covering writes will necessarily be at the root, so really all we
    // need to find is the server cache.
    {
      std::vector<std::string> directories = path.GetDirectories();
      Tree<SyncPoint>* tree = &sync_point_tree_;
      for (auto iter = directories.begin(); tree != nullptr; ++iter) {
        Optional<SyncPoint>& current_sync_point = tree->value();
        if (current_sync_point.has_value()) {
          if (!server_cache_variant.has_value()) {
            server_cache_variant =
                OptionalFromPointer(current_sync_point->GetCompleteServerCache(
                    Path(iter, directories.end())));
          }
          found_ancestor_default_view |= current_sync_point->HasCompleteView();
        }
        if (iter == directories.end()) break;
        tree = tree->GetChild(*iter);
      }
    }

    // Get the SyncPoint.
    SyncPoint* sync_point = sync_point_tree_.GetValueAt(path);
    if (sync_point) {
      // Found a SyncPoint.
      found_ancestor_default_view |= sync_point->HasCompleteView();
      if (!server_cache_variant.has_value()) {
        server_cache_variant =
            OptionalFromPointer(sync_point->GetCompleteServerCache(Path()));
      }
    } else {
      Optional<SyncPoint>& result =
          sync_point_tree_.SetValueAt(path, SyncPoint{});
      sync_point = &result.value();
    }

    persistence_manager_->SetQueryActive(query_spec);

    // Generate a server CacheNode. If we had a complete server cache, just use
    // that to populate it. If we didn't have a complete server cache, we're
    // going to need to build it up from what data we do have.
    CacheNode server_cache;
    if (server_cache_variant.has_value()) {
      server_cache =
          CacheNode(IndexedVariant(*server_cache_variant, params), true, false);
    } else {
      // Hit persistence
      CacheNode persistent_server_cache =
          persistence_manager_->ServerCache(query_spec);
      if (persistent_server_cache.fully_initialized()) {
        server_cache = persistent_server_cache;
      } else {
        server_cache_variant = Variant::Null();
        Tree<SyncPoint>* subtree = sync_point_tree_.GetChild(path);
        for (auto& path_subtree_pair : subtree->children()) {
          Path key(path_subtree_pair.first);
          Tree<SyncPoint>& child_subtree = path_subtree_pair.second;
          Optional<SyncPoint>& child_sync_point = child_subtree.value();
          if (child_sync_point.has_value()) {
            const Variant* complete_cache =
                child_sync_point->GetCompleteServerCache(Path());
            if (complete_cache) {
              SetVariantAtPath(&*server_cache_variant, key, *complete_cache);
            }
          }
        }
        // Fill the node with any available children we have.
        auto& variant = persistent_server_cache.indexed_variant().variant();
        if (variant.is_map()) {
          for (auto& key_value_pair : variant.map()) {
            const char* key = key_value_pair.first.AsString().string_value();
            const Variant& value = key_value_pair.second;
            if (GetInternalVariant(&*server_cache_variant, key) == nullptr) {
              VariantUpdateChild(&server_cache_variant.value(), key, value);
            }
          }
        }
        server_cache = CacheNode(IndexedVariant(*server_cache_variant, params),
                                 false, false);
      }
    }

    // Now that we have the sync point, see if there is an existing view of the
    // database, and if there isn't, then set one up.
    bool view_already_exists = sync_point->ViewExistsForQuery(query_spec);
    if (!view_already_exists && !QuerySpecLoadsAllData(query_spec)) {
      // We need to track a tag for this query
      FIREBASE_DEV_ASSERT_MESSAGE(!query_spec_to_tag_map_.count(query_spec),
                                  "View does not exist but we have a tag");
      Tag tag = GetNextQueryTag();
      query_spec_to_tag_map_[query_spec] = tag;
      tag_to_query_spec_map_[*tag] = query_spec;
    }
    WriteTreeRef writes_cache = pending_write_tree_->ChildWrites(path);
    events = sync_point->AddEventRegistration(Move(event_registration),
                                              writes_cache, server_cache,
                                              persistence_manager_.get());
    if (!view_already_exists && !found_ancestor_default_view) {
      const View* view = sync_point->ViewForQuery(query_spec);
      SetupListener(query_spec, view);
    }
    return true;
  });
  return events;
}

// Apply a listen complete to a path.
std::vector<Event> SyncTree::ApplyTaggedListenComplete(const Tag& tag) {
  std::vector<Event> results;
  persistence_manager_->RunInTransaction([&, this]() -> bool {
    const QuerySpec* query_spec = this->QuerySpecForTag(tag);
    if (query_spec != nullptr) {
      this->persistence_manager_->SetQueryComplete(*query_spec);
      Operation op = Operation::ListenComplete(
          OperationSource::ForServerTaggedQuery(query_spec->params), Path());
      results = this->ApplyTaggedOperation(*query_spec, op);
      return true;
    } else {
      // We've already removed the query. No big deal, ignore the update
      return true;
    }
  });
  return results;
}

std::vector<Event> SyncTree::ApplyTaggedOperation(const QuerySpec& query_spec,
                                                  const Operation& operation) {
  const Path& query_path = query_spec.path;
  SyncPoint* sync_point = sync_point_tree_.GetValueAt(query_path);
  FIREBASE_DEV_ASSERT_MESSAGE(
      sync_point != nullptr,
      "Missing sync point for query tag that we're tracking");
  WriteTreeRef writes_cache = pending_write_tree_->ChildWrites(query_path);
  return sync_point->ApplyOperation(operation, writes_cache, nullptr,
                                    persistence_manager_.get());
}

// Apply new server data for the specified tagged query.
std::vector<Event> SyncTree::ApplyTaggedQueryOverwrite(const Path& path,
                                                       const Variant& snap,
                                                       const Tag& tag) {
  std::vector<Event> results;
  persistence_manager_->RunInTransaction([&, this]() -> bool {
    const QuerySpec* query_spec = this->QuerySpecForTag(tag);
    if (query_spec != nullptr) {
      Optional<Path> relative_path = Path::GetRelative(query_spec->path, path);
      QuerySpec query_to_overwrite =
          relative_path->empty() ? *query_spec : QuerySpec(path);
      this->persistence_manager_->UpdateServerCache(query_to_overwrite, snap);
      Operation op = Operation::Overwrite(
          OperationSource::ForServerTaggedQuery(query_spec->params),
          *relative_path, snap);
      results = this->ApplyTaggedOperation(*query_spec, op);
      return true;
    } else {
      // Query must have been removed already
      return true;
    }
  });
  return results;
}

std::vector<Event> SyncTree::ApplyTaggedQueryMerge(
    const Path& path, const std::map<Path, Variant>& changed_children,
    const Tag& tag) {
  std::vector<Event> results;
  persistence_manager_->RunInTransaction([&, this]() -> bool {
    const QuerySpec* query_spec = QuerySpecForTag(tag);
    if (query_spec != nullptr) {
      Optional<Path> relative_path = Path::GetRelative(query_spec->path, path);
      FIREBASE_DEV_ASSERT(relative_path.has_value());
      CompoundWrite merge = CompoundWrite::FromPathMerge(changed_children);
      this->persistence_manager_->UpdateServerCache(path, merge);
      Operation op = Operation::Merge(
          OperationSource::ForServerTaggedQuery(query_spec->params),
          *relative_path, merge);
      results = ApplyTaggedOperation(*query_spec, op);
      return true;
    } else {
      // We've already removed the query. No big deal, ignore the update
      return true;
    }
  });
  return results;
}

std::vector<Event> SyncTree::ApplyListenComplete(const Path& path) {
  std::vector<Event> results;
  persistence_manager_->RunInTransaction([&, this]() -> bool {
    this->persistence_manager_->SetQueryComplete(QuerySpec(path));
    results = this->ApplyOperationToSyncPoints(
        Operation::ListenComplete(OperationSource::kServer, path));
    return true;
  });
  return results;
}

std::vector<Event> SyncTree::ApplyServerMerge(
    const Path& path, const std::map<Path, Variant>& changed_children) {
  std::vector<Event> results;
  persistence_manager_->RunInTransaction([&, this]() -> bool {
    CompoundWrite merge = CompoundWrite::FromPathMerge(changed_children);
    this->persistence_manager_->UpdateServerCache(path, merge);
    results = this->ApplyOperationToSyncPoints(
        Operation::Merge(OperationSource::kServer, path, merge));
    return true;
  });
  return results;
}

// A helper method that visits all descendant and ancestor SyncPoints, applying
// the operation.
//
// NOTES:
//  - Descendant SyncPoints will be visited first (since we raise events
//    depth-first).
//
//  - We call ApplyOperation() on each SyncPoint passing three things:
//     1. A version of the Operation that has been made relative to the
//        SyncPoint location.
//     2. A WriteTreeRef of any writes we have cached at the SyncPoint location.
//     3. A snapshot Variant with cached server data, if we have it.
//
//  - We concatenate all of the events returned by each SyncPoint and return the
//    result.
std::vector<Event> SyncTree::ApplyOperationToSyncPoints(
    const Operation& operation) {
  WriteTreeRef child_writes = pending_write_tree_->ChildWrites(Path());
  return ApplyOperationHelper(operation, &sync_point_tree_, nullptr,
                              &child_writes);
}

std::vector<Event> SyncTree::ApplyOperationHelper(
    const Operation& operation, Tree<SyncPoint>* sync_point_tree,
    const Variant* server_cache, WriteTreeRef* writes_cache) {
  if (operation.path.empty()) {
    return ApplyOperationDescendantsHelper(operation, sync_point_tree,
                                           server_cache, writes_cache);
  } else {
    Optional<SyncPoint>& sync_point = sync_point_tree->value();

    // If we don't have cached server data, see if we can get it from this
    // SyncPoint.
    if (server_cache == nullptr && sync_point.has_value()) {
      server_cache = sync_point->GetCompleteServerCache(Path());
    }

    // Apply the operation recursively deeper in the tree, in case there are
    // SyncPoints deeper in the tree.
    std::vector<Event> events;
    std::string child_key = operation.path.FrontDirectory().str();
    Optional<Operation> child_operation =
        OperationForChild(operation, child_key);
    Tree<SyncPoint>* child_tree = sync_point_tree->GetChild(child_key);
    if (child_tree && child_operation.has_value()) {
      const Variant* child_server_cache =
          server_cache ? &VariantGetChild(server_cache, child_key) : nullptr;
      WriteTreeRef child_writes_cache = writes_cache->Child(child_key);
      events = ApplyOperationHelper(*child_operation, child_tree,
                                    child_server_cache, &child_writes_cache);
    }

    // Apply the operation to the SyncPoint here if there is one here.
    if (sync_point.has_value()) {
      Extend(&events, sync_point->ApplyOperation(operation, *writes_cache,
                                                 &*server_cache,
                                                 persistence_manager_.get()));
    }
    return events;
  }
}

std::vector<Event> SyncTree::ApplyOperationDescendantsHelper(
    const Operation& operation, Tree<SyncPoint>* sync_point_tree,
    const Variant* server_cache, WriteTreeRef* writes_cache) {
  Optional<SyncPoint>& sync_point = sync_point_tree->value();

  // If we don't have cached server data, see if we can get it from this
  // SyncPoint.
  const Variant* resolved_server_cache;
  if (server_cache == nullptr && sync_point.has_value()) {
    resolved_server_cache = sync_point->GetCompleteServerCache(Path());
  } else {
    resolved_server_cache = server_cache;
  }

  std::vector<Event> events;
  for (auto& key_subtree_pair : sync_point_tree->children()) {
    const std::string& key = key_subtree_pair.first;
    Tree<SyncPoint>* sync_point_subtree = &key_subtree_pair.second;

    const Variant* child_server_cache = nullptr;
    if (resolved_server_cache != nullptr && resolved_server_cache->is_map()) {
      auto& map = resolved_server_cache->map();
      auto iter = map.find(key);
      child_server_cache = iter != map.end() ? &iter->second : nullptr;
    }
    WriteTreeRef child_writes_cache = writes_cache->Child(key);
    Optional<Operation> child_operation = OperationForChild(operation, key);
    if (child_operation.has_value()) {
      Extend(&events, ApplyOperationDescendantsHelper(
                          *child_operation, sync_point_subtree,
                          child_server_cache, &child_writes_cache));
    }
  }

  if (sync_point.has_value()) {
    Extend(&events, sync_point->ApplyOperation(operation, *writes_cache,
                                               resolved_server_cache,
                                               persistence_manager_.get()));
  }

  return events;
}

std::vector<Event> SyncTree::ApplyServerOverwrite(const Path& path,
                                                  const Variant& new_data) {
  std::vector<Event> results;
  persistence_manager_->RunInTransaction([&]() {
    QuerySpec query_spec(path);
    persistence_manager_->UpdateServerCache(query_spec, new_data);
    Operation operation = Operation::Overwrite(
        OperationSource(OperationSource::kSourceServer), path, new_data);
    results = ApplyOperationToSyncPoints(operation);
    return true;
  });
  return results;
}

std::vector<Event> SyncTree::ApplyUserMerge(
    const Path& path, const CompoundWrite& unresolved_children,
    const CompoundWrite& children, const WriteId write_id, Persist persist) {
  std::vector<Event> results;
  persistence_manager_->RunInTransaction([&, this]() -> bool {
    if (persist) {
      persistence_manager_->SaveUserMerge(path, unresolved_children, write_id);
    }
    pending_write_tree_->AddMerge(path, children, write_id);
    results = ApplyOperationToSyncPoints(
        Operation::Merge(OperationSource::kUser, path, children));
    return true;
  });
  return results;
}

std::vector<Event> SyncTree::ApplyUserOverwrite(
    const Path& path, const Variant& unresolved_new_data,
    const Variant& new_data, WriteId write_id, OverwriteVisibility visibility,
    Persist persist) {
  FIREBASE_DEV_ASSERT_MESSAGE(visibility == kOverwriteVisible || !persist,
                              "We shouldn't be persisting non-visible writes.");
  std::vector<Event> events;
  persistence_manager_->RunInTransaction([&, this]() {
    if (persist) {
      persistence_manager_->SaveUserOverwrite(path, unresolved_new_data,
                                              write_id);
    }
    pending_write_tree_->AddOverwrite(path, new_data, write_id, visibility);
    if (visibility == kOverwriteVisible) {
      events = this->ApplyOperationToSyncPoints(
          Operation::Overwrite(OperationSource::kUser, path, new_data));
    }
    return true;
  });

  return events;
}

std::vector<Event> SyncTree::RemoveAllWrites() {
  std::vector<Event> results;
  persistence_manager_->RunInTransaction([&, this]() -> bool {
    persistence_manager_->RemoveAllUserWrites();
    std::vector<UserWriteRecord> purged_writes =
        pending_write_tree_->PurgeAllWrites();
    if (purged_writes.empty()) {
      results = std::vector<Event>();
    } else {
      Tree<bool> affected_tree;
      affected_tree.set_value(true);
      results = ApplyOperationToSyncPoints(
          Operation::AckUserWrite(Path(), affected_tree, kAckRevert));
    }
    return true;
  });
  return results;
}

std::vector<Event> SyncTree::RemoveAllEventRegistrations(
    const QuerySpec& query_spec, Error error) {
  return RemoveEventRegistration(query_spec, nullptr, error);
}

Optional<Variant> SyncTree::CalcCompleteEventCache(
    const Path& path, const std::vector<WriteId>& write_ids_to_exclude) const {
  const Tree<SyncPoint>* tree = &sync_point_tree_;
  const Optional<SyncPoint>* current_sync_point = &tree->value();
  const Variant* server_cache = nullptr;
  Path path_to_follow = path;
  Path path_so_far;
  do {
    Path front = path_to_follow.FrontDirectory();
    path_to_follow = path_to_follow.PopFrontDirectory();
    path_so_far = path_so_far.GetChild(front);
    Optional<Path> relative_path = Path::GetRelative(path_so_far, path);
    tree = (!front.empty()) ? tree->GetChild(front) : nullptr;
    current_sync_point = tree ? &tree->value() : nullptr;
    if (current_sync_point && current_sync_point->has_value()) {
      server_cache =
          (*current_sync_point)->GetCompleteServerCache(*relative_path);
    }
  } while (!path_to_follow.empty() && server_cache == nullptr);
  return pending_write_tree_->CalcCompleteEventCache(
      path, server_cache, write_ids_to_exclude, kIncludeHiddenWrites);
}

void SyncTree::SetKeepSynchronized(const QuerySpec& query_spec,
                                   bool keep_synchronized) {
  bool contains =
      keep_synced_queries_.find(query_spec) != keep_synced_queries_.end();
  if (keep_synchronized && !contains) {
    AddEventRegistration(
        MakeUnique<KeepSyncedEventRegistration>(this, query_spec));
    keep_synced_queries_.insert(query_spec);
  } else if (!keep_synchronized && contains) {
    RemoveEventRegistration(query_spec, this, kErrorNone);
    keep_synced_queries_.erase(query_spec);
  }
}

static QuerySpec QuerySpecForListening(const QuerySpec& query_spec) {
  if (QuerySpecLoadsAllData(query_spec) && !QuerySpecIsDefault(query_spec)) {
    // We treat queries that load all data as default queries
    return MakeDefaultQuerySpec(query_spec);
  } else {
    return query_spec;
  }
}

void SyncTree::SetupListener(const QuerySpec& query_spec, const View* view) {
  const Path& path = query_spec.path;
  const Tag& tag = TagForQuerySpec(query_spec);
  listen_provider_->StartListening(QuerySpecForListening(query_spec), tag,
                                   view);

  Tree<SyncPoint>* subtree = sync_point_tree_.GetChild(path);

  // The root of this subtree has our query. We're here because we definitely
  // need to send a listen for that, but we may need to shadow other listens
  // as well.
  if (tag.has_value()) {
    FIREBASE_DEV_ASSERT_MESSAGE(
        !subtree->value()->HasCompleteView(),
        "If we're adding a query, it shouldn't be shadowed");
  } else {
    // Shadow everything at or below this location, this is a default listener.
    subtree->CallOnEach(path, [this](const Path& relative_path,
                                     const SyncPoint& child_sync_point) {
      if (!relative_path.empty() && child_sync_point.HasCompleteView()) {
        const QuerySpec& query_spec =
            child_sync_point.GetCompleteView()->query_spec();
        listen_provider_->StopListening(QuerySpecForListening(query_spec),
                                        TagForQuerySpec(query_spec));
      } else {
        // No default listener here.
        for (const View* sync_point_view :
             child_sync_point.GetIncompleteQueryViews()) {
          const QuerySpec& child_query_spec = sync_point_view->query_spec();
          listen_provider_->StopListening(
              QuerySpecForListening(child_query_spec),
              TagForQuerySpec(child_query_spec));
        }
      }
    });
  }
}

static void CollectDistinctViewsForSubTree(Tree<SyncPoint>* subtree,
                                           std::vector<const View*>* views) {
  Optional<SyncPoint>& maybe_sync_point = subtree->value();
  if (maybe_sync_point.has_value() && maybe_sync_point->HasCompleteView()) {
    views->push_back(maybe_sync_point->GetCompleteView());
  } else {
    if (maybe_sync_point.has_value()) {
      std::vector<const View*> query_views =
          maybe_sync_point->GetIncompleteQueryViews();
      views->insert(views->begin(), query_views.begin(), query_views.end());
    }
    for (auto& pair : subtree->children()) {
      CollectDistinctViewsForSubTree(&pair.second, views);
    }
  }
}

std::vector<Event> SyncTree::RemoveEventRegistration(
    const QuerySpec& query_spec, void* listener_ptr, Error cancel_error) {
  std::vector<Event> cancel_events;
  persistence_manager_->RunInTransaction([&]() {
    // Find the sync_point first. Then deal with whether or not it has matching
    // listeners
    SyncPoint* maybe_sync_point = sync_point_tree_.GetValueAt(query_spec.path);

    // A removal on a default query affects all queries at that location. A
    // removal on an indexed query, even one without other query constraints,
    // does *not* affect all queries at that location. So this check must be for
    // QuerySpecIsDefault(), and not QuerySpecLoadsAllData().
    if (maybe_sync_point != nullptr &&
        (QuerySpecIsDefault(query_spec) ||
         maybe_sync_point->ViewExistsForQuery(query_spec))) {
      std::vector<QuerySpec> removed;
      cancel_events = maybe_sync_point->RemoveEventRegistration(
          query_spec, listener_ptr, cancel_error, &removed);
      if (maybe_sync_point->IsEmpty()) {
        Tree<SyncPoint>* subtree = sync_point_tree_.GetChild(query_spec.path);
        if (subtree) subtree->value().reset();
      }

      // We may have just removed one of many listeners and can short-circuit
      // this whole process. We may also not have removed a default listener, in
      // which case all of the descendant listeners should already be properly
      // set up.
      //
      // Since indexed queries can shadow if they don't have other query
      // constraints, check for QuerySpecLoadsAllData(), instead of
      // QuerySpecIsDefault().
      bool removing_default = false;
      for (QuerySpec& query_removed : removed) {
        persistence_manager_->SetQueryInactive(query_spec);
        removing_default |= QuerySpecLoadsAllData(query_removed);
      }
      Tree<SyncPoint>* current_tree = &sync_point_tree_;
      bool covered = current_tree->value().has_value() &&
                     current_tree->value()->HasCompleteView();
      for (const std::string& directory : query_spec.path.GetDirectories()) {
        current_tree = current_tree->GetChild(directory);
        covered = covered || (current_tree->value().has_value() &&
                              current_tree->value()->HasCompleteView());
        if (covered || current_tree->IsEmpty()) {
          break;
        }
      }

      if (removing_default && !covered) {
        Tree<SyncPoint>* subtree = sync_point_tree_.GetChild(query_spec.path);
        // There are potentially child listeners. Determine what if any listens
        // we need to send before executing the removal.
        if (!subtree->IsEmpty()) {
          // We need to fold over our subtree and collect the listeners to send
          std::vector<const View*> new_views;
          CollectDistinctViewsForSubTree(subtree, &new_views);

          // Ok, we've collected all the listens we need. Set them up.
          for (const View* view : new_views) {
            QuerySpec new_query = view->query_spec();
            listen_provider_->StartListening(QuerySpecForListening(new_query),
                                             TagForQuerySpec(new_query), view);
          }
        } else {
          // There's nothing below us, so nothing we need to start listening on
        }
      }

      // If we removed anything and we're not covered by a higher up listen, we
      // need to stop listening on this query. The above block has us covered in
      // terms of making sure we're set up on listens lower in the tree.  Also,
      // note that if we have a cancelError, it's already been removed at the
      // provider level.
      if (!covered && !removed.empty() && cancel_error == kErrorNone) {
        // If we removed a default, then we weren't listening on any of the
        // other queries here. Just cancel the one default. Otherwise, we need
        // to iterate through and cancel each individual query
        if (removing_default) {
          listen_provider_->StopListening(QuerySpecForListening(query_spec),
                                          Tag());
        } else {
          for (const QuerySpec& query_to_remove : removed) {
            Tag tag = TagForQuerySpec(query_to_remove);
            FIREBASE_DEV_ASSERT(tag.has_value());
            listen_provider_->StopListening(
                QuerySpecForListening(query_to_remove), tag);
          }
        }
      }
      // Now, clear all of the tags we're tracking for the removed listens.
      RemoveTags(removed);
    } else {
      // No-op, this listener must've been already removed.
    }
    return true;
  });
  return cancel_events;
}

void SyncTree::RemoveTags(const std::vector<QuerySpec>& queries) {
  for (const QuerySpec& removed_query : queries) {
    if (!QuerySpecLoadsAllData(removed_query)) {
      // We should have a tag for this
      Tag tag = TagForQuerySpec(removed_query);
      FIREBASE_DEV_ASSERT(tag.has_value());
      query_spec_to_tag_map_.erase(removed_query);
      tag_to_query_spec_map_.erase(*tag);
    }
  }
}

const QuerySpec* SyncTree::QuerySpecForTag(const Tag& tag) {
  return MapGet(&tag_to_query_spec_map_, *tag);
}

Tag SyncTree::TagForQuerySpec(const QuerySpec& query_spec) {
  const Tag* tag_ptr = MapGet(&query_spec_to_tag_map_, query_spec);
  return tag_ptr ? *tag_ptr : Tag();
}

Tag SyncTree::GetNextQueryTag() { return Tag(next_query_tag_++); }

}  // namespace internal
}  // namespace database
}  // namespace firebase
