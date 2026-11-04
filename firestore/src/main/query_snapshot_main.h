// Copyright 2021 Google LLC
#ifndef FIREBASE_FIRESTORE_SRC_MAIN_QUERY_SNAPSHOT_MAIN_H_
#define FIREBASE_FIRESTORE_SRC_MAIN_QUERY_SNAPSHOT_MAIN_H_

#include <cstddef>
#include <mutex>
#include <optional>
#include <vector>
#include "Firestore/core/src/api/query_snapshot.h"
#include "firestore/src/include/firebase/firestore/document_change.h"
#include "firestore/src/include/firebase/firestore/document_snapshot.h"
#include "firestore/src/include/firebase/firestore/metadata_changes.h"

#if defined(__ANDROID__)
#error "This header should not be used on Android."
#endif

namespace firebase::firestore {

class Query;
class SnapshotMetadata;
class FirestoreInternal;

class QuerySnapshotInternal {
 public:
  explicit QuerySnapshotInternal(api::QuerySnapshot&& snapshot);
  QuerySnapshotInternal(const QuerySnapshotInternal& other);
  ~QuerySnapshotInternal();
  QuerySnapshotInternal(QuerySnapshotInternal&& other) noexcept;
  QuerySnapshotInternal& operator=(const QuerySnapshotInternal& other);
  QuerySnapshotInternal& operator=(QuerySnapshotInternal&& other) noexcept;

  FirestoreInternal* firestore_internal();
  const FirestoreInternal* firestore_internal() const;

  Query query() const;
  SnapshotMetadata metadata() const;
  std::size_t size() const;

  std::vector<DocumentChange> DocumentChanges(MetadataChanges metadata_changes) const;
  std::vector<DocumentSnapshot> documents() const;
  std::size_t Hash() const { return snapshot_.Hash(); }

  friend bool operator==(const QuerySnapshotInternal& lhs, const QuerySnapshotInternal& rhs);

 private:
  api::QuerySnapshot snapshot_;
  mutable std::mutex mutex_;
  mutable std::optional<std::vector<DocumentChange>> document_changes_exclude_metadata_;
  mutable std::optional<std::vector<DocumentChange>> document_changes_include_metadata_;
  mutable std::optional<std::vector<DocumentSnapshot>> documents_;
};

inline bool operator!=(const QuerySnapshotInternal& lhs, const QuerySnapshotInternal& rhs) {
  return !(lhs == rhs);
}

}  // namespace firebase::firestore

#endif  // FIREBASE_FIRESTORE_SRC_MAIN_QUERY_SNAPSHOT_MAIN_H_
