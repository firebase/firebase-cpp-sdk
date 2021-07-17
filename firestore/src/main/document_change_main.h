// Copyright 2021 Google LLC

#ifndef FIREBASE_FIRESTORE_SRC_MAIN_DOCUMENT_CHANGE_MAIN_H_
#define FIREBASE_FIRESTORE_SRC_MAIN_DOCUMENT_CHANGE_MAIN_H_

#include <cstdint>

#include "Firestore/core/src/api/document_change.h"
#include "firestore/src/include/firebase/firestore/document_change.h"
#include "firestore/src/main/firestore_main.h"

namespace firebase {
namespace firestore {

class DocumentChangeInternal {
 public:
  explicit DocumentChangeInternal(api::DocumentChange&& change);

  FirestoreInternal* firestore_internal();

  DocumentChange::Type type() const;
  DocumentSnapshot document() const;
  std::size_t old_index() const;
  std::size_t new_index() const;

 private:
  api::DocumentChange change_;
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_SRC_MAIN_DOCUMENT_CHANGE_MAIN_H_
