#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_IOS_DOCUMENT_CHANGE_IOS_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_IOS_DOCUMENT_CHANGE_IOS_H_

#include <cstdint>

#include "firestore/src/include/firebase/firestore/document_change.h"
#include "firestore/src/ios/firestore_ios.h"
#include "Firestore/core/src/api/document_change.h"

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

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_IOS_DOCUMENT_CHANGE_IOS_H_
