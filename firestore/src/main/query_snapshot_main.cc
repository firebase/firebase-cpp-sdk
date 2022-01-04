/*
 * Copyright 2021 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "firestore/src/main/query_snapshot_main.h"

#include <utility>

#include "firestore/src/main/converter_main.h"
#include "firestore/src/main/document_change_main.h"
#include "firestore/src/main/document_snapshot_main.h"
#include "firestore/src/main/query_main.h"
#include "firestore/src/main/util_main.h"

namespace firebase {
namespace firestore {

QuerySnapshotInternal::QuerySnapshotInternal(api::QuerySnapshot&& snapshot)
    : snapshot_{std::move(snapshot)} {}

FirestoreInternal* QuerySnapshotInternal::firestore_internal() {
  return GetFirestoreInternal(&snapshot_);
}

Query QuerySnapshotInternal::query() const {
  return MakePublic(snapshot_.query());
}

SnapshotMetadata QuerySnapshotInternal::metadata() const {
  const auto& result = snapshot_.metadata();
  return SnapshotMetadata{result.pending_writes(), result.from_cache()};
}

std::size_t QuerySnapshotInternal::size() const { return snapshot_.size(); }

std::vector<DocumentChange> QuerySnapshotInternal::DocumentChanges(
    MetadataChanges metadata_changes) const {
  bool include_metadata = metadata_changes == MetadataChanges::kInclude;

  if (!document_changes_ || changes_include_metadata_ != include_metadata) {
    std::vector<DocumentChange> result;
    snapshot_.ForEachChange(include_metadata,
                            [&result](api::DocumentChange change) {
                              result.push_back(MakePublic(std::move(change)));
                            });

    document_changes_ = std::move(result);
    changes_include_metadata_ = include_metadata;
  }

  return document_changes_.value();
}

std::vector<DocumentSnapshot> QuerySnapshotInternal::documents() const {
  if (!documents_) {
    std::vector<DocumentSnapshot> result;
    snapshot_.ForEachDocument([&result](api::DocumentSnapshot snapshot) {
      result.push_back(MakePublic(std::move(snapshot)));
    });

    documents_ = result;
  }

  return documents_.value();
}

bool operator==(const QuerySnapshotInternal& lhs,
                const QuerySnapshotInternal& rhs) {
  return lhs.snapshot_ == rhs.snapshot_;
}

}  // namespace firestore
}  // namespace firebase
