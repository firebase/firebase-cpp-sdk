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

#include "database/src/desktop/core/sparse_snapshot_tree.h"
#include "app/src/include/firebase/variant.h"
#include "app/src/path.h"
#include "database/src/desktop/core/tree.h"

namespace firebase {
namespace database {
namespace internal {

void SparseSnapshotTree::Remember(const Path& path, const Variant& data) {
  if (path.empty()) {
    value_ = data;
    children_.clear();
  } else if (value_.has_value()) {
    SetVariantAtPath(&value_.value(), path, data);
  } else {
    Path child_key = path.FrontDirectory();
    children_[child_key.str()].Remember(path.PopFrontDirectory(), data);
  }
}

bool SparseSnapshotTree::Forget(const Path& path) {
  if (path.empty()) {
    value_.reset();
    children_.clear();
    return true;
  } else {
    if (value_.has_value()) {
      if (VariantIsLeaf(*value_)) {
        // Non-empty path at leaf. The path leads to nowhere.
        return false;
      } else {
        Variant local_value = *value_;
        value_.reset();

        if (local_value.is_map()) {
          for (const auto& kvp : local_value.map()) {
            const Variant& key = kvp.first;
            const Variant& value = kvp.second;
            Remember(Path(key.AsString().string_value()), value);
          }
        }
        // We've cleared out the value and set the children. Call this method
        // again to hit the next case.
        return Forget(path);
      }
    } else {
      Path child_key = path.FrontDirectory();
      Path child_path = path.PopFrontDirectory();

      auto iter = children_.find(child_key.str());
      if (iter != children_.end()) {
        SparseSnapshotTree& child = iter->second;
        bool safe_to_remove = child.Forget(child_path);
        if (safe_to_remove) {
          children_.erase(iter);
        }
      }
      if (children_.empty()) {
        return true;
      } else {
        return false;
      }
    }
  }
  return true;
}

void SparseSnapshotTree::Clear() {
  value_.reset();
  children_.clear();
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
