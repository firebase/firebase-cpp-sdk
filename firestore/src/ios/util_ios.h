#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_IOS_UTIL_IOS_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_IOS_UTIL_IOS_H_

#include "firestore/src/include/firebase/firestore.h"
#include "firestore/src/ios/firestore_ios.h"
#include "Firestore/core/src/api/firestore.h"

namespace firebase {
namespace firestore {

template <typename T>
FirestoreInternal* GetFirestoreInternal(T* object) {
  void* raw_ptr = object->firestore()->extension();
  return static_cast<FirestoreInternal*>(raw_ptr);
}

template <typename T>
Firestore* GetFirestore(T* object) {
  return GetFirestoreInternal(object)->firestore_public();
}

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_IOS_UTIL_IOS_H_
