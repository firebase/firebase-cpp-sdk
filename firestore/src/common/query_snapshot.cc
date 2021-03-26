#include "firestore/src/include/firebase/firestore/query_snapshot.h"

#include <utility>

#include "app/src/assert.h"
#include "firestore/src/common/cleanup.h"

#include "firestore/src/include/firebase/firestore/document_change.h"
#include "firestore/src/include/firebase/firestore/document_snapshot.h"
#include "firestore/src/include/firebase/firestore/query.h"
#if defined(__ANDROID__)
#include "firestore/src/android/query_snapshot_android.h"
#elif defined(FIRESTORE_STUB_BUILD)
#include "firestore/src/stub/query_snapshot_stub.h"
#else
#include "firestore/src/ios/query_snapshot_ios.h"
#endif  // defined(__ANDROID__)

namespace firebase {
namespace firestore {

using CleanupFnQuerySnapshot = CleanupFn<QuerySnapshot>;

QuerySnapshot::QuerySnapshot() {}

QuerySnapshot::QuerySnapshot(const QuerySnapshot& snapshot) {
  if (snapshot.internal_) {
    internal_ = new QuerySnapshotInternal(*snapshot.internal_);
  }
  CleanupFnQuerySnapshot::Register(this, internal_);
}

QuerySnapshot::QuerySnapshot(QuerySnapshot&& snapshot) {
  CleanupFnQuerySnapshot::Unregister(&snapshot, snapshot.internal_);
  std::swap(internal_, snapshot.internal_);
  CleanupFnQuerySnapshot::Register(this, internal_);
}

QuerySnapshot::QuerySnapshot(QuerySnapshotInternal* internal)
    : internal_(internal) {
  FIREBASE_ASSERT(internal != nullptr);
  CleanupFnQuerySnapshot::Register(this, internal_);
}

QuerySnapshot::~QuerySnapshot() {
  CleanupFnQuerySnapshot::Unregister(this, internal_);
  delete internal_;
  internal_ = nullptr;
}

QuerySnapshot& QuerySnapshot::operator=(const QuerySnapshot& snapshot) {
  if (this == &snapshot) {
    return *this;
  }

  CleanupFnQuerySnapshot::Unregister(this, internal_);
  delete internal_;
  if (snapshot.internal_) {
    internal_ = new QuerySnapshotInternal(*snapshot.internal_);
  } else {
    internal_ = nullptr;
  }
  CleanupFnQuerySnapshot::Register(this, internal_);
  return *this;
}

QuerySnapshot& QuerySnapshot::operator=(QuerySnapshot&& snapshot) {
  if (this == &snapshot) {
    return *this;
  }

  CleanupFnQuerySnapshot::Unregister(&snapshot, snapshot.internal_);
  CleanupFnQuerySnapshot::Unregister(this, internal_);
  delete internal_;
  internal_ = snapshot.internal_;
  snapshot.internal_ = nullptr;
  CleanupFnQuerySnapshot::Register(this, internal_);
  return *this;
}

Query QuerySnapshot::query() const {
  if (!internal_) return {};
  return internal_->query();
}

SnapshotMetadata QuerySnapshot::metadata() const {
  if (!internal_) return SnapshotMetadata{false, false};
  return internal_->metadata();
}

std::vector<DocumentChange> QuerySnapshot::DocumentChanges(
    MetadataChanges metadata_changes) const {
  if (!internal_) return std::vector<DocumentChange>{};
  return internal_->DocumentChanges(metadata_changes);
}

std::vector<DocumentSnapshot> QuerySnapshot::documents() const {
  if (!internal_) return std::vector<DocumentSnapshot>{};
  return internal_->documents();
}

std::size_t QuerySnapshot::size() const {
  if (!internal_) return {};
  return internal_->size();
}

}  // namespace firestore
}  // namespace firebase
