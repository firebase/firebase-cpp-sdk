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

QuerySnapshotInternal::QuerySnapshotInternal(const QuerySnapshotInternal& other)
    : snapshot_{other.snapshot_} {}

QuerySnapshotInternal::QuerySnapshotInternal(
    QuerySnapshotInternal&& other) noexcept
    : snapshot_{std::move(other.snapshot_)} {}

QuerySnapshotInternal& QuerySnapshotInternal::operator=(
    const QuerySnapshotInternal& other) {
  if (this != &other) {
    std::lock_guard<std::mutex> lock(mutex_);
    snapshot_ = other.snapshot_;
    document_changes_exclude_metadata_.reset();
    document_changes_include_metadata_.reset();
    documents_.reset();
  }
  return *this;
}

QuerySnapshotInternal& QuerySnapshotInternal::operator=(
    QuerySnapshotInternal&& other) noexcept {
  if (this != &other) {
    std::lock_guard<std::mutex> lock(mutex_);
    snapshot_ = std::move(other.snapshot_);
    document_changes_exclude_metadata_.reset();
    document_changes_include_metadata_.reset();
    documents_.reset();
  }
  return *this;
}

FirestoreInternal* QuerySnapshotInternal::firestore_internal() {
  return GetFirestoreInternal(&snapshot_);
}

const FirestoreInternal* QuerySnapshotInternal::firestore_internal() const {
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
  std::lock_guard<std::mutex> lock(mutex_);
  auto& cache_slot = include_metadata ? document_changes_include_metadata_
                                      : document_changes_exclude_metadata_;

  if (!cache_slot) {
    std::vector<DocumentChange> result;
    snapshot_.ForEachChange(include_metadata,
                            [&result](api::DocumentChange change) {
                              result.push_back(MakePublic(std::move(change)));
                            });

    cache_slot = std::move(result);
  }

  return cache_slot.value();
}

std::vector<DocumentSnapshot> QuerySnapshotInternal::documents() const {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!documents_) {
    std::vector<DocumentSnapshot> result;
    result.reserve(snapshot_.size());
    snapshot_.ForEachDocument([&result](api::DocumentSnapshot snapshot) {
      result.push_back(MakePublic(std::move(snapshot)));
    });

    documents_ = std::move(result);
  }

  return documents_.value();
}

bool operator==(const QuerySnapshotInternal& lhs,
                const QuerySnapshotInternal& rhs) {
  return lhs.snapshot_ == rhs.snapshot_;
}

}  // namespace firestore
}  // namespace firebase
