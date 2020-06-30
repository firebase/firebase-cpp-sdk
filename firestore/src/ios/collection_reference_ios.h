#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_IOS_COLLECTION_REFERENCE_IOS_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_IOS_COLLECTION_REFERENCE_IOS_H_

#include <string>

#include "firestore/src/include/firebase/firestore/document_reference.h"
#include "firestore/src/ios/promise_factory_ios.h"
#include "firestore/src/ios/query_ios.h"
#include "Firestore/core/src/api/collection_reference.h"

namespace firebase {
namespace firestore {

class CollectionReferenceInternal : public QueryInternal {
 public:
  explicit CollectionReferenceInternal(api::CollectionReference&& collection);

  const std::string& id() const;
  std::string path() const;

  DocumentReference Parent() const;
  DocumentReference Document() const;
  DocumentReference Document(const std::string& document_path) const;

  Future<DocumentReference> Add(const MapFieldValue& data);

 private:
  const api::CollectionReference& collection_core_api() const;
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_IOS_COLLECTION_REFERENCE_IOS_H_
