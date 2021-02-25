#include "firestore/src/include/firebase/firestore/document_reference.h"

#include <ostream>

#include "app/meta/move.h"
#include "app/src/assert.h"
#include "app/src/include/firebase/future.h"
#include "firestore/src/common/cleanup.h"
#include "firestore/src/common/futures.h"
#include "firestore/src/common/util.h"

#include "firestore/src/include/firebase/firestore/collection_reference.h"
#include "firestore/src/include/firebase/firestore/document_snapshot.h"
#include "firestore/src/include/firebase/firestore/event_listener.h"
#include "firestore/src/include/firebase/firestore/listener_registration.h"
#if defined(__ANDROID__)
#include "firestore/src/android/document_reference_android.h"
#elif defined(FIRESTORE_STUB_BUILD)
#include "firestore/src/stub/document_reference_stub.h"
#else
#include "firestore/src/ios/document_reference_ios.h"
#endif  // defined(__ANDROID__)

namespace firebase {
namespace firestore {

using CleanupFnDocumentReference = CleanupFn<DocumentReference>;

DocumentReference::DocumentReference() {}

DocumentReference::DocumentReference(const DocumentReference& reference) {
  if (reference.internal_) {
    internal_ = new DocumentReferenceInternal(*reference.internal_);
  }
  CleanupFnDocumentReference::Register(this, internal_);
}

DocumentReference::DocumentReference(DocumentReference&& reference) {
  CleanupFnDocumentReference::Unregister(&reference, reference.internal_);
  std::swap(internal_, reference.internal_);
  CleanupFnDocumentReference::Register(this, internal_);
}

DocumentReference::DocumentReference(DocumentReferenceInternal* internal)
    : internal_(internal) {
  FIREBASE_ASSERT(internal != nullptr);
  CleanupFnDocumentReference::Register(this, internal_);
}

DocumentReference::~DocumentReference() {
  CleanupFnDocumentReference::Unregister(this, internal_);
  delete internal_;
  internal_ = nullptr;
}

DocumentReference& DocumentReference::operator=(
    const DocumentReference& reference) {
  if (this == &reference) {
    return *this;
  }

  CleanupFnDocumentReference::Unregister(this, internal_);
  delete internal_;
  if (reference.internal_) {
    internal_ = new DocumentReferenceInternal(*reference.internal_);
  } else {
    internal_ = nullptr;
  }
  CleanupFnDocumentReference::Register(this, internal_);
  return *this;
}

DocumentReference& DocumentReference::operator=(DocumentReference&& reference) {
  if (this == &reference) {
    return *this;
  }

  CleanupFnDocumentReference::Unregister(&reference, reference.internal_);
  CleanupFnDocumentReference::Unregister(this, internal_);
  delete internal_;
  internal_ = reference.internal_;
  reference.internal_ = nullptr;
  CleanupFnDocumentReference::Register(this, internal_);
  return *this;
}

const Firestore* DocumentReference::firestore() const {
  if (!internal_) return {};

  const Firestore* firestore = internal_->firestore();
  FIREBASE_ASSERT(firestore);
  return firestore;
}

Firestore* DocumentReference::firestore() {
  return const_cast<Firestore*>(
      static_cast<const DocumentReference&>(*this).firestore());
}

const std::string& DocumentReference::id() const {
  if (!internal_) return EmptyString();
  return internal_->id();
}

std::string DocumentReference::path() const {
  if (!internal_) return "";
  return internal_->path();
}

CollectionReference DocumentReference::Parent() const {
  if (!internal_) return {};
  return internal_->Parent();
}

CollectionReference DocumentReference::Collection(
    const char* collection_path) const {
  if (!internal_) return {};
  return internal_->Collection(collection_path);
}

CollectionReference DocumentReference::Collection(
    const std::string& collection_path) const {
  if (!internal_) return {};
  return internal_->Collection(collection_path);
}

Future<DocumentSnapshot> DocumentReference::Get(Source source) const {
  if (!internal_) return FailedFuture<DocumentSnapshot>();
  return internal_->Get(source);
}

Future<void> DocumentReference::Set(const MapFieldValue& data,
                                    const SetOptions& options) {
  if (!internal_) return FailedFuture<void>();
  return internal_->Set(data, options);
}

Future<void> DocumentReference::Update(const MapFieldValue& data) {
  if (!internal_) return FailedFuture<void>();
  return internal_->Update(data);
}

Future<void> DocumentReference::Update(const MapFieldPathValue& data) {
  if (!internal_) return FailedFuture<void>();
  return internal_->Update(data);
}

Future<void> DocumentReference::Delete() {
  if (!internal_) return FailedFuture<void>();
  return internal_->Delete();
}

#if defined(FIREBASE_USE_STD_FUNCTION) || defined(DOXYGEN)
ListenerRegistration DocumentReference::AddSnapshotListener(
    std::function<void(const DocumentSnapshot&, Error, const std::string&)>
        callback) {
  return AddSnapshotListener(MetadataChanges::kExclude,
                             firebase::Move(callback));
}

ListenerRegistration DocumentReference::AddSnapshotListener(
    MetadataChanges metadata_changes,
    std::function<void(const DocumentSnapshot&, Error, const std::string&)>
        callback) {
  FIREBASE_ASSERT_MESSAGE(callback, "invalid callback parameter is passed in.");
  if (!internal_) return {};
  return internal_->AddSnapshotListener(metadata_changes,
                                        firebase::Move(callback));
}
#endif  // defined(FIREBASE_USE_STD_FUNCTION) || defined(DOXYGEN)

#if !defined(FIREBASE_USE_STD_FUNCTION) || defined(DOXYGEN)
ListenerRegistration DocumentReference::AddSnapshotListener(
    EventListener<DocumentSnapshot>* listener) {
  return AddSnapshotListener(MetadataChanges::kExclude, listener);
}

ListenerRegistration DocumentReference::AddSnapshotListener(
    MetadataChanges metadata_changes,
    EventListener<DocumentSnapshot>* listener) {
  if (!internal_) return {};
  return internal_->AddSnapshotListener(metadata_changes, listener);
}
#endif  // !defined(FIREBASE_USE_STD_FUNCTION) || defined(DOXYGEN)

std::string DocumentReference::ToString() const {
  if (!is_valid()) return "DocumentReference(invalid)";

  return std::string("DocumentReference(") + path() + ')';
}

std::ostream& operator<<(std::ostream& out,
                         const DocumentReference& reference) {
  return out << reference.ToString();
}

bool operator==(const DocumentReference& lhs, const DocumentReference& rhs) {
  return lhs.internal_ == rhs.internal_ ||
         (lhs.firestore() == rhs.firestore() && lhs.path() == rhs.path());
}

}  // namespace firestore
}  // namespace firebase
