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
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "app/src/assert.h"
#include "app/src/include/firebase/variant.h"
#include "app/src/log.h"
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

class WriteTreeCompleteChildSource : public CompleteChildSource {
 public:
  WriteTreeCompleteChildSource(const WriteTreeRef& writes,
                               const ViewCache& view_cache,
                               const Variant* opt_complete_server_cache)
      : writes_(writes),
        view_cache_(view_cache),
        opt_complete_server_cache_(opt_complete_server_cache
                                       ? std::optional<Variant>(
                                             *opt_complete_server_cache)
                                       : std::nullopt) {}

  std::optional<Variant> GetCompleteChild(
      const std::string& child_key) const override {
    const CacheNode& cache_node = view_cache_.local_snap();
    if (cache_node.IsCompleteForChild(child_key)) {
      const Variant* p = GetInternalVariant(&cache_node.variant(), child_key);
      return p ? *p : Variant::Null();
    }
    CacheNode server_node;
    if (opt_complete_server_cache_.has_value()) {
      QueryParams params;
      params.order_by = QueryParams::kOrderByKey;
      server_node = CacheNode(
          IndexedVariant(*opt_complete_server_cache_, params), true, false);
    } else {
      server_node = view_cache_.server_snap();
    }
    return writes_.CalcCompleteChild(child_key, server_node);
  }

  std::optional<std::pair<Variant, Variant>> GetChildAfterChild(
      const QueryParams& query_params, const std::pair<Variant, Variant>& child,
      IterationDirection direction) const override {
    const Variant* snap = view_cache_.GetCompleteServerSnap();
    return writes_.CalcNextVariantAfterPost(
        opt_complete_server_cache_.has_value()
            ? opt_complete_server_cache_
            : (snap ? std::optional<Variant>(*snap) : std::nullopt),
        child, direction, query_params);
  }

 private:
  WriteTreeRef writes_;
  ViewCache view_cache_;
  const std::optional<Variant> opt_complete_server_cache_;
};

class NoCompleteSource : public CompleteChildSource {
 public:
  std::optional<Variant> GetCompleteChild(
      const std::string& /*child_key*/) const override {
    return std::nullopt;
  }

  std::optional<std::pair<Variant, Variant>> GetChildAfterChild(
      const QueryParams& /*query_params*/,
      const std::pair<Variant, Variant>& /*child*/,
      IterationDirection /*direction*/) const override {
    return std::nullopt;
  }
};

ViewProcessor::ViewProcessor(std::unique_ptr<VariantFilter> filter)
    : filter_(std::move(filter)) {}

ViewProcessor::~ViewProcessor() = default;

void ViewProcessor::ApplyOperation(const ViewCache& old_view_cache,
                                   const Operation& operation,
                                   const WriteTreeRef& writes_cache,
                                   const Variant* opt_complete_cache,
                                   ViewCache* out_view_cache,
                                   std::vector<Change>* out_changes) {
  FIREBASE_DEV_ASSERT(out_view_cache != nullptr);
  FIREBASE_DEV_ASSERT(out_changes != nullptr);
  ChildChangeAccumulator accumulator;
  switch (operation.type) {
    case Operation::kTypeOverwrite: {
      if (operation.source.source == OperationSource::kSourceUser) {
        *out_view_cache = ApplyUserOverwrite(old_view_cache, operation.path,
                                             operation.snapshot, writes_cache,
                                             opt_complete_cache, &accumulator);
      } else {
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
                           writes_cache, opt_complete_cache, &accumulator);
      } else {
        bool filter_server_node =
            operation.source.tagged || old_view_cache.server_snap().filtered();
        *out_view_cache = ApplyServerMerge(
            old_view_cache, operation.path, operation.children, writes_cache,
            opt_complete_cache, filter_server_node, &accumulator);
      }
      break;
    }
    case Operation::kTypeAckUserWrite: {
      *out_view_cache =
          !operation.revert
              ? AckUserWrite(old_view_cache, operation.path,
                             operation.affected_tree, writes_cache,
                             opt_complete_cache, &accumulator)
              : RevertUserWrite(old_view_cache, operation.path, writes_cache,
                                opt_complete_cache, &accumulator);
      break;
    }
    case Operation::kTypeListenComplete: {
      *out_view_cache = ListenComplete(old_view_cache, operation.path,
                                       writes_cache, &accumulator);
      break;
    }
  }

  out_changes->reserve(out_changes->size() + accumulator.size());
  for (const auto& [key, change] : accumulator) out_changes->push_back(change);
  MaybeAddValueEvent(old_view_cache, *out_view_cache, out_changes);
}

ViewCache ViewProcessor::RevertUserWrite(
    const ViewCache& view_cache, const Path& path,
    const WriteTreeRef& writes_cache, const Variant* opt_complete_server_cache,
    ChildChangeAccumulator* accumulator) {
  if (writes_cache.ShadowingWrite(path).has_value()) return view_cache;

  WriteTreeCompleteChildSource source(writes_cache, view_cache,
                                      opt_complete_server_cache);
  const IndexedVariant& old_event_cache =
      view_cache.local_snap().indexed_variant();

  IndexedVariant new_local_cache;
  if (path.empty() || IsPriorityKey(path.FrontDirectory().str())) {
    std::optional<Variant> new_node =
        view_cache.server_snap().fully_initialized()
            ? writes_cache.CalcCompleteEventCache(
                  view_cache.GetCompleteServerSnap())
            : std::optional<Variant>(writes_cache.CalcCompleteEventChildren(
                  view_cache.server_snap().variant()));
    IndexedVariant indexed_node(*new_node, filter_->query_params());
    new_local_cache =
        filter_->UpdateFullVariant(old_event_cache, indexed_node, accumulator);
  } else {
    std::string child_key = path.FrontDirectory().str();
    std::optional<Variant> new_child =
        writes_cache.CalcCompleteChild(child_key, view_cache.server_snap());
    if (!new_child.has_value() &&
        view_cache.server_snap().IsCompleteForChild(child_key)) {
      const Variant* server_child =
          GetInternalVariant(&view_cache.server_snap().variant(), child_key);
      new_child = server_child ? *server_child : Variant::Null();
    }

    if (new_child.has_value()) {
      new_local_cache =
          filter_->UpdateChild(old_event_cache, child_key, *new_child,
                               path.PopFrontDirectory(), &source, accumulator);
    } else if (GetInternalVariant(&view_cache.local_snap().variant(),
                                  child_key) != nullptr) {
      new_local_cache =
          filter_->UpdateChild(old_event_cache, child_key, Variant::Null(),
                               path.PopFrontDirectory(), &source, accumulator);
    } else {
      new_local_cache = old_event_cache;
    }

    if (new_local_cache.variant().is_null() &&
        view_cache.server_snap().fully_initialized()) {
      std::optional<Variant> complete = writes_cache.CalcCompleteEventCache(
          view_cache.GetCompleteServerSnap());
      if (complete.has_value() && VariantIsLeaf(*complete)) {
        IndexedVariant indexed_node(*complete, filter_->query_params());
        new_local_cache = filter_->UpdateFullVariant(new_local_cache,
                                                     indexed_node, accumulator);
      }
    }
  }

  bool complete = view_cache.server_snap().fully_initialized() ||
                  writes_cache.ShadowingWrite(Path()).has_value();
  return view_cache.UpdateLocalSnap(new_local_cache, complete,
                                    filter_->FiltersVariants());
}

void ViewProcessor::MaybeAddValueEvent(const ViewCache& old_view_cache,
                                       const ViewCache& new_view_cache,
                                       std::vector<Change>* accumulator) {
  const CacheNode& local_snap = new_view_cache.local_snap();
  if (!local_snap.fully_initialized()) return;
  bool is_leaf_or_empty = VariantIsLeaf(local_snap.variant()) ||
                          VariantIsEmpty(local_snap.variant());
  if (!accumulator->empty() ||
      !old_view_cache.local_snap().fully_initialized() ||
      (is_leaf_or_empty &&
       local_snap.variant() != old_view_cache.local_snap().variant()) ||
      !VariantsAreEquivalent(
          GetVariantPriority(local_snap.variant()),
          GetVariantPriority(old_view_cache.local_snap().variant()))) {
    accumulator->push_back(ValueChange(local_snap.indexed_variant()));
  }
}

ViewCache ViewProcessor::GenerateEventCacheAfterServerEvent(
    const ViewCache& view_cache, const Path& change_path,
    const WriteTreeRef& writes_cache, const CompleteChildSource* source,
    ChildChangeAccumulator* accumulator) {
  const CacheNode& old_local_snap = view_cache.local_snap();
  if (writes_cache.ShadowingWrite(change_path).has_value()) return view_cache;

  IndexedVariant new_local_cache;
  if (change_path.empty()) {
    FIREBASE_DEV_ASSERT_MESSAGE(
        view_cache.server_snap().fully_initialized(),
        "If change path is empty, we must have complete server data");
    std::optional<Variant> node_with_local_writes;
    if (view_cache.server_snap().filtered()) {
      const Variant* server_cache = view_cache.GetCompleteServerSnap();
      Variant complete_children =
          (server_cache && !VariantIsLeaf(*server_cache)) ? *server_cache
                                                          : Variant::Null();
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
      std::optional<Variant> updated_priority =
          writes_cache.CalcEventCacheAfterServerOverwrite(
              change_path, &old_local_snap.variant(),
              &view_cache.server_snap().variant());
      new_local_cache = updated_priority.has_value()
                            ? filter_->UpdatePriority(
                                  old_local_snap.indexed_variant(),
                                  *updated_priority)
                            : old_local_snap.indexed_variant();
    } else {
      Path child_change_path = change_path.PopFrontDirectory();
      std::optional<Variant> new_local_child;
      if (old_local_snap.IsCompleteForChild(child_key)) {
        std::optional<Variant> local_child_update =
            writes_cache.CalcEventCacheAfterServerOverwrite(
                change_path, &old_local_snap.variant(),
                &view_cache.server_snap().variant());
        new_local_child =
            VariantGetChild(&old_local_snap.variant(), child_key);
        if (local_child_update.has_value()) {
          VariantUpdateChild(&new_local_child.value(), child_change_path,
                             *local_child_update);
        }
      } else {
        new_local_child =
            writes_cache.CalcCompleteChild(child_key, view_cache.server_snap());
      }
      new_local_cache =
          new_local_child.has_value()
              ? filter_->UpdateChild(old_local_snap.indexed_variant(),
                                     child_key, *new_local_child,
                                     child_change_path, source, accumulator)
              : old_local_snap.indexed_variant();
    }
  }
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
  const VariantFilter* server_filter =
      filter_server_node ? filter_.get() : filter_->GetIndexedFilter();

  if (change_path.empty()) {
    new_server_cache = server_filter->UpdateFullVariant(
        old_server_snap.indexed_variant(),
        IndexedVariant(changed_snap, server_filter->query_params()), nullptr);
  } else if (server_filter->FiltersVariants() && !old_server_snap.filtered()) {
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
      return old_view_cache;
    }
    Path child_change_path = change_path.PopFrontDirectory();
    Variant new_child_node =
        VariantGetChild(&old_server_snap.variant(), child_key);
    VariantUpdateChild(&new_child_node, child_change_path, changed_snap);
    if (IsPriorityKey(child_key.str())) {
      new_server_cache = server_filter->UpdatePriority(
          old_server_snap.indexed_variant(), new_child_node);
    } else {
      NoCompleteSource source;
      new_server_cache = server_filter->UpdateChild(
          old_server_snap.indexed_variant(), child_key.str(), new_child_node,
          child_change_path, &source, nullptr);
    }
  }

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
  WriteTreeCompleteChildSource source(writes_cache, old_view_cache,
                                      opt_complete_cache);

  if (change_path.empty()) {
    IndexedVariant new_indexed(changed_snap, filter_->query_params());
    IndexedVariant new_local_cache = filter_->UpdateFullVariant(
        old_local_snap.indexed_variant(), new_indexed, accumulator);
    return old_view_cache.UpdateLocalSnap(new_local_cache, true,
                                          filter_->FiltersVariants());
  }

  std::string child_key = change_path.FrontDirectory().str();
  if (IsPriorityKey(child_key)) {
    IndexedVariant new_local_cache = filter_->UpdatePriority(
        old_local_snap.indexed_variant(), changed_snap);
    return old_view_cache.UpdateLocalSnap(new_local_cache,
                                          old_local_snap.fully_initialized(),
                                          filter_->FiltersVariants());
  }

  Path child_change_path = change_path.PopFrontDirectory();
  const Variant& old_child =
      VariantGetChild(&old_local_snap.variant(), child_key);
  Variant new_child;
  if (child_change_path.empty()) {
    new_child = changed_snap;
  } else {
    std::optional<Variant> child_node = source.GetCompleteChild(child_key);
    if (child_node.has_value()) {
      new_child = *child_node;
      if (!(IsPriorityKey(child_change_path.str()) &&
            VariantIsEmpty(VariantGetChild(&child_node.value(),
                                           child_change_path.GetParent())))) {
        VariantUpdateChild(&new_child, child_change_path, changed_snap);
      }
    } else {
      new_child = Variant::Null();
    }
  }
  if (VariantsAreEquivalent(old_child, new_child)) return old_view_cache;
  IndexedVariant new_local_snap = filter_->UpdateChild(
      old_local_snap.indexed_variant(), child_key, new_child, child_change_path,
      &source, accumulator);
  return old_view_cache.UpdateLocalSnap(new_local_snap,
                                        old_local_snap.fully_initialized(),
                                        filter_->FiltersVariants());
}

static bool CacheHasChild(const ViewCache& view_cache,
                          const std::string& child_key) {
  return view_cache.local_snap().IsCompleteForChild(child_key);
}

ViewCache ViewProcessor::ApplyUserMerge(const ViewCache& view_cache,
                                        const Path& path,
                                        const CompoundWrite& changed_children,
                                        const WriteTreeRef& writes_cache,
                                        const Variant* opt_complete_cache,
                                        ChildChangeAccumulator* accumulator) {
  FIREBASE_DEV_ASSERT_MESSAGE(!changed_children.GetRootWrite().has_value(),
                              "Can't have a merge that is an overwrite");
  ViewCache current = view_cache;
  for (bool has_child_pass : {true, false}) {
    current = changed_children.write_tree().Fold(
        current,
        [&, has_child_pass](const Path& child_path, const Variant& value,
                            ViewCache acc) {
          Path write_path = path.GetChild(child_path);
          if (CacheHasChild(view_cache, write_path.FrontDirectory().str()) ==
              has_child_pass) {
            acc = ApplyUserOverwrite(acc, write_path, value, writes_cache,
                                     opt_complete_cache, accumulator);
          }
          return acc;
        });
  }
  return current;
}

ViewCache ViewProcessor::ApplyServerMerge(const ViewCache& view_cache,
                                          const Path& path,
                                          const CompoundWrite& changed_children,
                                          const WriteTreeRef& writes_cache,
                                          const Variant* opt_complete_cache,
                                          bool filter_server_node,
                                          ChildChangeAccumulator* accumulator) {
  if (VariantIsEmpty(view_cache.server_snap().variant()) &&
      !view_cache.server_snap().fully_initialized()) {
    return view_cache;
  }

  ViewCache current_view_cache = view_cache;
  FIREBASE_DEV_ASSERT_MESSAGE(!changed_children.GetRootWrite().has_value(),
                              "Can't have a merge that is an overwrite");
  CompoundWrite actual_merge =
      path.empty()
          ? changed_children
          : CompoundWrite::EmptyWrite().AddWrites(path, changed_children);
  const Variant& server_node = view_cache.server_snap().variant();
  std::map<std::string, CompoundWrite> child_compound_writes =
      actual_merge.ChildCompoundWrites();

  for (const auto& [child_key, child_write] : child_compound_writes) {
    if (const Variant* server_child =
            GetInternalVariant(&server_node, child_key)) {
      current_view_cache = ApplyServerOverwrite(
          current_view_cache, Path(child_key), child_write.Apply(*server_child),
          writes_cache, opt_complete_cache, filter_server_node, accumulator);
    }
  }

  for (const auto& [child_key, child_write] : child_compound_writes) {
    bool is_unknown_deep_merge =
        !view_cache.server_snap().IsCompleteForChild(child_key) &&
        !child_write.GetRootWrite().has_value();
    if (!GetInternalVariant(&server_node, child_key) &&
        !is_unknown_deep_merge) {
      current_view_cache = ApplyServerOverwrite(
          current_view_cache, Path(child_key),
          child_write.Apply(Variant::Null()), writes_cache, opt_complete_cache,
          filter_server_node, accumulator);
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
  if (writes_cache.ShadowingWrite(ack_path).has_value()) return view_cache;

  bool filter_server_node = view_cache.server_snap().filtered();
  const CacheNode& server_cache = view_cache.server_snap();
  if (affected_tree.value().has_value()) {
    if ((ack_path.empty() && server_cache.fully_initialized()) ||
        server_cache.IsCompleteForPath(ack_path)) {
      const Variant* variant =
          GetInternalVariant(&server_cache.variant(), ack_path);
      return ApplyServerOverwrite(view_cache, ack_path,
                                  variant ? *variant : kNullVariant,
                                  writes_cache, opt_complete_cache,
                                  filter_server_node, accumulator);
    } else if (ack_path.empty()) {
      CompoundWrite changed_children = CompoundWrite::EmptyWrite();
      if (server_cache.variant().is_map()) {
        for (const auto& [k, v] : server_cache.variant().map()) {
          changed_children.AddWriteInline(k.AsString().string_value(), v);
        }
      }
      return ApplyServerMerge(view_cache, ack_path, changed_children,
                              writes_cache, opt_complete_cache,
                              filter_server_node, accumulator);
    }
    return view_cache;
  }

  CompoundWrite changed_children = affected_tree.Fold(
      CompoundWrite::EmptyWrite(),
      [&](Path merge_path, bool /*unused*/, const CompoundWrite& accum) {
        Path server_cache_path = ack_path.GetChild(merge_path);
        if (server_cache.IsCompleteForPath(server_cache_path)) {
          const Variant* variant =
              GetInternalVariant(&server_cache.variant(), server_cache_path);
          return accum.AddWrite(merge_path,
                                variant ? *variant : Variant::Null());
        }
        return accum;
      });
  return ApplyServerMerge(view_cache, ack_path, changed_children, writes_cache,
                          opt_complete_cache, filter_server_node, accumulator);
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
