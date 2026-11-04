// Copyright 2020 Google LLC
#include "firestore/src/include/firebase/firestore/document_snapshot.h"

#include <ostream>
#include <utility>
#include "firestore/src/common/cleanup.h"
#include "firestore/src/common/exception_common.h"
#include "firestore/src/common/hard_assert_common.h"
#include "firestore/src/common/to_string.h"
#include "firestore/src/common/util.h"
#include "firestore/src/include/firebase/firestore/document_reference.h"
#include "firestore/src/include/firebase/firestore/field_path.h"
#include "firestore/src/include/firebase/firestore/field_value.h"
#if defined(__ANDROID__)
#include "firestore/src/android/document_snapshot_android.h"
#else
#include "firestore/src/main/document_snapshot_main.h"
#endif

namespace firebase::firestore {

using CleanupFnDocumentSnapshot = CleanupFn<DocumentSnapshot>;

DocumentSnapshot::DocumentSnapshot() = default;

DocumentSnapshot::DocumentSnapshot(const DocumentSnapshot& snapshot)
    : internal_(snapshot.internal_ ? new DocumentSnapshotInternal(*snapshot.internal_) : nullptr) {
#if FIRESTORE_HAVE_EXCEPTIONS
  try {
    CleanupFnDocumentSnapshot::Register(this, internal_);
  } catch (...) {
    delete internal_;
    throw;
  }
#else
  CleanupFnDocumentSnapshot::Register(this, internal_);
#endif
}

DocumentSnapshot::DocumentSnapshot(DocumentSnapshot&& snapshot) noexcept {
  CleanupFnDocumentSnapshot::Unregister(&snapshot, snapshot.internal_);
  internal_ = std::exchange(snapshot.internal_, nullptr);
  CleanupFnDocumentSnapshot::Register(this, internal_);
}

DocumentSnapshot::DocumentSnapshot(DocumentSnapshotInternal* internal) : internal_(internal) {
  SIMPLE_HARD_ASSERT(internal != nullptr);
#if FIRESTORE_HAVE_EXCEPTIONS
  try {
    CleanupFnDocumentSnapshot::Register(this, internal_);
  } catch (...) {
    delete internal_;
    throw;
  }
#else
  CleanupFnDocumentSnapshot::Register(this, internal_);
#endif
}

DocumentSnapshot::~DocumentSnapshot() {
  CleanupFnDocumentSnapshot::Unregister(this, internal_);
  delete std::exchange(internal_, nullptr);
}

DocumentSnapshot& DocumentSnapshot::operator=(const DocumentSnapshot& snapshot) {
  if (this == &snapshot) return *this;
  DocumentSnapshotInternal* new_internal =
      snapshot.internal_ ? new DocumentSnapshotInternal(*snapshot.internal_) : nullptr;
  CleanupFnDocumentSnapshot::Unregister(this, internal_);
  delete std::exchange(internal_, new_internal);
#if FIRESTORE_HAVE_EXCEPTIONS
  try {
    CleanupFnDocumentSnapshot::Register(this, internal_);
  } catch (...) {
    delete std::exchange(internal_, nullptr);
    throw;
  }
#else
  CleanupFnDocumentSnapshot::Register(this, internal_);
#endif
  return *this;
}

DocumentSnapshot& DocumentSnapshot::operator=(DocumentSnapshot&& snapshot) noexcept {
  if (this == &snapshot) return *this;
  CleanupFnDocumentSnapshot::Unregister(&snapshot, snapshot.internal_);
  CleanupFnDocumentSnapshot::Unregister(this, internal_);
  delete std::exchange(internal_, std::exchange(snapshot.internal_, nullptr));
  CleanupFnDocumentSnapshot::Register(this, internal_);
  return *this;
}

const std::string& DocumentSnapshot::id() const {
  return internal_ ? internal_->id() : EmptyString();
}
DocumentReference DocumentSnapshot::reference() const {
  return internal_ ? internal_->reference() : DocumentReference{};
}
SnapshotMetadata DocumentSnapshot::metadata() const {
  return internal_ ? internal_->metadata() : SnapshotMetadata{false, false};
}
bool DocumentSnapshot::exists() const { return internal_ ? internal_->exists() : false; }

MapFieldValue DocumentSnapshot::GetData(ServerTimestampBehavior stb) const {
  return internal_ ? internal_->GetData(stb) : MapFieldValue{};
}

FieldValue DocumentSnapshot::Get(const char* field, ServerTimestampBehavior stb) const {
  if (!field) {
    SimpleThrowInvalidArgument("Field name cannot be null.");
    return {};
  }
  return Get(std::string(field), stb);
}

FieldValue DocumentSnapshot::Get(const std::string& field, ServerTimestampBehavior stb) const {
  return internal_ ? internal_->Get(FieldPath::FromDotSeparatedString(field), stb) : FieldValue{};
}

FieldValue DocumentSnapshot::Get(const FieldPath& field, ServerTimestampBehavior stb) const {
  if (!field.is_valid()) {
    SimpleThrowInvalidArgument("Field path cannot be invalid.");
    return {};
  }
  return internal_ ? internal_->Get(field, stb) : FieldValue{};
}

std::string DocumentSnapshot::ToString() const {
  if (!internal_) return "DocumentSnapshot(invalid)";
  return std::string("DocumentSnapshot(id=") + id() + ", metadata=" + metadata().ToString() +
         ", doc=" + (exists() ? ::firebase::firestore::ToString(GetData()) : "none") + ')';
}

std::size_t DocumentSnapshot::Hash() const { return internal_ ? internal_->Hash() : 0; }

std::ostream& operator<<(std::ostream& out, const DocumentSnapshot& document) {
  return out << document.ToString();
}

bool operator==(const DocumentSnapshot& lhs, const DocumentSnapshot& rhs) {
  return EqualityCompare(lhs.internal_, rhs.internal_);
}

}  // namespace firebase::firestore
