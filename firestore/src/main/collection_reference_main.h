// Copyright 2021 Google LLC

#ifndef FIREBASE_FIRESTORE_SRC_MAIN_COLLECTION_REFERENCE_MAIN_H_
#define FIREBASE_FIRESTORE_SRC_MAIN_COLLECTION_REFERENCE_MAIN_H_

#include <string>

#include "Firestore/core/src/api/collection_reference.h"
#include "firestore/src/include/firebase/firestore/document_reference.h"
#include "firestore/src/main/promise_factory_main.h"
#include "firestore/src/main/query_main.h"

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

#endif  // FIREBASE_FIRESTORE_SRC_MAIN_COLLECTION_REFERENCE_MAIN_H_
