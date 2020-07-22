#include "firestore/src/include/firebase/firestore/document_snapshot.h"

#include <ostream>
#include <utility>

#include "app/src/assert.h"
#include "firestore/src/common/cleanup.h"
#include "firestore/src/common/util.h"

#include "firestore/src/common/to_string.h"
#include "firestore/src/include/firebase/firestore/document_reference.h"
#include "firestore/src/include/firebase/firestore/field_path.h"
#include "firestore/src/include/firebase/firestore/field_value.h"
#if defined(__ANDROID__)
#include "firestore/src/android/document_snapshot_android.h"
#elif defined(FIRESTORE_STUB_BUILD)
#include "firestore/src/stub/document_snapshot_stub.h"
#else
#include "firestore/src/ios/document_snapshot_ios.h"
#endif  // defined(__ANDROID__)

namespace firebase {
namespace firestore {

using CleanupFnDocumentSnapshot = CleanupFn<DocumentSnapshot>;

DocumentSnapshot::DocumentSnapshot() {}

DocumentSnapshot::DocumentSnapshot(const DocumentSnapshot& snapshot) {
  if (snapshot.internal_) {
    internal_ = new DocumentSnapshotInternal(*snapshot.internal_);
  }
  CleanupFnDocumentSnapshot::Register(this, internal_);
}

DocumentSnapshot::DocumentSnapshot(DocumentSnapshot&& snapshot) {
  CleanupFnDocumentSnapshot::Unregister(&snapshot, snapshot.internal_);
  std::swap(internal_, snapshot.internal_);
  CleanupFnDocumentSnapshot::Register(this, internal_);
}

DocumentSnapshot::DocumentSnapshot(DocumentSnapshotInternal* internal)
    : internal_(internal) {
  FIREBASE_ASSERT(internal != nullptr);
  CleanupFnDocumentSnapshot::Register(this, internal_);
}

DocumentSnapshot::~DocumentSnapshot() {
  CleanupFnDocumentSnapshot::Unregister(this, internal_);
  delete internal_;
  internal_ = nullptr;
}

DocumentSnapshot& DocumentSnapshot::operator=(
    const DocumentSnapshot& snapshot) {
  if (this == &snapshot) {
    return *this;
  }

  CleanupFnDocumentSnapshot::Unregister(this, internal_);
  delete internal_;
  if (snapshot.internal_) {
    internal_ = new DocumentSnapshotInternal(*snapshot.internal_);
  } else {
    internal_ = nullptr;
  }
  CleanupFnDocumentSnapshot::Register(this, internal_);
  return *this;
}

DocumentSnapshot& DocumentSnapshot::operator=(DocumentSnapshot&& snapshot) {
  if (this == &snapshot) {
    return *this;
  }

  CleanupFnDocumentSnapshot::Unregister(&snapshot, snapshot.internal_);
  CleanupFnDocumentSnapshot::Unregister(this, internal_);
  delete internal_;
  internal_ = snapshot.internal_;
  snapshot.internal_ = nullptr;
  CleanupFnDocumentSnapshot::Register(this, internal_);
  return *this;
}

const std::string& DocumentSnapshot::id() const {
  if (!internal_) return EmptyString();
  return internal_->id();
}

DocumentReference DocumentSnapshot::reference() const {
  if (!internal_) return {};
  return internal_->reference();
}

SnapshotMetadata DocumentSnapshot::metadata() const {
  if (!internal_) return SnapshotMetadata{false, false};
  return internal_->metadata();
}

bool DocumentSnapshot::exists() const {
  if (!internal_) return {};
  return internal_->exists();
}

MapFieldValue DocumentSnapshot::GetData(ServerTimestampBehavior stb) const {
  if (!internal_) return MapFieldValue{};
  return internal_->GetData(stb);
}

FieldValue DocumentSnapshot::Get(const char* field,
                                 ServerTimestampBehavior stb) const {
  if (!internal_) return {};
  return internal_->Get(FieldPath::FromDotSeparatedString(field), stb);
}

FieldValue DocumentSnapshot::Get(const std::string& field,
                                 ServerTimestampBehavior stb) const {
  if (!internal_) return {};
  return internal_->Get(FieldPath::FromDotSeparatedString(field), stb);
}

FieldValue DocumentSnapshot::Get(const FieldPath& field,
                                 ServerTimestampBehavior stb) const {
  if (!internal_) return {};
  return internal_->Get(field, stb);
}

std::string DocumentSnapshot::ToString() const {
  if (!internal_) return "DocumentSnapshot(invalid)";

  return std::string("DocumentSnapshot(id=") + id() +
         ", metadata=" + metadata().ToString() +
         ", doc=" + ::firebase::firestore::ToString(GetData()) + ')';
}

std::ostream& operator<<(std::ostream& out, const DocumentSnapshot& document) {
  return out << document.ToString();
}

}  // namespace firestore
}  // namespace firebase
