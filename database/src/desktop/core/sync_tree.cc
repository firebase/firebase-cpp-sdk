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

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "app/src/assert.h"
#include "app/src/include/firebase/variant.h"
#include "app/src/path.h"
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

namespace firebase::database::internal {

bool SyncTree::IsEmpty() { return sync_point_tree_.IsEmpty(); }

std::vector<Event> SyncTree::AckUserWrite(WriteId write_id, AckStatus revert,
                                          Persist persist,
                                          int64_t server_time_offset) {
  std::vector<Event> results;
  persistence_manager_->RunInTransaction([&, this]() -> bool {
    if (persist) persistence_manager_->RemoveUserWrite(write_id);
    const UserWriteRecord* write_record =
        pending_write_tree_->GetWrite(write_id);
    if (write_record == nullptr) return true;
    UserWriteRecord write = *write_record;
    bool need_to_reevaluate = pending_write_tree_->RemoveWrite(write_id);
    if (write.visible && !revert) {
      Variant server_values = GenerateServerValues(server_time_offset);
      if (write.is_overwrite) {
        persistence_manager_->ApplyUserWriteToServerCache(
            write.path,
            ResolveDeferredValueSnapshot(write.overwrite, server_values));
      } else {
        persistence_manager_->ApplyUserWriteToServerCache(
            write.path, ResolveDeferredValueMerge(write.merge, server_values));
      }
    }
    if (!need_to_reevaluate) {
      results.clear();
      return true;
    }
    Tree<bool> affected_tree;
    if (write.is_overwrite) {
      affected_tree.SetValueAt(Path(), true);
    } else {
      for (const auto& [key, child_write] : write.merge.ChildCompoundWrites()) {
        affected_tree.SetValueAt(Path(key), true);
      }
    }
    results = ApplyOperationToSyncPoints(
        Operation::AckUserWrite(write.path, affected_tree, revert));
    return true;
  });
  return results;
}

std::vector<Event> SyncTree::AddEventRegistration(
    std::unique_ptr<EventRegistration> event_registration) {
  std::vector<Event> events;
  persistence_manager_->RunInTransaction([&, this]() -> bool {
    const QuerySpec& query_spec = event_registration->query_spec();
    const Path& path = query_spec.path;
    const QueryParams& params = query_spec.params;

    std::optional<Variant> server_cache_variant;
    bool found_ancestor_default_view = false;
    {
      std::vector<std::string> directories = path.GetDirectories();
      Tree<SyncPoint>* tree = &sync_point_tree_;
      for (auto iter = directories.begin(); tree != nullptr; ++iter) {
        std::optional<SyncPoint>& current_sync_point = tree->value();
        if (current_sync_point.has_value()) {
          if (!server_cache_variant.has_value()) {
            const Variant* p = current_sync_point->GetCompleteServerCache(
                Path(iter, directories.end()));
            if (p) server_cache_variant = *p;
          }
          found_ancestor_default_view |= current_sync_point->HasCompleteView();
        }
        if (iter == directories.end()) break;
        tree = tree->GetChild(*iter);
      }
    }

    SyncPoint* sync_point = sync_point_tree_.GetValueAt(path);
    if (sync_point) {
      found_ancestor_default_view |= sync_point->HasCompleteView();
      if (!server_cache_variant.has_value()) {
        if (const Variant* p = sync_point->GetCompleteServerCache(Path())) {
          server_cache_variant = *p;
        }
      }
    } else {
      sync_point = &sync_point_tree_.SetValueAt(path, SyncPoint{}).value();
    }

    persistence_manager_->SetQueryActive(query_spec);

    CacheNode server_cache;
    if (server_cache_variant.has_value()) {
      server_cache =
          CacheNode(IndexedVariant(*server_cache_variant, params), true, false);
    } else {
      CacheNode persistent_server_cache =
          persistence_manager_->ServerCache(query_spec);
      if (persistent_server_cache.fully_initialized()) {
        server_cache = persistent_server_cache;
      } else {
        server_cache_variant = Variant::Null();
        Tree<SyncPoint>* subtree = sync_point_tree_.GetChild(path);
        for (auto& [child_key, child_subtree] : subtree->children()) {
          if (child_subtree.value().has_value()) {
            if (const Variant* complete_cache =
                    child_subtree.value()->GetCompleteServerCache(Path())) {
              SetVariantAtPath(&*server_cache_variant, Path(child_key),
                               *complete_cache);
            }
          }
        }
        const auto& variant =
            persistent_server_cache.indexed_variant().variant();
        if (variant.is_map()) {
          for (const auto& [raw_key, value] : variant.map()) {
            Variant key_variant = raw_key.AsString();
            const char* key = key_variant.string_value();
            if (GetInternalVariant(&*server_cache_variant, key) == nullptr) {
              VariantUpdateChild(&server_cache_variant.value(), key, value);
            }
          }
        }
        server_cache = CacheNode(IndexedVariant(*server_cache_variant, params),
                                 false, false);
      }
    }

    bool view_already_exists = sync_point->ViewExistsForQuery(query_spec);
    if (!view_already_exists && !QuerySpecLoadsAllData(query_spec)) {
      FIREBASE_DEV_ASSERT_MESSAGE(!query_spec_to_tag_map_.count(query_spec),
                                  "View does not exist but we have a tag");
      Tag tag = GetNextQueryTag();
      query_spec_to_tag_map_[query_spec] = tag;
      tag_to_query_spec_map_[*tag] = query_spec;
    }
    WriteTreeRef writes_cache = pending_write_tree_->ChildWrites(path);
    events = sync_point->AddEventRegistration(std::move(event_registration),
                                              writes_cache, server_cache,
                                              persistence_manager_.get());
    if (!view_already_exists && !found_ancestor_default_view) {
      SetupListener(query_spec);
    }
    return true;
  });
  return events;
}

std::vector<Event> SyncTree::ApplyTaggedListenComplete(const Tag& tag) {
  std::vector<Event> results;
  persistence_manager_->RunInTransaction([&, this]() -> bool {
    if (const QuerySpec* query_spec = QuerySpecForTag(tag)) {
      persistence_manager_->SetQueryComplete(*query_spec);
      results = ApplyTaggedOperation(
          *query_spec,
          Operation::ListenComplete(
              OperationSource::ForServerTaggedQuery(query_spec->params),
              Path()));
    }
    return true;
  });
  return results;
}

std::vector<Event> SyncTree::ApplyTaggedOperation(const QuerySpec& query_spec,
                                                  const Operation& operation) {
  SyncPoint* sync_point = sync_point_tree_.GetValueAt(query_spec.path);
  FIREBASE_DEV_ASSERT_MESSAGE(
      sync_point != nullptr,
      "Missing sync point for query tag that we're tracking");
  if (sync_point == nullptr) return {};
  WriteTreeRef writes_cache = pending_write_tree_->ChildWrites(query_spec.path);
  return sync_point->ApplyOperation(operation, writes_cache, nullptr,
                                    persistence_manager_.get());
}

std::vector<Event> SyncTree::ApplyTaggedQueryOverwrite(const Path& path,
                                                       const Variant& snap,
                                                       const Tag& tag) {
  std::vector<Event> results;
  persistence_manager_->RunInTransaction([&, this]() -> bool {
    if (const QuerySpec* query_spec = QuerySpecForTag(tag)) {
      auto relative_path =
          Path::GetRelative(query_spec->path, path);
      FIREBASE_DEV_ASSERT(relative_path.has_value());
      if (!relative_path.has_value()) return true;
      persistence_manager_->UpdateServerCache(
          relative_path->empty() ? *query_spec : QuerySpec(path), snap);
      results = ApplyTaggedOperation(
          *query_spec,
          Operation::Overwrite(
              OperationSource::ForServerTaggedQuery(query_spec->params),
              *relative_path, snap));
    }
    return true;
  });
  return results;
}

std::vector<Event> SyncTree::ApplyTaggedQueryMerge(
    const Path& path, const std::map<Path, Variant>& changed_children,
    const Tag& tag) {
  std::vector<Event> results;
  persistence_manager_->RunInTransaction([&, this]() -> bool {
    if (const QuerySpec* query_spec = QuerySpecForTag(tag)) {
      auto relative_path =
          Path::GetRelative(query_spec->path, path);
      FIREBASE_DEV_ASSERT(relative_path.has_value());
      if (!relative_path.has_value()) return true;
      CompoundWrite merge = CompoundWrite::FromPathMerge(changed_children);
      persistence_manager_->UpdateServerCache(path, merge);
      results = ApplyTaggedOperation(
          *query_spec,
          Operation::Merge(
              OperationSource::ForServerTaggedQuery(query_spec->params),
              *relative_path, merge));
    }
    return true;
  });
  return results;
}

std::vector<Event> SyncTree::ApplyListenComplete(const Path& path) {
  std::vector<Event> results;
  persistence_manager_->RunInTransaction([&, this]() -> bool {
    persistence_manager_->SetQueryComplete(QuerySpec(path));
    results = ApplyOperationToSyncPoints(
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
    persistence_manager_->UpdateServerCache(path, merge);
    results = ApplyOperationToSyncPoints(
        Operation::Merge(OperationSource::kServer, path, merge));
    return true;
  });
  return results;
}

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
  }
  std::optional<SyncPoint>& sync_point = sync_point_tree->value();
  if (server_cache == nullptr && sync_point.has_value()) {
    server_cache = sync_point->GetCompleteServerCache(Path());
  }

  std::vector<Event> events;
  std::string child_key = operation.path.FrontDirectory().str();
  std::optional<Operation> child_operation =
      OperationForChild(operation, child_key);
  Tree<SyncPoint>* child_tree = sync_point_tree->GetChild(child_key);
  if (child_tree && child_operation.has_value()) {
    const Variant* child_server_cache =
        server_cache ? &VariantGetChild(server_cache, child_key) : nullptr;
    WriteTreeRef child_writes_cache = writes_cache->Child(child_key);
    events = ApplyOperationHelper(*child_operation, child_tree,
                                  child_server_cache, &child_writes_cache);
  }
  if (sync_point.has_value()) {
    Extend(&events,
           sync_point->ApplyOperation(operation, *writes_cache, server_cache,
                                      persistence_manager_.get()));
  }
  return events;
}

std::vector<Event> SyncTree::ApplyOperationDescendantsHelper(
    const Operation& operation, Tree<SyncPoint>* sync_point_tree,
    const Variant* server_cache, WriteTreeRef* writes_cache) {
  std::optional<SyncPoint>& sync_point = sync_point_tree->value();
  const Variant* resolved_server_cache =
      (server_cache == nullptr && sync_point.has_value())
          ? sync_point->GetCompleteServerCache(Path())
          : server_cache;

  std::vector<Event> events;
  for (auto& [key, sync_point_subtree] : sync_point_tree->children()) {
    const Variant* child_server_cache =
        resolved_server_cache ? &VariantGetChild(resolved_server_cache, key)
                              : nullptr;
    WriteTreeRef child_writes_cache = writes_cache->Child(key);
    if (std::optional<Operation> child_op = OperationForChild(operation, key)) {
      Extend(&events, ApplyOperationDescendantsHelper(
                          *child_op, &sync_point_subtree, child_server_cache,
                          &child_writes_cache));
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
    persistence_manager_->UpdateServerCache(QuerySpec(path), new_data);
    results = ApplyOperationToSyncPoints(Operation::Overwrite(
        OperationSource(OperationSource::kSourceServer), path, new_data));
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
      events = ApplyOperationToSyncPoints(
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
    if (!pending_write_tree_->PurgeAllWrites().empty()) {
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

std::optional<Variant> SyncTree::CalcCompleteEventCache(
    const Path& path, const std::vector<WriteId>& write_ids_to_exclude) const {
  const Tree<SyncPoint>* tree = &sync_point_tree_;
  const Variant* server_cache =
      tree->value().has_value() ? tree->value()->GetCompleteServerCache(path)
                                : nullptr;
  Path path_to_follow = path;
  Path path_so_far;
  while (tree != nullptr && !path_to_follow.empty() &&
         server_cache == nullptr) {
    Path front = path_to_follow.FrontDirectory();
    path_to_follow = path_to_follow.PopFrontDirectory();
    path_so_far = path_so_far.GetChild(front);
    auto relative_path = Path::GetRelative(path_so_far, path);
    tree = (!front.empty()) ? tree->GetChild(front) : nullptr;
    if (tree && tree->value().has_value() && relative_path.has_value()) {
      server_cache = tree->value()->GetCompleteServerCache(*relative_path);
    }
  }
  return pending_write_tree_->CalcCompleteEventCache(
      path, server_cache, write_ids_to_exclude, kIncludeHiddenWrites);
}

void SyncTree::SetKeepSynchronized(const QuerySpec& query_spec,
                                   bool keep_synchronized) {
  bool contains = keep_synced_queries_.count(query_spec) != 0;
  if (keep_synchronized && !contains) {
    AddEventRegistration(
        std::make_unique<KeepSyncedEventRegistration>(this, query_spec));
    keep_synced_queries_.insert(query_spec);
  } else if (!keep_synchronized && contains) {
    RemoveEventRegistration(query_spec, this, kErrorNone);
    keep_synced_queries_.erase(query_spec);
  }
}

static QuerySpec QuerySpecForListening(const QuerySpec& query_spec) {
  return (QuerySpecLoadsAllData(query_spec) && !QuerySpecIsDefault(query_spec))
             ? MakeDefaultQuerySpec(query_spec)
             : query_spec;
}

void SyncTree::SetupListener(const QuerySpec& query_spec) {
  const Path& path = query_spec.path;
  const Tag& tag = TagForQuerySpec(query_spec);
  listen_provider_->StartListening(QuerySpecForListening(query_spec), tag);

  Tree<SyncPoint>* subtree = sync_point_tree_.GetChild(path);
  if (tag.has_value()) {
    FIREBASE_DEV_ASSERT_MESSAGE(
        !subtree->value()->HasCompleteView(),
        "If we're adding a query, it shouldn't be shadowed");
  } else {
    subtree->CallOnEach(
        path, [this](const Path& relative_path,
                     const SyncPoint& child_sync_point) {
          if (!relative_path.empty() && child_sync_point.HasCompleteView()) {
            const QuerySpec& qs =
                child_sync_point.GetCompleteView()->query_spec();
            listen_provider_->StopListening(QuerySpecForListening(qs),
                                            TagForQuerySpec(qs));
          } else {
            for (const View* view :
                 child_sync_point.GetIncompleteQueryViews()) {
              const QuerySpec& qs = view->query_spec();
              listen_provider_->StopListening(QuerySpecForListening(qs),
                                              TagForQuerySpec(qs));
            }
          }
        });
  }
}

static void CollectDistinctViewsForSubTree(Tree<SyncPoint>* subtree,
                                           std::vector<const View*>* views) {
  std::optional<SyncPoint>& maybe_sync_point = subtree->value();
  if (maybe_sync_point.has_value() && maybe_sync_point->HasCompleteView()) {
    views->push_back(maybe_sync_point->GetCompleteView());
  } else {
    if (maybe_sync_point.has_value()) {
      auto query_views = maybe_sync_point->GetIncompleteQueryViews();
      views->insert(views->begin(), query_views.begin(), query_views.end());
    }
    for (auto& [key, child] : subtree->children()) {
      CollectDistinctViewsForSubTree(&child, views);
    }
  }
}

std::vector<Event> SyncTree::RemoveEventRegistration(
    const QuerySpec& query_spec, void* listener_ptr, Error cancel_error) {
  std::vector<Event> cancel_events;
  persistence_manager_->RunInTransaction([&]() {
    SyncPoint* maybe_sync_point = sync_point_tree_.GetValueAt(query_spec.path);
    if (maybe_sync_point != nullptr &&
        (QuerySpecIsDefault(query_spec) ||
         maybe_sync_point->ViewExistsForQuery(query_spec))) {
      std::vector<QuerySpec> removed;
      cancel_events = maybe_sync_point->RemoveEventRegistration(
          query_spec, listener_ptr, cancel_error, &removed);
      if (maybe_sync_point->IsEmpty()) {
        if (Tree<SyncPoint>* subtree =
                sync_point_tree_.GetChild(query_spec.path)) {
          subtree->value().reset();
        }
      }

      bool removing_default = false;
      for (const QuerySpec& query_removed : removed) {
        persistence_manager_->SetQueryInactive(query_removed);
        removing_default |= QuerySpecLoadsAllData(query_removed);
      }
      Tree<SyncPoint>* current_tree = &sync_point_tree_;
      bool covered = current_tree->value().has_value() &&
                     current_tree->value()->HasCompleteView();
      for (const std::string& directory : query_spec.path.GetDirectories()) {
        current_tree = current_tree->GetChild(directory);
        covered = covered || (current_tree->value().has_value() &&
                              current_tree->value()->HasCompleteView());
        if (covered || current_tree->IsEmpty()) break;
      }

      if (removing_default && !covered) {
        Tree<SyncPoint>* subtree = sync_point_tree_.GetChild(query_spec.path);
        if (!subtree->IsEmpty()) {
          std::vector<const View*> new_views;
          CollectDistinctViewsForSubTree(subtree, &new_views);
          for (const View* view : new_views) {
            QuerySpec new_query = view->query_spec();
            listen_provider_->StartListening(QuerySpecForListening(new_query),
                                             TagForQuerySpec(new_query));
          }
        }
      }

      if (!covered && !removed.empty() && cancel_error == kErrorNone) {
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
      RemoveTags(removed);
    }
    return true;
  });
  return cancel_events;
}

void SyncTree::RemoveTags(const std::vector<QuerySpec>& queries) {
  for (const QuerySpec& removed_query : queries) {
    if (!QuerySpecLoadsAllData(removed_query)) {
      Tag tag = TagForQuerySpec(removed_query);
      FIREBASE_DEV_ASSERT(tag.has_value());
      query_spec_to_tag_map_.erase(removed_query);
      tag_to_query_spec_map_.erase(*tag);
    }
  }
}

const QuerySpec* SyncTree::QuerySpecForTag(const Tag& tag) {
  return tag.has_value() ? MapGet(&tag_to_query_spec_map_, *tag) : nullptr;
}

Tag SyncTree::TagForQuerySpec(const QuerySpec& query_spec) {
  const Tag* tag_ptr = MapGet(&query_spec_to_tag_map_, query_spec);
  return tag_ptr ? *tag_ptr : Tag();
}

Tag SyncTree::GetNextQueryTag() { return Tag(next_query_tag_++); }

}  // namespace firebase::database::internal
