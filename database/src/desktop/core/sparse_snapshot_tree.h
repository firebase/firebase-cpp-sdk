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

#ifndef FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_CORE_SPARSE_SNAPSHOT_TREE_H_
#define FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_CORE_SPARSE_SNAPSHOT_TREE_H_

#include "app/src/include/firebase/variant.h"
#include "app/src/path.h"
#include "database/src/desktop/core/tree.h"

namespace firebase {
namespace database {
namespace internal {

class SparseSnapshotTree {
 public:
  SparseSnapshotTree() : value_(), children_() {}

  void Remember(const Path& path, const Variant& data);

  bool Forget(const Path& path);

  void Clear();

  template <typename Func>
  void ForEachTree(const Path& prefix_path, const Func& func) const {
    if (value_.has_value()) {
      func(prefix_path, *value_);
    } else {
      ForEachChild([prefix_path, func](const std::string& key,
                                       const SparseSnapshotTree& tree) {
        tree.ForEachTree(prefix_path.GetChild(key), func);
      });
    }
  }

  template <typename Func>
  void ForEachChild(const Func& func) const {
    for (auto& entry : children_) {
      func(entry.first, entry.second);
    }
  }

 private:
  Optional<Variant> value_;
  std::map<std::string, SparseSnapshotTree> children_;
};

}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_CORE_SPARSE_SNAPSHOT_TREE_H_
