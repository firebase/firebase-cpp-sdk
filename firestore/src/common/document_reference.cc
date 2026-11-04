// Copyright 2020 Google LLC
#include "firestore/src/include/firebase/firestore/document_reference.h"

#include <ostream>
#include <utility>
#include "app/src/include/firebase/future.h"
#include "firestore/src/common/cleanup.h"
#include "firestore/src/common/event_listener.h"
#include "firestore/src/common/exception_common.h"
#include "firestore/src/common/futures.h"
#include "firestore/src/common/hard_assert_common.h"
#include "firestore/src/common/util.h"
#include "firestore/src/include/firebase/firestore/collection_reference.h"
#include "firestore/src/include/firebase/firestore/document_snapshot.h"
#include "firestore/src/include/firebase/firestore/listener_registration.h"
#if defined(__ANDROID__)
#include "firestore/src/android/document_reference_android.h"
#else
#include "firestore/src/main/document_reference_main.h"
#endif

namespace firebase::firestore {

using CleanupFnDocumentReference = CleanupFn<DocumentReference>;

DocumentReference::DocumentReference() {}

DocumentReference::DocumentReference(const DocumentReference& reference)
    : internal_(reference.internal_ ? new DocumentReferenceInternal(*reference.internal_)
                                    : nullptr) {
  CleanupFnDocumentReference::Register(this, internal_);
}

DocumentReference::DocumentReference(DocumentReference&& reference) {
  CleanupFnDocumentReference::Unregister(&reference, reference.internal_);
  std::swap(internal_, reference.internal_);
  CleanupFnDocumentReference::Register(this, internal_);
}

DocumentReference::DocumentReference(DocumentReferenceInternal* internal) : internal_(internal) {
  SIMPLE_HARD_ASSERT(internal != nullptr);
  CleanupFnDocumentReference::Register(this, internal_);
}

DocumentReference::~DocumentReference() {
  CleanupFnDocumentReference::Unregister(this, internal_);
  delete std::exchange(internal_, nullptr);
}

DocumentReference& DocumentReference::operator=(const DocumentReference& reference) {
  if (this == &reference) return *this;
  CleanupFnDocumentReference::Unregister(this, internal_);
  delete std::exchange(internal_, reference.internal_
                                      ? new DocumentReferenceInternal(*reference.internal_)
                                      : nullptr);
  CleanupFnDocumentReference::Register(this, internal_);
  return *this;
}

DocumentReference& DocumentReference::operator=(DocumentReference&& reference) {
  if (this == &reference) return *this;
  CleanupFnDocumentReference::Unregister(&reference, reference.internal_);
  CleanupFnDocumentReference::Unregister(this, internal_);
  delete std::exchange(internal_, std::exchange(reference.internal_, nullptr));
  CleanupFnDocumentReference::Register(this, internal_);
  return *this;
}

const Firestore* DocumentReference::firestore() const {
  return internal_ ? internal_->firestore() : nullptr;
}
Firestore* DocumentReference::firestore() {
  return const_cast<Firestore*>(static_cast<const DocumentReference&>(*this).firestore());
}

const std::string& DocumentReference::id() const {
  return internal_ ? internal_->id() : EmptyString();
}
std::string DocumentReference::path() const { return internal_ ? internal_->path() : ""; }
CollectionReference DocumentReference::Parent() const {
  return internal_ ? internal_->Parent() : CollectionReference{};
}

CollectionReference DocumentReference::Collection(const char* collection_path) const {
  if (!collection_path) {
    SimpleThrowInvalidArgument("Collection path cannot be null.");
    return {};
  }
  return Collection(std::string(collection_path));
}

CollectionReference DocumentReference::Collection(const std::string& collection_path) const {
  if (collection_path.empty()) {
    SimpleThrowInvalidArgument("Collection path cannot be empty.");
    return {};
  }
  return internal_ ? internal_->Collection(collection_path) : CollectionReference{};
}

Future<DocumentSnapshot> DocumentReference::Get(Source source) const {
  return internal_ ? internal_->Get(source) : FailedFuture<DocumentSnapshot>();
}

Future<void> DocumentReference::Set(const MapFieldValue& data, const SetOptions& options) {
  return internal_ ? internal_->Set(data, options) : FailedFuture<void>();
}

Future<void> DocumentReference::Update(const MapFieldValue& data) {
  return internal_ ? internal_->Update(data) : FailedFuture<void>();
}

Future<void> DocumentReference::Update(const MapFieldPathValue& data) {
  if (!internal_) return FailedFuture<void>();
  for (const auto& kv : data) {
    if (!kv.first.is_valid()) {
      SimpleThrowInvalidArgument("Field path cannot be invalid.");
      return FailedFuture<void>();
    }
  }
  return internal_->Update(data);
}

Future<void> DocumentReference::Delete() {
  return internal_ ? internal_->Delete() : FailedFuture<void>();
}

ListenerRegistration DocumentReference::AddSnapshotListener(
    std::function<void(const DocumentSnapshot&, Error, const std::string&)> callback) {
  return AddSnapshotListener(MetadataChanges::kExclude, std::move(callback));
}

ListenerRegistration DocumentReference::AddSnapshotListener(
    MetadataChanges metadata_changes,
    std::function<void(const DocumentSnapshot&, Error, const std::string&)> callback) {
  if (!callback) {
    SimpleThrowInvalidArgument(
        "Snapshot listener callback parameter cannot be an empty function.");
    return {};
  }
  return internal_ ? internal_->AddSnapshotListener(metadata_changes, std::move(callback))
                   : ListenerRegistration{};
}

std::string DocumentReference::ToString() const {
  return is_valid() ? std::string("DocumentReference(") + path() + ')'
                    : "DocumentReference(invalid)";
}

std::ostream& operator<<(std::ostream& out, const DocumentReference& reference) {
  return out << reference.ToString();
}

bool operator==(const DocumentReference& lhs, const DocumentReference& rhs) {
  if (!lhs.internal_ || !rhs.internal_) return lhs.internal_ == rhs.internal_;
  return lhs.internal_ == rhs.internal_ ||
         (lhs.firestore() == rhs.firestore() && lhs.path() == rhs.path());
}

}  // namespace firebase::firestore
