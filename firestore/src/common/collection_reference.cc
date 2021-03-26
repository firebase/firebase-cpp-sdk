#include "firestore/src/include/firebase/firestore/collection_reference.h"

#include "app/meta/move.h"
#include "app/src/include/firebase/future.h"
#include "firestore/src/common/futures.h"
#include "firestore/src/common/util.h"
#include "firestore/src/include/firebase/firestore/document_reference.h"
#if defined(__ANDROID__)
#include "firestore/src/android/collection_reference_android.h"
#elif defined(FIRESTORE_STUB_BUILD)
#include "firestore/src/stub/collection_reference_stub.h"
#else
#include "firestore/src/ios/collection_reference_ios.h"
#endif  // defined(__ANDROID__)

namespace firebase {
namespace firestore {

// Design for the internal_:
//
// We wrap on one object instead of two. Instead of creating one for each of the
// CollectionReferenceInternal, which wraps around a Java CollectionReference
// object, and the QueryInternal, which wraps around a Java Query object, we
// create only the former. This requires CollectionReferenceInternal (resp.
// Java CollectionReference) be subclass of QueryInternal (resp. Java Query),
// which is already the case.

CollectionReference::CollectionReference() {}

CollectionReference::CollectionReference(const CollectionReference& reference)
    : Query(reference.internal()
                ? new CollectionReferenceInternal(*reference.internal())
                : nullptr) {}

CollectionReference::CollectionReference(CollectionReference&& reference)
    : Query(firebase::Move(reference)) {}

CollectionReference::CollectionReference(CollectionReferenceInternal* internal)
    : Query(internal) {}

CollectionReference& CollectionReference::operator=(
    const CollectionReference& reference) {
  Query::operator=(reference);
  return *this;
}

CollectionReference& CollectionReference::operator=(
    CollectionReference&& reference) {
  Query::operator=(firebase::Move(reference));
  return *this;
}

const std::string& CollectionReference::id() const {
  if (!internal()) return EmptyString();
  return internal()->id();
}

std::string CollectionReference::path() const {
  if (!internal()) return "";
  return internal()->path();
}

DocumentReference CollectionReference::Parent() const {
  if (!internal()) return {};
  return internal()->Parent();
}

DocumentReference CollectionReference::Document() const {
  if (!internal()) return {};
  return internal()->Document();
}

DocumentReference CollectionReference::Document(
    const char* document_path) const {
  if (!internal()) return {};
  return internal()->Document(document_path);
}

DocumentReference CollectionReference::Document(
    const std::string& document_path) const {
  if (!internal()) return {};
  return internal()->Document(document_path);
}

Future<DocumentReference> CollectionReference::Add(const MapFieldValue& data) {
  if (!internal()) return FailedFuture<DocumentReference>();
  return internal()->Add(data);
}

CollectionReferenceInternal* CollectionReference::internal() const {
  return static_cast<CollectionReferenceInternal*>(internal_);
}

}  // namespace firestore
}  // namespace firebase
