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

#include "app/src/assert.h"
#include "database/src/desktop/core/compound_write.h"
#include "database/src/desktop/query_params_comparator.h"
#include "database/src/desktop/view/view_cache.h"

namespace firebase {
namespace database {
namespace internal {

bool DefaultFilter(const UserWriteRecord& record, void*) {
  return record.visible;
}

WriteTreeRef WriteTree::ChildWrites(const Path& database_path) {
  return WriteTreeRef(database_path, this);
}

void WriteTree::AddOverwrite(const Path& path, const Variant& snap,
                             WriteId write_id, OverwriteVisibility visibility) {
  // Stacking an older write on top of newer ones.
  FIREBASE_DEV_ASSERT(write_id > last_write_id_);
  all_writes_.push_back(
      UserWriteRecord(write_id, path, snap, visibility == kOverwriteVisible));
  if (visibility == kOverwriteVisible) {
    visible_writes_.AddWriteInline(path, snap);
  }
  last_write_id_ = write_id;
}

void WriteTree::AddMerge(const Path& path,
                         const CompoundWrite& changed_children,
                         WriteId write_id) {
  // Stacking an older write on top of newer ones.
  FIREBASE_DEV_ASSERT(write_id > last_write_id_);
  all_writes_.push_back(UserWriteRecord(write_id, path, changed_children));
  visible_writes_.AddWritesInline(path, changed_children);
  last_write_id_ = write_id;
}

UserWriteRecord* WriteTree::GetWrite(WriteId write_id) {
  for (UserWriteRecord& record : all_writes_) {
    if (record.write_id == write_id) {
      return &record;
    }
  }
  return nullptr;
}

std::vector<UserWriteRecord> WriteTree::PurgeAllWrites() {
  std::vector<UserWriteRecord> purged_writes(all_writes_);
  // Reset everything.
  visible_writes_ = CompoundWrite();
  all_writes_.clear();
  return purged_writes;
}

bool WriteTree::RemoveWrite(WriteId write_id) {
  UserWriteRecord write_to_remove;
  std::vector<UserWriteRecord>::iterator iter;
  for (iter = all_writes_.begin(); iter != all_writes_.end(); ++iter) {
    if (iter->write_id == write_id) {
      write_to_remove = *iter;
      break;
    }
  }
  FIREBASE_DEV_ASSERT_MESSAGE(iter != all_writes_.end(),
                              "remove_write called with nonexistent write_id");

  iter = all_writes_.erase(iter);

  bool removed_write_was_visible = write_to_remove.visible;
  bool removed_write_overlaps_with_other_writes = false;

  for (auto i = all_writes_.rbegin(); i != all_writes_.rend(); ++i) {
    UserWriteRecord& current_write = *i;
    if (current_write.visible) {
      if (i.base() >= iter &&
          RecordContainsPath(current_write, write_to_remove.path)) {
        // The removed write was completely shadowed by a subsequent write.
        removed_write_was_visible = false;
        break;
      } else if (write_to_remove.path.IsParent(current_write.path)) {
        // Either we're covering some writes or they're covering part of us
        // (depending on which came first).
        removed_write_overlaps_with_other_writes = true;
      }
    }
  }

  if (!removed_write_was_visible) {
    return false;
  } else if (removed_write_overlaps_with_other_writes) {
    // There's some shadowing going on. Just rebuild the visible writes from
    // scratch.
    ResetTree();
    return true;
  } else {
    // There's no shadowing.  We can safely just remove the write(s) from
    // visible_writes_.
    if (write_to_remove.is_overwrite) {
      visible_writes_.RemoveWriteInline(write_to_remove.path);
    } else {
      for (auto& entry : write_to_remove.merge.write_tree().children()) {
        Path path(entry.first);
        visible_writes_.RemoveWriteInline(write_to_remove.path.GetChild(path));
      }
    }
    return true;
  }
}

Optional<Variant> WriteTree::GetCompleteWriteData(const Path& path) const {
  return visible_writes_.GetCompleteVariant(path);
}

Optional<Variant> WriteTree::CalcCompleteEventCache(
    const Path& tree_path, const Variant* complete_server_cache) const {
  return CalcCompleteEventCache(tree_path, complete_server_cache,
                                std::vector<WriteId>());
}

Optional<Variant> WriteTree::CalcCompleteEventCache(
    const Path& tree_path, const Variant* complete_server_cache,
    const std::vector<WriteId>& write_ids_to_exclude) const {
  return CalcCompleteEventCache(tree_path, complete_server_cache,
                                write_ids_to_exclude, kExcludeHiddenWrites);
}

Optional<Variant> WriteTree::CalcCompleteEventCache(
    const Path& tree_path, const Variant* complete_server_cache,
    const std::vector<WriteId>& write_ids_to_exclude,
    HiddenWriteInclusion include_hidden_writes) const {
  if (write_ids_to_exclude.empty() &&
      (include_hidden_writes == kExcludeHiddenWrites)) {
    Optional<Variant> shadowing_variant = ShadowingWrite(tree_path);
    if (shadowing_variant.has_value()) {
      return shadowing_variant;
    } else {
      CompoundWrite sub_merge = visible_writes_.ChildCompoundWrite(tree_path);
      if (sub_merge.IsEmpty()) {
        return OptionalFromPointer(complete_server_cache);
      } else if (complete_server_cache == nullptr &&
                 !sub_merge.HasCompleteWrite(Path())) {
        // We wouldn't have a complete snapshot, since there's no underlying
        // data and no complete shadow
        return Optional<Variant>();
      } else {
        const Variant* layered_cache;
        if (complete_server_cache != nullptr) {
          layered_cache = complete_server_cache;
        } else {
          layered_cache = &kNullVariant;
        }
        return Optional<Variant>(sub_merge.Apply(*layered_cache));
      }
    }
  } else {
    CompoundWrite merge = visible_writes_.ChildCompoundWrite(tree_path);
    if (!include_hidden_writes && merge.IsEmpty()) {
      return OptionalFromPointer(complete_server_cache);
    } else {
      // If the server cache is nullptr, and we don't have a complete cache,
      // we need to return nullptr
      if (!include_hidden_writes && complete_server_cache == nullptr &&
          !merge.HasCompleteWrite(Path())) {
        return Optional<Variant>();
      } else {
        struct FilterData {
          const std::vector<WriteId>* write_ids_to_exclude;
          const Path* tree_path;
          HiddenWriteInclusion include_hidden_writes;
        } filter_userdata{&write_ids_to_exclude, &tree_path,
                          include_hidden_writes};
        UserWriteRecordPredicateFn filter = [](const UserWriteRecord& write,
                                               void* userdata) {
          const auto filter_data = static_cast<FilterData*>(userdata);
          if (write.visible || filter_data->include_hidden_writes) {
            auto iter = std::find(filter_data->write_ids_to_exclude->begin(),
                                  filter_data->write_ids_to_exclude->end(),
                                  write.write_id);
            if (iter == filter_data->write_ids_to_exclude->end()) {
              return write.path.IsParent(*filter_data->tree_path) ||
                     filter_data->tree_path->IsParent(write.path);
            }
          }
          return false;
        };
        Variant layered_cache;
        CompoundWrite merge_at_path = LayerTree(
            all_writes_, filter, (void*)(&filter_userdata), tree_path);
        layered_cache = complete_server_cache != nullptr
                            ? *complete_server_cache
                            : Variant();
        return Optional<Variant>(merge_at_path.Apply(layered_cache));
      }
    }
  }
}

Variant WriteTree::CalcCompleteEventChildren(
    const Path& tree_path, const Variant& complete_server_children) const {
  Variant complete_children = Variant::Null();
  Optional<Variant> top_level_set =
      visible_writes_.GetCompleteVariant(tree_path);
  if (top_level_set.has_value()) {
    if (top_level_set->is_map()) {
      complete_children = *top_level_set;
    }
  } else {
    // Layer any children we have on top of this.
    // We know we don't have a top-level set, so just enumerate existing
    // children, and apply any updates.
    CompoundWrite merge = visible_writes_.ChildCompoundWrite(tree_path);
    if (complete_server_children.is_map()) {
      for (auto& key_value_pair : complete_server_children.map()) {
        const Variant& key = key_value_pair.first;
        const Variant& value = key_value_pair.second;
        Path key_path(key.string_value());
        Variant* result =
            MakeVariantAtPath(&complete_children, Path(key.string_value()));
        *result = merge.ChildCompoundWrite(key_path).Apply(value);
      }
    }
    // Add any complete children we have from the set.
    for (auto& key_value_pair : merge.GetCompleteChildren()) {
      const Variant& key = key_value_pair.first;
      const Variant& value = key_value_pair.second;
      Variant* result =
          MakeVariantAtPath(&complete_children, Path(key.string_value()));
      *result = value;
    }
  }
  return complete_children;
}

Optional<Variant> WriteTree::CalcEventCacheAfterServerOverwrite(
    const Path& tree_path, const Path& child_path,
    const Variant* existing_local_snap,
    const Variant* existing_server_snap) const {
  // Possibilities:
  //  1. No writes are shadowing. Events should be raised, the snap to be
  //     applied comes from the server data.
  //  2. Some write is completely shadowing. No events to be raised.
  //  3. Is partially shadowed. Events.
  FIREBASE_DEV_ASSERT_MESSAGE(
      existing_local_snap != nullptr || existing_server_snap != nullptr,
      "Either existing_local_snap or existing_server_snap must exist");
  Path path = tree_path.GetChild(child_path);
  if (visible_writes_.HasCompleteWrite(path)) {
    // At this point we can probably guarantee that we're in case 2, meaning
    // no events.
    return Optional<Variant>();
  } else {
    // No complete shadowing. We're either partially shadowing or not
    // shadowing at all.
    CompoundWrite child_merge = visible_writes_.ChildCompoundWrite(path);
    if (child_merge.IsEmpty()) {
      // We're not shadowing at all. Case 1.
      return Optional<Variant>(
          VariantGetChild(existing_server_snap, child_path));
    } else {
      // This could be more efficient if the server_node + updates doesn't
      // change the local_snap However this is tricky to find out, since user
      // updates don't necessary change the server snap, e.g. priority updates
      // on empty nodes, or deep deletes. Another special case is if the
      // server adds nodes, but doesn't change any existing writes. It is
      // therefore not enough to only check if the updates change the
      // server_node. Maybe check if the merge tree contains these special
      // cases and only do a full overwrite in that case?
      return Optional<Variant>(
          child_merge.Apply(VariantGetChild(existing_server_snap, child_path)));
    }
  }
}

Optional<Variant> WriteTree::CalcCompleteChild(
    const Path& tree_path, const std::string& child_key,
    const CacheNode& existing_server_snap) const {
  Path path = tree_path.GetChild(child_key);
  Optional<Variant> shadowing_variant =
      visible_writes_.GetCompleteVariant(path);
  if (shadowing_variant.has_value()) {
    return shadowing_variant;
  } else {
    if (existing_server_snap.IsCompleteForChild(child_key)) {
      CompoundWrite child_merge = visible_writes_.ChildCompoundWrite(path);
      const Variant& child =
          VariantGetChild(&existing_server_snap.variant(), child_key);
      return Optional<Variant>(child_merge.Apply(child));
    }
    return Optional<Variant>();
  }
}

Optional<std::pair<Variant, Variant>> WriteTree::CalcNextVariantAfterPost(
    const Path& tree_path, const Optional<Variant>& complete_server_data,
    const std::pair<Variant, Variant>& post, IterationDirection direction,
    const QueryParams& query_params) const {
  Optional<Variant> to_iterate;
  CompoundWrite merge = visible_writes_.ChildCompoundWrite(tree_path);
  Optional<Variant> shadowing_variant = merge.GetCompleteVariant(Path());
  if (shadowing_variant.has_value()) {
    to_iterate = shadowing_variant;
  } else if (complete_server_data.has_value()) {
    to_iterate = merge.Apply(complete_server_data.value());
  } else {
    // No children to iterate on.
    return Optional<std::pair<Variant, Variant>>();
  }

  Optional<std::pair<Variant, Variant>> current_next;
  const Variant& post_key = post.first;
  const Variant& post_value = post.second;
  for (const auto& key_value : to_iterate->map()) {
    QueryParamsComparator comp(&query_params);
    const Variant& key = key_value.first;
    const Variant& value = key_value.second;
    // If reverse is set, flip the results.
    int d = (direction == kIterateForward) ? 1 : -1;
    if ((d * comp.Compare(key, value, post_key, post_value)) > 0) {
      if (!current_next.has_value()) {
        current_next = key_value;
      } else {
        const Variant& current_key = current_next->first;
        const Variant& current_value = current_next->second;
        if ((d * comp.Compare(key, value, current_key, current_value)) < 0) {
          current_next = key_value;
        }
      }
    }
  }
  return current_next;
}

Optional<Variant> WriteTree::ShadowingWrite(const Path& path) const {
  return visible_writes_.GetCompleteVariant(path);
}

bool WriteTree::RecordContainsPath(const UserWriteRecord& write_record,
                                   const Path& path) {
  if (write_record.is_overwrite) {
    return write_record.path.IsParent(path);
  } else {
    bool result = false;
    write_record.merge.write_tree().CallOnEach(
        Path(), [&](const Path& current_path, Variant) {
          if (write_record.path.GetChild(current_path).IsParent(path)) {
            result = true;
          }
        });
    return result;
  }
}

void WriteTree::ResetTree() {
  visible_writes_ = LayerTree(all_writes_, DefaultFilter, nullptr, Path());
  if (!all_writes_.empty()) {
    last_write_id_ = all_writes_.back().write_id;
  } else {
    last_write_id_ = -1L;
  }
}

CompoundWrite WriteTree::LayerTree(const std::vector<UserWriteRecord>& writes,
                                   WriteTree::UserWriteRecordPredicateFn filter,
                                   void* filter_userdata,
                                   const Path& tree_root) {
  CompoundWrite compound_write = CompoundWrite();
  for (const UserWriteRecord& write : writes) {
    // Theory, a later set will either:
    // a) abort a relevant transaction, so no need to worry about excluding it
    // from calculating that transaction
    // b) not be relevant to a transaction (separate branch), so again will
    // not affect the data for that transaction
    if (filter(write, filter_userdata)) {
      Path write_path = write.path;
      if (write.is_overwrite) {
        if (tree_root.IsParent(write_path)) {
          Optional<Path> relative_path =
              Path::GetRelative(tree_root, write_path);
          compound_write =
              compound_write.AddWrite(*relative_path, write.overwrite);
        } else if (write_path.IsParent(tree_root)) {
          compound_write = compound_write.AddWrite(
              Path(),
              VariantGetChild(&write.overwrite,
                              *Path::GetRelative(write_path, tree_root)));
        } else {
          // There is no overlap between root path and write path, ignore
          // write
        }
      } else {
        if (tree_root.IsParent(write_path)) {
          Optional<Path> relative_path =
              Path::GetRelative(tree_root, write_path);
          compound_write =
              compound_write.AddWrites(*relative_path, write.merge);
        } else if (write_path.IsParent(tree_root)) {
          Optional<Path> relative_path =
              Path::GetRelative(write_path, tree_root);
          if (relative_path->empty()) {
            compound_write = compound_write.AddWrites(Path(), write.merge);
          } else {
            Optional<Variant> deep_node =
                write.merge.GetCompleteVariant(*relative_path);
            if (deep_node.has_value()) {
              compound_write = compound_write.AddWrite(Path(), deep_node);
            }
          }
        } else {
          // There is no overlap between root path and write path, ignore
          // write
        }
      }
    }
  }
  return compound_write;
}

WriteTreeRef::WriteTreeRef(const Path& path, WriteTree* write_tree)
    : path_(path), write_tree_(write_tree) {}

Optional<Variant> WriteTreeRef::CalcCompleteEventCache(
    const Variant* complete_server_cache) const {
  return write_tree_->CalcCompleteEventCache(path_, complete_server_cache);
}

Optional<Variant> WriteTreeRef::CalcCompleteEventCache(
    const Variant* complete_server_cache,
    const std::vector<WriteId>& write_ids_to_exclude) const {
  return write_tree_->CalcCompleteEventCache(path_, complete_server_cache,
                                             write_ids_to_exclude);
}

Optional<Variant> WriteTreeRef::CalcCompleteEventCache(
    const Variant* complete_server_cache,
    const std::vector<WriteId>& write_ids_to_exclude,
    HiddenWriteInclusion include_hidden_writes) const {
  return write_tree_->CalcCompleteEventCache(path_, complete_server_cache,
                                             write_ids_to_exclude,
                                             include_hidden_writes);
}

Variant WriteTreeRef::CalcCompleteEventChildren(
    const Variant& complete_server_children) const {
  return write_tree_->CalcCompleteEventChildren(path_,
                                                complete_server_children);
}

Optional<Variant> WriteTreeRef::CalcEventCacheAfterServerOverwrite(
    const Path& path, const Variant* existing_local_snap,
    const Variant* existing_server_snap) const {
  return write_tree_->CalcEventCacheAfterServerOverwrite(
      path_, path, existing_local_snap, existing_server_snap);
}

Optional<Variant> WriteTreeRef::ShadowingWrite(const Path& path) const {
  return write_tree_->ShadowingWrite(path_.GetChild(path));
}

Optional<Variant> WriteTreeRef::CalcCompleteChild(
    const std::string& child_key,
    const CacheNode& existing_server_cache) const {
  return write_tree_->CalcCompleteChild(path_, child_key,
                                        existing_server_cache);
}

Optional<std::pair<Variant, Variant>> WriteTreeRef::CalcNextVariantAfterPost(
    const Optional<Variant>& complete_server_data,
    const std::pair<Variant, Variant>& start_post, IterationDirection direction,
    const QueryParams& query_params) const {
  return write_tree_->CalcNextVariantAfterPost(
      path_, complete_server_data, start_post, direction, query_params);
}

// Return a WriteTreeRef for a child.
WriteTreeRef WriteTreeRef::Child(const std::string& child_key) const {
  return WriteTreeRef(path_.GetChild(child_key), write_tree_);
}

Path WriteTreeRef::path() { return path_; }

WriteTree* WriteTreeRef::write_tree() { return write_tree_; }

}  // namespace internal
}  // namespace database
}  // namespace firebase
