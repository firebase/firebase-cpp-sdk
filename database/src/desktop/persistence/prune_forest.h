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

#ifndef FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_PERSISTENCE_PRUNE_FOREST_H_
#define FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_PERSISTENCE_PRUNE_FOREST_H_

#include <set>
#include <string>

#include "app/src/path.h"
#include "database/src/desktop/core/tree.h"

namespace firebase {
namespace database {
namespace internal {

// Forest of "prune trees" where a prune tree is a location that can be pruned
// with a tree of descendants that must be excluded from the pruning.
//
// Internally we store this as a single tree of bools with the following
// characteristics:
//
//   * 'true' indicates a location that can be pruned, possibly
//     with some excluded descendants.
//   * 'false' indicates a location that we should keep (i.e. exclude from
//     pruning).
//   * 'true' (prune) cannot be a descendant of 'false' (keep). This will
//     trigger an exception.
//   * 'true' cannot be a descendant of 'true' (we'll just keep the more shallow
//     'true').
//   * 'false' cannot be a descendant of 'false' (we'll just keep the more
//     shallow 'false').
//
typedef Tree<bool> PruneForest;

// A PruneTreeRef is a way to operate on a PruneTree, treating any node as the
// root. It provides fucntions to set or keep various locations, as well as
// GetChild allow you to drill into the children of a location in the tree.
// PruneForestRef is a lightweight object that does not take ownership of the
// PruneForest passed to it.
class PruneForestRef {
 public:
  explicit PruneForestRef(PruneForest* prune_forest);

  // Check if this PruneForestRef is equal to another PruneForestRef. This is
  // mostly used for testing.
  bool operator==(const PruneForestRef& other) const;

  // Check if this PruneForestRef is not equal to another PruneForestRef. This
  // is mostly used for testing.
  bool operator!=(const PruneForestRef& other) const {
    return !(*this == other);
  }

  // Returns true if this PruneForestRef prunes anything.
  bool PrunesAnything() const;

  // Indicates that path is marked for pruning, so anything below it that didn't
  // have keep() called on it should be pruned.
  //
  // @param path The path in question
  // @return True if we should prune descendants that didn't have keep() called
  // on them.
  bool ShouldPruneUnkeptDescendants(const Path& path) const;

  // Returns true if the given path should be kept.
  bool ShouldKeep(const Path& path) const;

  // Returns true if the given path should be pruned.
  bool AffectsPath(const Path& path) const;

  // Get the child of this tree at the given key.
  PruneForestRef GetChild(const std::string& key);
  const PruneForestRef GetChild(const std::string& key) const;

  // Get the child of this tree at the given path.
  PruneForestRef GetChild(const Path& path);
  const PruneForestRef GetChild(const Path& path) const;

  // Mark that the value at the given path should be pruned.
  void Prune(const Path& path);

  // Mark that the value at the given path should be kept.
  void Keep(const Path& path);

  // Mark that the given child values at the given path should be kept.
  void KeepAll(const Path& path, const std::set<std::string>& children);

  // Mark that the given child values at the given path should be pruned.
  void PruneAll(const Path& path, const std::set<std::string>& children);

  // Accumulate a result given a visitor function to be applied to all nodes
  // that are marked as being kept.
  template <typename T, typename Func>
  T FoldKeptNodes(const T& start_value, const Func& visitor) const {
    return prune_forest_->Fold(
        start_value,
        [visitor](const Path& relative_path, bool prune, T accum) -> T {
          if (!prune) {
            return visitor(relative_path, accum);
          } else {
            return accum;
          }
        });
  }

 private:
  void DoAll(const Path& path, const std::set<std::string>& children,
             const Tree<bool>& keep_or_prune_tree);

  PruneForest* prune_forest_;
};

}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_PERSISTENCE_PRUNE_FOREST_H_
