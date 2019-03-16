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

#ifndef FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_CORE_WRITE_TREE_H_
#define FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_CORE_WRITE_TREE_H_

#include <string>
#include <utility>
#include <vector>
#include "app/src/include/firebase/variant.h"
#include "app/src/optional.h"
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

// A WriteTreeRef wraps a WriteTree and a path, for convenient access to a
// particular subtree. All of the methods just proxy to the underlying
// WriteTree.
class WriteTreeRef {
 public:
  WriteTreeRef(const Path& path, WriteTree* write_tree);

  Optional<Variant> CalcCompleteEventCache(
      const Variant* complete_server_cache) const;

  Optional<Variant> CalcCompleteEventCache(
      const Variant* complete_server_cache,
      const std::vector<WriteId>& write_ids_to_exclude) const;

  Optional<Variant> CalcCompleteEventCache(
      const Variant* complete_server_cache,
      const std::vector<WriteId>& write_ids_to_exclude,
      HiddenWriteInclusion include_hidden_writes) const;

  Variant CalcCompleteEventChildren(
      const Variant& complete_server_children) const;

  Optional<Variant> CalcEventCacheAfterServerOverwrite(
      const Path& path, const Variant* existing_local_snap,
      const Variant* existing_server_snap) const;

  Optional<Variant> ShadowingWrite(const Path& path) const;

  Optional<Variant> CalcCompleteChild(
      const std::string& child_key,
      const CacheNode& existing_server_cache) const;

  Optional<std::pair<Variant, Variant>> CalcNextVariantAfterPost(
      const Optional<Variant>& complete_server_data,
      const std::pair<Variant, Variant>& start_post,
      IterationDirection direction, const QueryParams& query_params) const;

  // Return a WriteTreeRef for a child.
  WriteTreeRef Child(const std::string& child_key) const;

  Path path();

  WriteTree* write_tree();

 private:
  // The path to this particular write tree ref. Used for calling methods on
  // WriteTree while exposing a simpler interface to callers.
  Path path_;

  // A reference to the actual tree of write data. All methods are pass-through
  // to the tree, but with the appropriate path prefixed.
  //
  // This lets us make cheap references to points in the tree for sync points
  // without having to copy and maintain all of the data.
  WriteTree* write_tree_;
};

// Defines a single user-initiated write operation. May be the result of a
// set(), transaction(), or update() call. In the case of a set() or
// transaction, snap wil be non-null. In the case of an update(), children will
// be non-null.
//
// Some functions are marked as virtual so that they can be mocked and used in
// unit tests.
class WriteTree {
 public:
  // WriteTree tracks all pending user-initiated writes and has methods to
  // calculate the result of merging them with underlying server data (to create
  // "event cache" data). Pending writes are added with AddOverwrite() and
  // AddMerge(), and removed with RemoveWrite().
  WriteTree() : visible_writes_(), all_writes_(), last_write_id_(-1L) {}

  virtual ~WriteTree() {}

  // Create a new WriteTreeRef for the given path. For use with a new sync point
  // at the given path.
  WriteTreeRef ChildWrites(const Path& path);

  // Record a new overwrite from user code. The new overwrite must have a higher
  // WriteId than all previous Overwrites or Merges.
  void AddOverwrite(const Path& path, const Variant& snap, WriteId write_id,
                    OverwriteVisibility visibility);

  // Record a new merge from user code. The new merge must have a higher
  // WriteId than all previous Overwrites or Merges.

  void AddMerge(const Path& path, const CompoundWrite& changed_children,
                WriteId write_id);

  // Returns the UserWriteRecord associated with the given WriteId
  UserWriteRecord* GetWrite(WriteId write_id);

  // Resets all writes in this write tree, and return the UserWriteRecordds of
  // the writes that were purged.
  std::vector<UserWriteRecord> PurgeAllWrites();

  // Remove a write (either an overwrite or merge) that has been successfully
  // acknowledge by the server. Recalculates the tree if necessary. We return
  // whether the write may have been visible, meaning views need to reevaluate.
  //
  // @return true if the write may have been visible (meaning we'll need to
  // reevaluate / raise events as a result).
  bool RemoveWrite(WriteId write_id);

  // Return a complete snapshot for the given path if there's visible write data
  // at that path, else nullptr. No server data is considered.
  Optional<Variant> GetCompleteWriteData(const Path& path) const;

  // Given optional, underlying server data, and an optional set of constraints
  // (exclude some sets, include hidden writes), attempt to calculate a complete
  // snapshot for the given path
  virtual Optional<Variant> CalcCompleteEventCache(
      const Path& tree_path, const Variant* complete_server_cache) const;

  virtual Optional<Variant> CalcCompleteEventCache(
      const Path& tree_path, const Variant* complete_server_cache,
      const std::vector<WriteId>& write_ids_to_exclude) const;

  virtual Optional<Variant> CalcCompleteEventCache(
      const Path& tree_path, const Variant* complete_server_cache,
      const std::vector<WriteId>& write_ids_to_exclude,
      HiddenWriteInclusion include_hidden_writes) const;

  // With underlying server data, attempt to return a children node of children
  // that we have complete data for. Used when creating new views, to pre-fill
  // their complete event children snapshot.
  virtual Variant CalcCompleteEventChildren(
      const Path& tree_path, const Variant& complete_server_children) const;

  // Given that the underlying server data has updated, determine what, if
  // anything, needs to be applied to the event cache.
  //
  // Possibilities:
  //
  //  1. No writes are shadowing. Events should be raised, the snap to be
  //     applied comes from the server data.
  //
  //  2. Some write is completely shadowing. No events to be raised.
  //
  //  3. Is partially shadowed. Events should be raised.
  //
  // Either existing_local_snap or existing_server_snap must exist.
  virtual Optional<Variant> CalcEventCacheAfterServerOverwrite(
      const Path& tree_path, const Path& child_path,
      const Variant* existing_local_snap,
      const Variant* existing_server_snap) const;

  // Returns a complete child for a given server snap after applying all user
  // writes or nothing if there is no complete child for this std::string.
  virtual Optional<Variant> CalcCompleteChild(
      const Path& tree_path, const std::string& child_key,
      const CacheNode& existing_server_snap) const;

  // This method is used when processing child remove events on a query. If we
  // can, we pull in children that were outside the window, but may now be in
  // the window.
  virtual Optional<std::pair<Variant, Variant>> CalcNextVariantAfterPost(
      const Path& tree_path, const Optional<Variant>& complete_server_data,
      const std::pair<Variant, Variant>& post, IterationDirection direction,
      const QueryParams& params) const;

  // Returns a node if there is a complete overwrite for this path. More
  // specifically, if there is a write at a higher path, this will return the
  // child of that write relative to the write and this path. Returns nullptr if
  // there is no write at this path.
  virtual Optional<Variant> ShadowingWrite(const Path& path) const;

 private:
  bool RecordContainsPath(const UserWriteRecord& write_record,
                          const Path& path);

  // Re-layer the writes and merges into a tree so we can efficiently calculate
  // event snapshots
  void ResetTree();

  typedef bool (*UserWriteRecordPredicateFn)(const UserWriteRecord& record,
                                             void* userdata);

  // Static method. Given an array of WriteRecords, a filter for which ones to
  // include, and a path, construct a merge at that path.
  static CompoundWrite LayerTree(const std::vector<UserWriteRecord>& writes,
                                 UserWriteRecordPredicateFn filter,
                                 void* userdata, const Path& tree_root);

  // A tree tracking the result of applying all visible writes. This does not
  // include transactions with apply_locally=false or writes that are completely
  // shadowed by other writes.
  CompoundWrite visible_writes_;

  // A list of all pending writes, regardless of visibility and shadowed-ness.
  // Used to calculate arbitrary sets of the changed data, such as hidden writes
  // (from transactions) or changes with certain writes excluded (also used by
  // transactions).
  std::vector<UserWriteRecord> all_writes_;

  // The last WriteId seen by the tree through AddOverwrite or AddMerge. The
  // The WriteId passed to these functions should always be larger than the last
  // one seen.
  WriteId last_write_id_;
};

}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_CORE_WRITE_TREE_H_
