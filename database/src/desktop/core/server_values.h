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

#ifndef FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_CORE_SERVER_VALUES_H_
#define FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_CORE_SERVER_VALUES_H_

#include "app/src/include/firebase/variant.h"
#include "database/src/desktop/core/compound_write.h"
#include "database/src/desktop/core/sparse_snapshot_tree.h"

namespace firebase {
namespace database {
namespace internal {

Variant GenerateServerValues(int64_t server_time_offset);

const Variant& ResolveDeferredValue(const Variant& value,
                                    const Variant& server_values);

SparseSnapshotTree ResolveDeferredValueTree(const SparseSnapshotTree& tree,
                                            const Variant& server_values);

Variant ResolveDeferredValueSnapshot(const Variant& data,
                                     const Variant& server_values);

CompoundWrite ResolveDeferredValueMerge(const CompoundWrite& merge,
                                        const Variant& server_values);

}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_CORE_SERVER_VALUES_H_
