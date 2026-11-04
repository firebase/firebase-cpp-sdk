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

#ifndef FIREBASE_DATABASE_SRC_DESKTOP_CORE_WRITE_TREE_H_
#define FIREBASE_DATABASE_SRC_DESKTOP_CORE_WRITE_TREE_H_

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "app/src/include/firebase/variant.h"
#include "app/src/path.h"
#include "database/src/desktop/core/compound_write.h"
#include "database/src/desktop/persistence/persistence_storage_engine.h"
#include "database/src/desktop/view/view_cache.h"

namespace firebase {
namespace database {
namespace internal {

class WriteTree;
class WriteTreeRef;

enum OverwriteVisibility {
  kOverwriteInvisible = false,
  kOverwriteVisible = true,
};

enum HiddenWriteInclusion {
  kExcludeHiddenWrites = false,
  kIncludeHiddenWrites = true,
};

enum IterationDirection {
  kIterateForward = false,
  kIterateReverse = true,
};

class WriteTreeRef {
 public:
  WriteTreeRef(const Path& path, WriteTree* write_tree)
      : path_(path), write_tree_(write_tree) {}

  std::optional<Variant> CalcCompleteEventCache(
      const Variant* complete_server_cache) const;
  std::optional<Variant> CalcCompleteEventCache(
      const Variant* complete_server_cache,
      const std::vector<WriteId>& write_ids_to_exclude) const;
  std::optional<Variant> CalcCompleteEventCache(
      const Variant* complete_server_cache,
      const std::vector<WriteId>& write_ids_to_exclude,
      HiddenWriteInclusion include_hidden_writes) const;
  Variant CalcCompleteEventChildren(
      const Variant& complete_server_children) const;
  std::optional<Variant> CalcEventCacheAfterServerOverwrite(
      const Path& path, const Variant* existing_local_snap,
      const Variant* existing_server_snap) const;
  std::optional<Variant> ShadowingWrite(const Path& path) const;
  std::optional<Variant> CalcCompleteChild(
      const std::string& child_key,
      const CacheNode& existing_server_cache) const;
  std::optional<std::pair<Variant, Variant>> CalcNextVariantAfterPost(
      const std::optional<Variant>& complete_server_data,
      const std::pair<Variant, Variant>& start_post,
      IterationDirection direction, const QueryParams& query_params) const;

  WriteTreeRef Child(const std::string& child_key) const {
    return WriteTreeRef(path_.GetChild(child_key), write_tree_);
  }
  Path path() { return path_; }
  WriteTree* write_tree() { return write_tree_; }

 private:
  Path path_;
  WriteTree* write_tree_;
};

class WriteTree {
 public:
  WriteTree() : last_write_id_(-1L) {}
  virtual ~WriteTree() = default;

  WriteTreeRef ChildWrites(const Path& path) {
    return WriteTreeRef(path, this);
  }

  void AddOverwrite(const Path& path, const Variant& snap, WriteId write_id,
                    OverwriteVisibility visibility);
  void AddMerge(const Path& path, const CompoundWrite& changed_children,
                WriteId write_id);
  UserWriteRecord* GetWrite(WriteId write_id);
  std::vector<UserWriteRecord> PurgeAllWrites();
  bool RemoveWrite(WriteId write_id);
  std::optional<Variant> GetCompleteWriteData(const Path& path) const {
    return visible_writes_.GetCompleteVariant(path);
  }

  virtual std::optional<Variant> CalcCompleteEventCache(
      const Path& tree_path, const Variant* complete_server_cache) const {
    return CalcCompleteEventCache(tree_path, complete_server_cache, {});
  }
  virtual std::optional<Variant> CalcCompleteEventCache(
      const Path& tree_path, const Variant* complete_server_cache,
      const std::vector<WriteId>& write_ids_to_exclude) const {
    return CalcCompleteEventCache(tree_path, complete_server_cache,
                                  write_ids_to_exclude, kExcludeHiddenWrites);
  }
  virtual std::optional<Variant> CalcCompleteEventCache(
      const Path& tree_path, const Variant* complete_server_cache,
      const std::vector<WriteId>& write_ids_to_exclude,
      HiddenWriteInclusion include_hidden_writes) const;
  virtual Variant CalcCompleteEventChildren(
      const Path& tree_path, const Variant& complete_server_children) const;
  virtual std::optional<Variant> CalcEventCacheAfterServerOverwrite(
      const Path& tree_path, const Path& child_path,
      const Variant* existing_local_snap,
      const Variant* existing_server_snap) const;
  virtual std::optional<Variant> CalcCompleteChild(
      const Path& tree_path, const std::string& child_key,
      const CacheNode& existing_server_snap) const;
  virtual std::optional<std::pair<Variant, Variant>> CalcNextVariantAfterPost(
      const Path& tree_path, const std::optional<Variant>& complete_server_data,
      const std::pair<Variant, Variant>& post, IterationDirection direction,
      const QueryParams& params) const;
  virtual std::optional<Variant> ShadowingWrite(const Path& path) const {
    return visible_writes_.GetCompleteVariant(path);
  }

 private:
  bool RecordContainsPath(const UserWriteRecord& write_record,
                          const Path& path);
  void ResetTree();

  using UserWriteRecordPredicateFn = bool (*)(const UserWriteRecord& record,
                                              void* userdata);
  static CompoundWrite LayerTree(const std::vector<UserWriteRecord>& writes,
                                 UserWriteRecordPredicateFn filter,
                                 void* userdata, const Path& tree_root);

  CompoundWrite visible_writes_;
  std::vector<UserWriteRecord> all_writes_;
  WriteId last_write_id_;
};

}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_SRC_DESKTOP_CORE_WRITE_TREE_H_
