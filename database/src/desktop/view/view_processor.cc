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

#include "database/src/desktop/view/view_processor.h"

#include <algorithm>
#include <vector>

#include "app/src/assert.h"
#include "app/src/include/firebase/variant.h"
#include "app/src/log.h"
#include "app/src/optional.h"
#include "app/src/path.h"
#include "app/src/variant_util.h"
#include "database/src/desktop/core/operation.h"
#include "database/src/desktop/util_desktop.h"
#include "database/src/desktop/view/change.h"
#include "database/src/desktop/view/child_change_accumulator.h"
#include "database/src/desktop/view/indexed_filter.h"
#include "database/src/desktop/view/variant_filter.h"
#include "database/src/desktop/view/view.h"

namespace firebase {
namespace database {
namespace internal {

// An implementation of CompleteChildSource that uses a WriteTree in addition
// to any other server data or old event caches available to calculate
// complete children. This allows us to combine both the data we know the server
// to have, as well as the writes that are pending to form a complete child.
class WriteTreeCompleteChildSource : public CompleteChildSource {
 public:
  WriteTreeCompleteChildSource(const WriteTreeRef& writes,
                               const ViewCache& view_cache,
                               const Variant* opt_complete_server_cache)
      : writes_(writes),
        view_cache_(view_cache),
        opt_complete_server_cache_(
            OptionalFromPointer(opt_complete_server_cache)) {}

  Optional<Variant> GetCompleteChild(
      const std::string& child_key) const override {
    const CacheNode& cache_node = view_cache_.local_snap();
    if (cache_node.IsCompleteForChild(child_key)) {
      Optional<Variant> result = OptionalFromPointer(
          GetInternalVariant(&cache_node.variant(), child_key));
      return result.has_value() ? result : Optional<Variant>(Variant::Null());
    }
    CacheNode server_node;
    if (opt_complete_server_cache_.has_value()) {
      // Since we're only ever getting child nodes, we can use the key index
      // here.
      QueryParams params;
      params.order_by = QueryParams::kOrderByKey;
      server_node = CacheNode(
          IndexedVariant(*opt_complete_server_cache_, params), true, false);
    } else {
      server_node = view_cache_.server_snap();
    }
    return Optional<Variant>(writes_.CalcCompleteChild(child_key, server_node));
  }

  Optional<std::pair<Variant, Variant>> GetChildAfterChild(
      const QueryParams& query_params, const std::pair<Variant, Variant>& child,
      IterationDirection direction) const override {
    return writes_.CalcNextVariantAfterPost(
        opt_complete_server_cache_.has_value()
            ? opt_complete_server_cache_
            : OptionalFromPointer(view_cache_.GetCompleteServerSnap()),
        child, direction, query_params);
  }

 private:
  WriteTreeRef writes_;
  ViewCache view_cache_;
  const Optional<Variant> opt_complete_server_cache_;
};

// An implementation of CompleteChildSource that never returns any additional
// children.
class NoCompleteSource : public CompleteChildSource {
 public:
  Optional<Variant> GetCompleteChild(
      const std::string& child_key) const override {
    return Optional<Variant>();
  }

  Optional<std::pair<Variant, Variant>> GetChildAfterChild(
      const QueryParams& query_params, const std::pair<Variant, Variant>& child,
      IterationDirection direction) const override {
    return Optional<std::pair<Variant, Variant>>();
  }
};

ViewProcessor::ViewProcessor(UniquePtr<VariantFilter> filter)
    : filter_(std::move(filter)) {}

ViewProcessor::~ViewProcessor() {}

void ViewProcessor::ApplyOperation(const ViewCache& old_view_cache,
                                   const Operation& operation,
                                   const WriteTreeRef& writes_cache,
                                   const Variant* opt_complete_cache,
                                   ViewCache* out_view_cache,
                                   std::vector<Change>* out_changes) {
  ChildChangeAccumulator accumulator;
  switch (operation.type) {
    case Operation::kTypeOverwrite: {
      if (operation.source.source == OperationSource::kSourceUser) {
        *out_view_cache = ApplyUserOverwrite(old_view_cache, operation.path,
                                             operation.snapshot, writes_cache,
                                             opt_complete_cache, &accumulator);
      } else {
        // We filter the node if it's a tagged update or the node has been
        // previously filtered and the update is not at the root in which case
        // it is ok (and necessary) to mark the node unfiltered again
        bool filter_server_node = operation.source.tagged ||
                                  (old_view_cache.server_snap().filtered() &&
                                   !operation.path.empty());
        *out_view_cache = ApplyServerOverwrite(
            old_view_cache, operation.path, operation.snapshot, writes_cache,
            opt_complete_cache, filter_server_node, &accumulator);
      }
      break;
    }
    case Operation::kTypeMerge: {
      if (operation.source.source == OperationSource::kSourceUser) {
        *out_view_cache =
            ApplyUserMerge(old_view_cache, operation.path, operation.children,
                           writes_cache, *opt_complete_cache, &accumulator);
      } else {
        // We filter the node if the node has been previously filtered.
        bool filter_server_node =
            operation.source.tagged || old_view_cache.server_snap().filtered();
        *out_view_cache = ApplyServerMerge(
            old_view_cache, operation.path, operation.children, writes_cache,
            *opt_complete_cache, filter_server_node, &accumulator);
      }
      break;
    }
    case Operation::kTypeAckUserWrite: {
      if (!operation.revert) {
        *out_view_cache = AckUserWrite(old_view_cache, operation.path,
                                       operation.affected_tree, writes_cache,
                                       opt_complete_cache, &accumulator);
      } else {
        *out_view_cache =
            RevertUserWrite(old_view_cache, operation.path, writes_cache,
                            opt_complete_cache, &accumulator);
      }
      break;
    }
    case Operation::kTypeListenComplete: {
      *out_view_cache = ListenComplete(old_view_cache, operation.path,
                                       writes_cache, &accumulator);
      break;
    }
  }

  out_changes->reserve(out_changes->size() + accumulator.size());
  for (auto& key_value_pair : accumulator) {
    out_changes->push_back(key_value_pair.second);
  }
  MaybeAddValueEvent(old_view_cache, *out_view_cache, out_changes);
}

ViewCache ViewProcessor::RevertUserWrite(
    const ViewCache& view_cache, const Path& path,
    const WriteTreeRef& writes_cache, const Variant* opt_complete_server_cache,
    ChildChangeAccumulator* accumulator) {
  // If there is a shadowing write, this change can't be seen, so do nothing.
  if (writes_cache.ShadowingWrite(path).has_value()) {
    return view_cache;
  }

  WriteTreeCompleteChildSource source(writes_cache, view_cache,
                                      opt_complete_server_cache);
  const IndexedVariant& old_event_cache =
      view_cache.local_snap().indexed_variant();
  std::string child_key = path.FrontDirectory().str();

  // Set up the new local data.
  IndexedVariant new_local_cache;
  if (path.empty() || IsPriorityKey(child_key)) {
    Optional<Variant> new_node;
    // If the server snapshot if complete, then generate a complete cache.
    // Otherwise, get as much of it as we can from the children that are
    // present.
    if (view_cache.server_snap().fully_initialized()) {
      new_node = writes_cache.CalcCompleteEventCache(
          view_cache.GetCompleteServerSnap());
    } else {
      new_node = writes_cache.CalcCompleteEventChildren(
          view_cache.server_snap().variant());
    }
    IndexedVariant indexed_node(*new_node, filter_->query_params());
    new_local_cache =
        filter_->UpdateFullVariant(old_event_cache, indexed_node, accumulator);
  } else {
    Optional<Variant> new_child =
        writes_cache.CalcCompleteChild(child_key, view_cache.server_snap());
    if (!new_child.has_value() &&
        view_cache.server_snap().IsCompleteForChild(child_key)) {
      new_child = OptionalFromPointer(
          GetInternalVariant(&old_event_cache.variant(), child_key));
    }

    // Get the new local cache set up.
    if (new_child.has_value()) {
      new_local_cache =
          filter_->UpdateChild(old_event_cache, child_key, *new_child,
                               path.PopFrontDirectory(), &source, accumulator);
    } else if (!new_child.has_value() &&
               GetInternalVariant(&view_cache.local_snap().variant(),
                                  child_key) != nullptr) {
      // No complete child available, delete the existing one, if any.
      new_local_cache =
          filter_->UpdateChild(old_event_cache, child_key, Variant::Null(),
                               path.PopFrontDirectory(), &source, accumulator);
    } else {
      new_local_cache = old_event_cache;
    }

    if (new_local_cache.variant().is_null() &&
        view_cache.server_snap().fully_initialized()) {
      // We might have reverted all child writes. Maybe the old event was
      // a leaf node.
      Optional<Variant> complete = writes_cache.CalcCompleteEventCache(
          view_cache.GetCompleteServerSnap());
      if (complete.has_value() && VariantIsLeaf(*complete)) {
        IndexedVariant indexed_node(*complete, filter_->query_params());
        new_local_cache = filter_->UpdateFullVariant(new_local_cache,
                                                     indexed_node, accumulator);
      }
    }
  }

  // Apply the new data to the local cache.
  bool complete = view_cache.server_snap().fully_initialized() ||
                  writes_cache.ShadowingWrite(Path()).has_value();
  return view_cache.UpdateLocalSnap(new_local_cache, complete,
                                    filter_->FiltersVariants());
}

void ViewProcessor::MaybeAddValueEvent(const ViewCache& old_view_cache,
                                       const ViewCache& new_view_cache,
                                       std::vector<Change>* accumulator) {
  const CacheNode& local_snap = new_view_cache.local_snap();
  if (local_snap.fully_initialized()) {
    bool is_leaf_or_empty = VariantIsLeaf(local_snap.variant()) ||
                            VariantIsEmpty(local_snap.variant());
    if (!accumulator->empty() ||
        !old_view_cache.local_snap().fully_initialized() ||
        (is_leaf_or_empty &&
         local_snap.variant() != old_view_cache.local_snap().variant()) ||
        !VariantsAreEquivalent(
            GetVariantPriority(local_snap.variant()),
            GetVariantPriority(*old_view_cache.GetCompleteLocalSnap()))) {
      accumulator->push_back(ValueChange(local_snap.indexed_variant()));
    }
  }
}

ViewCache ViewProcessor::GenerateEventCacheAfterServerEvent(
    const ViewCache& view_cache, const Path& change_path,
    const WriteTreeRef& writes_cache, const CompleteChildSource* source,
    ChildChangeAccumulator* accumulator) {
  const CacheNode& old_local_snap = view_cache.local_snap();
  if (writes_cache.ShadowingWrite(change_path).has_value()) {
    // We have a shadowing write, ignore changes.
    return view_cache;
  }
  // Set up the new local cache.
  IndexedVariant new_local_cache;
  if (change_path.empty()) {
    FIREBASE_DEV_ASSERT_MESSAGE(
        view_cache.server_snap().fully_initialized(),
        "If change path is empty, we must have complete server data");
    Optional<Variant> node_with_local_writes;
    if (view_cache.server_snap().filtered()) {
      // We need to special case this, because we need to only apply writes to
      // complete children, or we might end up raising events for incomplete
      // children. If the server data is filtered deep writes cannot be
      // guaranteed to be complete.
      const Variant* server_cache = view_cache.GetCompleteServerSnap();
      Variant complete_children = Variant::Null();
      if (server_cache && !VariantIsLeaf(*server_cache)) {
        complete_children = *server_cache;
      }
      node_with_local_writes =
          writes_cache.CalcCompleteEventChildren(complete_children);
    } else {
      node_with_local_writes = writes_cache.CalcCompleteEventCache(
          view_cache.GetCompleteServerSnap());
    }
    FIREBASE_DEV_ASSERT(node_with_local_writes.has_value());
    IndexedVariant indexed_node(*node_with_local_writes,
                                filter_->query_params());
    new_local_cache = filter_->UpdateFullVariant(
        view_cache.local_snap().indexed_variant(), indexed_node, accumulator);
  } else {
    std::vector<std::string> directories = change_path.GetDirectories();
    std::string child_key = directories.front();
    if (IsPriorityKey(child_key)) {
      FIREBASE_DEV_ASSERT_MESSAGE(
          directories.size() == 1,
          "Can't have a priority with additional path components");
      const Variant& old_event_node = old_local_snap.variant();
      const Variant& server_node = view_cache.server_snap().variant();
      // We might have overwrites for this priority.
      Optional<Variant> updated_priority =
          writes_cache.CalcEventCacheAfterServerOverwrite(
              change_path, &old_event_node, &server_node);
      // Update the priority if necessary.
      if (updated_priority.has_value()) {
        new_local_cache = filter_->UpdatePriority(
            old_local_snap.indexed_variant(), *updated_priority);
      } else {
        // Priority didn't change, keep old node.
        new_local_cache = old_local_snap.indexed_variant();
      }
    } else {
      Path child_change_path = change_path.PopFrontDirectory();
      // Update the local child.
      Optional<Variant> new_local_child;
      if (old_local_snap.IsCompleteForChild(child_key)) {
        // If we have a complete child, then we calculate an updated cache, and
        // set the new local child to the appropriate value.
        const Variant& server_node = view_cache.server_snap().variant();

        // Apply updates based on the write cache if present. Otherwise
        // use the local cache.
        Optional<Variant> local_child_update =
            writes_cache.CalcEventCacheAfterServerOverwrite(
                change_path, &old_local_snap.variant(), &server_node);
        if (local_child_update.has_value()) {
          new_local_child =
              VariantGetChild(&old_local_snap.variant(), child_key);
          VariantUpdateChild(&new_local_child.value(), child_change_path,
                             *local_child_update);
        } else {
          // Nothing changed, just keep the old child.
          new_local_child =
              VariantGetChild(&old_local_snap.variant(), child_key);
        }
      } else {
        // If the child isn't complete, we calculate it as best we can.
        new_local_child =
            writes_cache.CalcCompleteChild(child_key, view_cache.server_snap());
      }

      if (new_local_child.has_value()) {
        new_local_cache = filter_->UpdateChild(
            old_local_snap.indexed_variant(), child_key, *new_local_child,
            child_change_path, source, accumulator);
      } else {
        // No complete child available or no change.
        new_local_cache = old_local_snap.indexed_variant();
      }
    }
  }
  // Return the updated local cache.
  return view_cache.UpdateLocalSnap(
      new_local_cache,
      old_local_snap.fully_initialized() || change_path.empty(),
      filter_->FiltersVariants());
}

ViewCache ViewProcessor::ApplyServerOverwrite(
    const ViewCache& old_view_cache, const Path& change_path,
    const Variant& changed_snap, const WriteTreeRef& writes_cache,
    const Variant* opt_complete_cache, bool filter_server_node,
    ChildChangeAccumulator* accumulator) {
  CacheNode old_server_snap = old_view_cache.server_snap();
  IndexedVariant new_server_cache;
  IndexedFilter default_filter((QueryParams()));
  VariantFilter* server_filter =
      filter_server_node ? filter_.get() : &default_filter;

  if (change_path.empty()) {
    // If the path is empty, we can just apply the overwrite directly.
    new_server_cache = server_filter->UpdateFullVariant(
        old_server_snap.indexed_variant(),
        IndexedVariant(changed_snap, server_filter->query_params()), nullptr);
  } else if (server_filter->FiltersVariants() && !old_server_snap.filtered()) {
    // We want to filter the server node, but we didn't filter the server
    // node yet, so simulate a full update.
    std::string child_key = change_path.FrontDirectory().str();
    Path update_path = change_path.PopFrontDirectory();
    Variant new_child = VariantGetChild(&old_server_snap.variant(), child_key);
    VariantUpdateChild(&new_child, update_path, changed_snap);
    IndexedVariant new_server_node =
        old_server_snap.indexed_variant().UpdateChild(child_key, new_child);
    new_server_cache = server_filter->UpdateFullVariant(
        old_server_snap.indexed_variant(), new_server_node, nullptr);
  } else {
    Path child_key = change_path.FrontDirectory();
    if (!old_server_snap.IsCompleteForPath(change_path) &&
        change_path.GetDirectories().size() > 1) {
      // We don't update incomplete nodes with updates intended for other
      // listeners
      return old_view_cache;
    }
    // Apply the server overwrite to the appropriate child.
    Path child_change_path = change_path.PopFrontDirectory();
    // Get a copy of the child (if present) so that it can be mutated.
    Variant new_child_node =
        VariantGetChild(&old_server_snap.variant(), child_key);
    VariantUpdateChild(&new_child_node, child_change_path, changed_snap);
    if (IsPriorityKey(child_key.str())) {
      // If this is a priority node, update the priority on the indexed node.
      new_server_cache =
          server_filter->UpdatePriority(new_server_cache, new_child_node);
    } else {
      // If this is a regular update, the update through the filter to make sure
      // we get only the values that are not filtered by the query spec.
      NoCompleteSource source;
      new_server_cache = server_filter->UpdateChild(
          old_server_snap.indexed_variant(), child_key.str(), new_child_node,
          child_change_path, &source, nullptr);
    }
  }

  // Update the server cache, and generate the appropriate events.
  ViewCache new_view_cache = old_view_cache.UpdateServerSnap(
      new_server_cache,
      old_server_snap.fully_initialized() || change_path.empty(),
      server_filter->FiltersVariants());
  WriteTreeCompleteChildSource source(writes_cache, new_view_cache,
                                      opt_complete_cache);
  return GenerateEventCacheAfterServerEvent(new_view_cache, change_path,
                                            writes_cache, &source, accumulator);
}

ViewCache ViewProcessor::ApplyUserOverwrite(
    const ViewCache& old_view_cache, const Path& change_path,
    const Variant& changed_snap, const WriteTreeRef& writes_cache,
    const Variant* opt_complete_cache, ChildChangeAccumulator* accumulator) {
  const CacheNode& old_local_snap = old_view_cache.local_snap();
  ViewCache new_view_cache;
  WriteTreeCompleteChildSource source(writes_cache, old_view_cache,
                                      opt_complete_cache);

  if (change_path.empty()) {
    // If the path is empty, we can just apply the overwrite directly.
    IndexedVariant new_indexed(changed_snap, filter_->query_params());
    IndexedVariant new_local_cache = filter_->UpdateFullVariant(
        old_view_cache.local_snap().indexed_variant(), new_indexed,
        accumulator);
    new_view_cache = old_view_cache.UpdateLocalSnap(new_local_cache, true,
                                                    filter_->FiltersVariants());
  } else {
    // Apply the server overwrite to the appropriate child.
    std::string child_key = change_path.FrontDirectory().str();
    if (IsPriorityKey(child_key)) {
      // If this is a priority node, update the priority on the indexed node.
      IndexedVariant new_local_cache = filter_->UpdatePriority(
          old_view_cache.local_snap().indexed_variant(), changed_snap);
      new_view_cache = old_view_cache.UpdateLocalSnap(
          new_local_cache, old_local_snap.fully_initialized(),
          old_local_snap.filtered());
    } else {
      // Get the cached child variant that needs updating.
      Path child_change_path = change_path.PopFrontDirectory();
      const Variant& old_child =
          VariantGetChild(&old_local_snap.variant(), child_key);
      Variant new_child;
      if (child_change_path.empty()) {
        // Child overwrite, we can replace the child.
        new_child = changed_snap;
      } else {
        Optional<Variant> child_node = source.GetCompleteChild(child_key);
        if (child_node.has_value()) {
          if (IsPriorityKey(child_change_path.str()) &&
              VariantIsEmpty(VariantGetChild(&child_node.value(),
                                             child_change_path.GetParent()))) {
            // This is a priority update on an empty node. If this node
            // exists on the server, the server will send down the priority
            // in the update, so ignore for now.
            new_child = *child_node;
          } else {
            new_child = *child_node;
            VariantUpdateChild(&new_child, child_change_path, changed_snap);
          }
        } else {
          // There is no complete child node available
          new_child = Variant::Null();
        }
      }
      if (!VariantsAreEquivalent(old_child, new_child)) {
        IndexedVariant new_local_snap = filter_->UpdateChild(
            old_local_snap.indexed_variant(), child_key, new_child,
            child_change_path, &source, accumulator);
        new_view_cache = old_view_cache.UpdateLocalSnap(
            new_local_snap, old_local_snap.fully_initialized(),
            filter_->FiltersVariants());
      } else {
        new_view_cache = old_view_cache;
      }
    }
  }
  return new_view_cache;
}

static bool CacheHasChild(const ViewCache& view_cache,
                          const std::string& child_key) {
  return view_cache.local_snap().IsCompleteForChild(child_key);
}

ViewCache ViewProcessor::ApplyUserMerge(const ViewCache& view_cache,
                                        const Path& path,
                                        const CompoundWrite& changed_children,
                                        const WriteTreeRef& writes_cache,
                                        const Variant& server_cache,
                                        ChildChangeAccumulator* accumulator) {
  // NOTE: This behavior is replicated from the Java/Objective C implementation,
  // where is it described as a workaround. Leave as-is so as to not break
  // parity with the other implementations.
  //
  // In the case of a limit query, there may be some changes that bump things
  // out of the window leaving room for new items. It's important we process
  // these changes first, so we iterate the changes twice, first processing any
  // that affect items currently in view.
  FIREBASE_DEV_ASSERT_MESSAGE(!changed_children.GetRootWrite().has_value(),
                              "Can't have a merge that is an overwrite");
  ViewCache current_view_cache;

  current_view_cache = changed_children.write_tree().Fold(
      view_cache, [this, &path, &view_cache, &writes_cache, &server_cache,
                   &accumulator](const Path& child_path, const Variant& value,
                                 ViewCache current_view_cache) {
        Path write_path = path.GetChild(child_path);

        if (CacheHasChild(view_cache, write_path.FrontDirectory().str())) {
          current_view_cache =
              ApplyUserOverwrite(current_view_cache, write_path, value,
                                 writes_cache, &server_cache, accumulator);
        }
        return current_view_cache;
      });

  current_view_cache = changed_children.write_tree().Fold(
      current_view_cache,
      [this, &path, &view_cache, &writes_cache, &server_cache, &accumulator](
          const Path& child_path, const Variant& value,
          ViewCache current_view_cache) {
        Path write_path = path.GetChild(child_path);
        if (!CacheHasChild(view_cache, write_path.FrontDirectory().str())) {
          current_view_cache =
              ApplyUserOverwrite(current_view_cache, write_path, value,
                                 writes_cache, &server_cache, accumulator);
        }
        return current_view_cache;
      });

  return current_view_cache;
}

ViewCache ViewProcessor::ApplyServerMerge(const ViewCache& view_cache,
                                          const Path& path,
                                          const CompoundWrite& changed_children,
                                          const WriteTreeRef& writes_cache,
                                          const Variant& server_cache,
                                          bool filter_server_node,
                                          ChildChangeAccumulator* accumulator) {
  // If we don't have a cache yet, this merge was intended for a previously
  // listen in the same location. Ignore it and wait for the complete data
  // update coming soon.
  if (VariantIsEmpty(view_cache.server_snap().variant()) &&
      !view_cache.server_snap().fully_initialized()) {
    return view_cache;
  }

  // NOTE: This behavior is replicated from the Java/Objective C implementation,
  // where is it described as a workaround. Leave as-is so as to not break
  // parity with the other implementations.
  //
  // In the case of a limit query, there may be some changes that bump things
  // out of the window leaving room for new items.  It's important we process
  // these changes first, so we iterate the changes twice, first processing any
  // that affect items currently in view.
  ViewCache current_view_cache = view_cache;
  FIREBASE_DEV_ASSERT_MESSAGE(!changed_children.GetRootWrite().has_value(),
                              "Can't have a merge that is an overwrite");
  CompoundWrite actual_merge;
  if (path.empty()) {
    actual_merge = changed_children;
  } else {
    actual_merge =
        CompoundWrite::EmptyWrite().AddWrites(path, changed_children);
  }
  const Variant& server_node = view_cache.server_snap().variant();
  std::map<std::string, CompoundWrite> child_compound_writes =
      actual_merge.ChildCompoundWrites();

  for (const auto& key_value : child_compound_writes) {
    const std::string& child_key = key_value.first;
    const CompoundWrite& child_write = key_value.second;
    const Variant* server_child = GetInternalVariant(&server_node, child_key);
    if (server_child) {
      Variant new_child = child_write.Apply(*server_child);
      current_view_cache = ApplyServerOverwrite(
          current_view_cache, Path(child_key), new_child, writes_cache,
          &server_cache, filter_server_node, accumulator);
    }
  }

  for (const auto& key_value : child_compound_writes) {
    const std::string& child_key = key_value.first;
    const CompoundWrite& child_write = key_value.second;
    bool is_unknown_deep_merge =
        !view_cache.server_snap().IsCompleteForChild(child_key) &&
        !child_write.GetRootWrite().has_value();
    const Variant* server_child = GetInternalVariant(&server_node, child_key);
    if (!server_child && !is_unknown_deep_merge) {
      Variant new_child = child_write.Apply(Variant::Null());
      current_view_cache = ApplyServerOverwrite(
          current_view_cache, Path(child_key), new_child, writes_cache,
          &server_cache, filter_server_node, accumulator);
    }
  }

  return current_view_cache;
}

ViewCache ViewProcessor::AckUserWrite(const ViewCache& view_cache,
                                      const Path& ack_path,
                                      const Tree<bool>& affected_tree,
                                      const WriteTreeRef& writes_cache,
                                      const Variant* opt_complete_cache,
                                      ChildChangeAccumulator* accumulator) {
  if (writes_cache.ShadowingWrite(ack_path).has_value()) {
    return view_cache;
  }

  // Only filter server node if it is currently filtered.
  bool filter_server_node = view_cache.server_snap().filtered();

  // Essentially we'll just get our existing server cache for the affected
  // paths and re-apply it as a server update now that it won't be
  // shadowed.
  const CacheNode& server_cache = view_cache.server_snap();
  if (affected_tree.value().has_value()) {
    // This is an overwrite.
    if ((ack_path.empty() && server_cache.fully_initialized()) ||
        server_cache.IsCompleteForPath(ack_path)) {
      const Variant* variant =
          GetInternalVariant(&server_cache.variant(), ack_path);
      if (!variant) variant = &kNullVariant;
      return ApplyServerOverwrite(view_cache, ack_path, *variant, writes_cache,
                                  opt_complete_cache, filter_server_node,
                                  accumulator);
    } else if (ack_path.empty()) {
      // This is a goofy edge case where we are acking data at this location
      // but don't have full data. We should just re-apply whatever we have
      // in our cache as a merge.
      CompoundWrite changed_children = CompoundWrite::EmptyWrite();
      if (server_cache.variant().is_map()) {
        for (const auto& key_value : server_cache.variant().map()) {
          CompoundWrite temp = changed_children.AddWrite(
              key_value.first.AsString().mutable_string(), key_value.second);
          changed_children = temp;
        }
      }
      return ApplyServerMerge(view_cache, ack_path, changed_children,
                              writes_cache, *opt_complete_cache,
                              filter_server_node, accumulator);
    } else {
      return view_cache;
    }
  } else {
    // This is a merge.
    CompoundWrite changed_children = affected_tree.Fold(
        CompoundWrite::EmptyWrite(),
        [&](Path merge_path, bool unused, const CompoundWrite& accum) {
          Path server_cache_path = ack_path.GetChild(merge_path);
          if (server_cache.IsCompleteForPath(server_cache_path)) {
            return accum.AddWrite(
                merge_path, OptionalFromPointer(GetInternalVariant(
                                &server_cache.variant(), server_cache_path)));
          }
          return accum;
        });

    return ApplyServerMerge(view_cache, ack_path, changed_children,
                            writes_cache, *opt_complete_cache,
                            filter_server_node, accumulator);
  }
}

ViewCache ViewProcessor::ListenComplete(const ViewCache& view_cache,
                                        const Path& path,
                                        const WriteTreeRef& writes_cache,
                                        ChildChangeAccumulator* accumulator) {
  const CacheNode& old_server_node = view_cache.server_snap();
  ViewCache new_view_cache = view_cache.UpdateServerSnap(
      old_server_node.indexed_variant(),
      old_server_node.fully_initialized() || path.empty(),
      old_server_node.filtered());
  NoCompleteSource source;
  return GenerateEventCacheAfterServerEvent(new_view_cache, path, writes_cache,
                                            &source, accumulator);
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
