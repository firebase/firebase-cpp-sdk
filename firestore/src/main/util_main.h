// Copyright 2021 Google LLC

#ifndef FIREBASE_FIRESTORE_SRC_MAIN_UTIL_MAIN_H_
#define FIREBASE_FIRESTORE_SRC_MAIN_UTIL_MAIN_H_

#include "Firestore/core/src/api/firestore.h"
#include "firestore/src/include/firebase/firestore.h"
#include "firestore/src/main/firestore_main.h"

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

#endif  // FIREBASE_FIRESTORE_SRC_MAIN_UTIL_MAIN_H_
