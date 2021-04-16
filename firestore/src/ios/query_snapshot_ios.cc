#include "firestore/src/ios/query_snapshot_ios.h"

#include <utility>

#include "firestore/src/ios/converter_ios.h"
#include "firestore/src/ios/document_change_ios.h"
#include "firestore/src/ios/document_snapshot_ios.h"
#include "firestore/src/ios/query_ios.h"
#include "firestore/src/ios/util_ios.h"

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

}  // namespace firestore
}  // namespace firebase
