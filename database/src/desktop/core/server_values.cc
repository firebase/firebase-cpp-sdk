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

#include "database/src/desktop/core/server_values.h"

#include <ctime>

#include "app/src/include/firebase/variant.h"
#include "database/src/desktop/core/compound_write.h"
#include "database/src/desktop/util_desktop.h"

namespace firebase {
namespace database {
namespace internal {

static const char kNameSubkeyServerValue[] = ".sv";

Variant GenerateServerValues(int64_t server_time_offset) {
  Variant server_values = Variant::EmptyMap();
  int64_t corrected_time = time(nullptr) * 1000L + server_time_offset;
  server_values.map()["timestamp"] = Variant::FromInt64(corrected_time);
  return server_values;
}

const Variant& ResolveDeferredValue(const Variant& value,
                                    const Variant& server_values) {
  const Variant* key = GetInternalVariant(&value, kNameSubkeyServerValue);
  if (key) {
    const Variant* resolved_value = GetInternalVariant(&server_values, *key);
    if (resolved_value) {
      return *resolved_value;
    }
  }
  return value;
}

SparseSnapshotTree ResolveDeferredValueTree(const SparseSnapshotTree& tree,
                                            const Variant& server_values) {
  SparseSnapshotTree resolved_tree;
  tree.ForEachTree(Path(), [&resolved_tree, &tree, &server_values](
                               const Path& prefix_path, const Variant& node) {
    resolved_tree.Remember(prefix_path,
                           ResolveDeferredValueSnapshot(node, server_values));
  });
  return resolved_tree;
}

static void ResolveDeferredValueSnapshotHelper(Variant* data,
                                               const Variant& server_values) {
  // If this is a server value, update it.
  *data = ResolveDeferredValue(*data, server_values);

  // If it wasn't a server value, recurse into child nodes and update them as
  // necessary.
  if (data->is_map()) {
    for (auto& kvp : data->map()) {
      ResolveDeferredValueSnapshotHelper(&kvp.second, server_values);
    }
  }
}

Variant ResolveDeferredValueSnapshot(const Variant& data,
                                     const Variant& server_values) {
  Variant priority = GetVariantPriority(data);
  priority = ResolveDeferredValue(priority, server_values);
  Variant new_data = data;
  ResolveDeferredValueSnapshotHelper(&new_data, server_values);
  return CombineValueAndPriority(new_data, priority);
}

CompoundWrite ResolveDeferredValueMerge(const CompoundWrite& merge,
                                        const Variant& server_values) {
  return merge.write_tree().Fold(
      Path(),
      [&server_values](const Path& path, const Variant& child,
                       const CompoundWrite& accum) {
        return accum.AddWrite(
            path, ResolveDeferredValueSnapshot(child, server_values));
      },
      CompoundWrite());
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
