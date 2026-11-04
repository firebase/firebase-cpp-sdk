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

#include "database/src/desktop/core/write_tree.h"

#include <algorithm>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "app/src/assert.h"
#include "database/src/desktop/core/compound_write.h"
#include "database/src/desktop/query_params_comparator.h"
#include "database/src/desktop/view/view_cache.h"

namespace firebase {
namespace database {
namespace internal {

namespace {
bool DefaultFilter(const UserWriteRecord& record, void*) {
  return record.visible;
}
std::optional<Variant> OptVariant(const Variant* ptr) {
  return ptr ? std::optional<Variant>(*ptr) : std::nullopt;
}
}  // namespace

void WriteTree::AddOverwrite(const Path& path, const Variant& snap,
                             WriteId write_id, OverwriteVisibility visibility) {
  FIREBASE_DEV_ASSERT(write_id > last_write_id_);
  all_writes_.emplace_back(write_id, path, snap,
                           visibility == kOverwriteVisible);
  if (visibility == kOverwriteVisible) {
    visible_writes_.AddWriteInline(path, snap);
  }
  last_write_id_ = write_id;
}

void WriteTree::AddMerge(const Path& path,
                         const CompoundWrite& changed_children,
                         WriteId write_id) {
  FIREBASE_DEV_ASSERT(write_id > last_write_id_);
  all_writes_.emplace_back(write_id, path, changed_children);
  visible_writes_.AddWritesInline(path, changed_children);
  last_write_id_ = write_id;
}

UserWriteRecord* WriteTree::GetWrite(WriteId write_id) {
  for (UserWriteRecord& record : all_writes_) {
    if (record.write_id == write_id) return &record;
  }
  return nullptr;
}

std::vector<UserWriteRecord> WriteTree::PurgeAllWrites() {
  std::vector<UserWriteRecord> purged_writes(std::move(all_writes_));
  visible_writes_ = CompoundWrite();
  all_writes_.clear();
  return purged_writes;
}

bool WriteTree::RemoveWrite(WriteId write_id) {
  auto iter = std::find_if(all_writes_.begin(), all_writes_.end(),
                           [write_id](const UserWriteRecord& r) {
                             return r.write_id == write_id;
                           });
  FIREBASE_DEV_ASSERT_MESSAGE(iter != all_writes_.end(),
                              "remove_write called with nonexistent write_id");
  UserWriteRecord write_to_remove = *iter;
  iter = all_writes_.erase(iter);

  bool removed_write_was_visible = write_to_remove.visible;
  bool removed_write_overlaps = false;

  for (auto i = all_writes_.rbegin(); i != all_writes_.rend(); ++i) {
    if (!i->visible) continue;
    if (i.base() >= iter && RecordContainsPath(*i, write_to_remove.path)) {
      removed_write_was_visible = false;
      break;
    } else if (write_to_remove.path.IsParent(i->path)) {
      removed_write_overlaps = true;
    }
  }

  if (!removed_write_was_visible) return false;
  if (removed_write_overlaps) {
    ResetTree();
    return true;
  }
  if (write_to_remove.is_overwrite) {
    visible_writes_.RemoveWriteInline(write_to_remove.path);
  } else {
    for (const auto& [key, subtree] :
         write_to_remove.merge.write_tree().children()) {
      visible_writes_.RemoveWriteInline(write_to_remove.path.GetChild(key));
    }
  }
  return true;
}

std::optional<Variant> WriteTree::CalcCompleteEventCache(
    const Path& tree_path, const Variant* complete_server_cache,
    const std::vector<WriteId>& write_ids_to_exclude,
    HiddenWriteInclusion include_hidden_writes) const {
  if (write_ids_to_exclude.empty() &&
      include_hidden_writes == kExcludeHiddenWrites) {
    if (std::optional<Variant> shadowing = ShadowingWrite(tree_path)) {
      return shadowing;
    }
    CompoundWrite sub_merge = visible_writes_.ChildCompoundWrite(tree_path);
    if (sub_merge.IsEmpty()) return OptVariant(complete_server_cache);
    if (complete_server_cache == nullptr &&
        !sub_merge.HasCompleteWrite(Path())) {
      return std::nullopt;
    }
    return sub_merge.Apply(complete_server_cache ? *complete_server_cache
                                                 : kNullVariant);
  }

  CompoundWrite merge = visible_writes_.ChildCompoundWrite(tree_path);
  if (!include_hidden_writes && merge.IsEmpty()) {
    return OptVariant(complete_server_cache);
  }
  if (!include_hidden_writes && complete_server_cache == nullptr &&
      !merge.HasCompleteWrite(Path())) {
    return std::nullopt;
  }
  struct FilterData {
    const std::vector<WriteId>* write_ids_to_exclude;
    const Path* tree_path;
    HiddenWriteInclusion include_hidden_writes;
  } filter_userdata{&write_ids_to_exclude, &tree_path, include_hidden_writes};
  UserWriteRecordPredicateFn filter = [](const UserWriteRecord& write,
                                         void* userdata) {
    const auto* d = static_cast<FilterData*>(userdata);
    if (!write.visible && !d->include_hidden_writes) return false;
    if (std::find(d->write_ids_to_exclude->begin(),
                  d->write_ids_to_exclude->end(),
                  write.write_id) != d->write_ids_to_exclude->end()) {
      return false;
    }
    return write.path.IsParent(*d->tree_path) ||
           d->tree_path->IsParent(write.path);
  };
  CompoundWrite merge_at_path =
      LayerTree(all_writes_, filter, &filter_userdata, tree_path);
  return merge_at_path.Apply(complete_server_cache ? *complete_server_cache
                                                   : Variant());
}

Variant WriteTree::CalcCompleteEventChildren(
    const Path& tree_path, const Variant& complete_server_children) const {
  Variant complete_children = Variant::Null();
  if (std::optional<Variant> top_level_set =
          visible_writes_.GetCompleteVariant(tree_path)) {
    if (top_level_set->is_map()) complete_children = *top_level_set;
    return complete_children;
  }
  CompoundWrite merge = visible_writes_.ChildCompoundWrite(tree_path);
  if (complete_server_children.is_map()) {
    for (const auto& [key, value] : complete_server_children.map()) {
      Path key_path(key.string_value());
      *MakeVariantAtPath(&complete_children, key_path) =
          merge.ChildCompoundWrite(key_path).Apply(value);
    }
  }
  for (const auto& [key, value] : merge.GetCompleteChildren()) {
    *MakeVariantAtPath(&complete_children, Path(key.string_value())) = value;
  }
  return complete_children;
}

std::optional<Variant> WriteTree::CalcEventCacheAfterServerOverwrite(
    const Path& tree_path, const Path& child_path,
    const Variant* existing_local_snap,
    const Variant* existing_server_snap) const {
  FIREBASE_DEV_ASSERT_MESSAGE(
      existing_local_snap != nullptr || existing_server_snap != nullptr,
      "Either existing_local_snap or existing_server_snap must exist");
  Path path = tree_path.GetChild(child_path);
  if (visible_writes_.HasCompleteWrite(path)) return std::nullopt;
  CompoundWrite child_merge = visible_writes_.ChildCompoundWrite(path);
  const Variant& server_child =
      VariantGetChild(existing_server_snap, child_path);
  return child_merge.IsEmpty() ? server_child : child_merge.Apply(server_child);
}

std::optional<Variant> WriteTree::CalcCompleteChild(
    const Path& tree_path, const std::string& child_key,
    const CacheNode& existing_server_snap) const {
  Path path = tree_path.GetChild(child_key);
  if (std::optional<Variant> shadowing =
          visible_writes_.GetCompleteVariant(path)) {
    return shadowing;
  }
  if (existing_server_snap.IsCompleteForChild(child_key)) {
    return visible_writes_.ChildCompoundWrite(path).Apply(
        VariantGetChild(&existing_server_snap.variant(), child_key));
  }
  return std::nullopt;
}

std::optional<std::pair<Variant, Variant>> WriteTree::CalcNextVariantAfterPost(
    const Path& tree_path, const std::optional<Variant>& complete_server_data,
    const std::pair<Variant, Variant>& post, IterationDirection direction,
    const QueryParams& query_params) const {
  CompoundWrite merge = visible_writes_.ChildCompoundWrite(tree_path);
  std::optional<Variant> to_iterate = merge.GetCompleteVariant(Path());
  if (!to_iterate.has_value()) {
    if (!complete_server_data.has_value()) return std::nullopt;
    to_iterate = merge.Apply(*complete_server_data);
  }

  std::optional<std::pair<Variant, Variant>> current_next;
  QueryParamsComparator comp(&query_params);
  int d = (direction == kIterateForward) ? 1 : -1;
  for (const auto& kv : to_iterate->map()) {
    if ((d * comp.Compare(kv.first, kv.second, post.first, post.second)) > 0) {
      if (!current_next.has_value() ||
          (d * comp.Compare(kv.first, kv.second, current_next->first,
                            current_next->second)) < 0) {
        current_next = kv;
      }
    }
  }
  return current_next;
}

bool WriteTree::RecordContainsPath(const UserWriteRecord& write_record,
                                   const Path& path) {
  if (write_record.is_overwrite) return write_record.path.IsParent(path);
  bool result = false;
  write_record.merge.write_tree().CallOnEach(
      Path(), [&](const Path& current_path, Variant) {
        if (write_record.path.GetChild(current_path).IsParent(path)) {
          result = true;
        }
      });
  return result;
}

void WriteTree::ResetTree() {
  visible_writes_ = LayerTree(all_writes_, DefaultFilter, nullptr, Path());
  last_write_id_ = all_writes_.empty() ? -1L : all_writes_.back().write_id;
}

CompoundWrite WriteTree::LayerTree(const std::vector<UserWriteRecord>& writes,
                                   WriteTree::UserWriteRecordPredicateFn filter,
                                   void* filter_userdata,
                                   const Path& tree_root) {
  CompoundWrite compound_write;
  for (const UserWriteRecord& write : writes) {
    if (!filter(write, filter_userdata)) continue;
    const Path& write_path = write.path;
    if (write.is_overwrite) {
      if (tree_root.IsParent(write_path)) {
        compound_write.AddWriteInline(
            *Path::GetRelative(tree_root, write_path), write.overwrite);
      } else if (write_path.IsParent(tree_root)) {
        compound_write.AddWriteInline(
            Path(), VariantGetChild(&write.overwrite,
                                    *Path::GetRelative(write_path, tree_root)));
      }
    } else {
      if (tree_root.IsParent(write_path)) {
        compound_write.AddWritesInline(
            *Path::GetRelative(tree_root, write_path), write.merge);
      } else if (write_path.IsParent(tree_root)) {
        auto relative_path = Path::GetRelative(write_path, tree_root);
        if (relative_path->empty()) {
          compound_write.AddWritesInline(Path(), write.merge);
        } else if (std::optional<Variant> deep_node =
                       write.merge.GetCompleteVariant(*relative_path)) {
          compound_write.AddWriteInline(Path(), deep_node);
        }
      }
    }
  }
  return compound_write;
}

std::optional<Variant> WriteTreeRef::CalcCompleteEventCache(
    const Variant* complete_server_cache) const {
  return write_tree_->CalcCompleteEventCache(path_, complete_server_cache);
}

std::optional<Variant> WriteTreeRef::CalcCompleteEventCache(
    const Variant* complete_server_cache,
    const std::vector<WriteId>& write_ids_to_exclude) const {
  return write_tree_->CalcCompleteEventCache(path_, complete_server_cache,
                                             write_ids_to_exclude);
}

std::optional<Variant> WriteTreeRef::CalcCompleteEventCache(
    const Variant* complete_server_cache,
    const std::vector<WriteId>& write_ids_to_exclude,
    HiddenWriteInclusion include_hidden_writes) const {
  return write_tree_->CalcCompleteEventCache(
      path_, complete_server_cache, write_ids_to_exclude,
      include_hidden_writes);
}

Variant WriteTreeRef::CalcCompleteEventChildren(
    const Variant& complete_server_children) const {
  return write_tree_->CalcCompleteEventChildren(path_,
                                                complete_server_children);
}

std::optional<Variant> WriteTreeRef::CalcEventCacheAfterServerOverwrite(
    const Path& path, const Variant* existing_local_snap,
    const Variant* existing_server_snap) const {
  return write_tree_->CalcEventCacheAfterServerOverwrite(
      path_, path, existing_local_snap, existing_server_snap);
}

std::optional<Variant> WriteTreeRef::ShadowingWrite(const Path& path) const {
  return write_tree_->ShadowingWrite(path_.GetChild(path));
}

std::optional<Variant> WriteTreeRef::CalcCompleteChild(
    const std::string& child_key,
    const CacheNode& existing_server_cache) const {
  return write_tree_->CalcCompleteChild(path_, child_key,
                                        existing_server_cache);
}

std::optional<std::pair<Variant, Variant>>
WriteTreeRef::CalcNextVariantAfterPost(
    const std::optional<Variant>& complete_server_data,
    const std::pair<Variant, Variant>& start_post, IterationDirection direction,
    const QueryParams& query_params) const {
  return write_tree_->CalcNextVariantAfterPost(
      path_, complete_server_data, start_post, direction, query_params);
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
