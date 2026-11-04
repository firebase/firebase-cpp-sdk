// Copyright 2020 Google LLC

#ifndef FIREBASE_FIRESTORE_SRC_JNI_SET_H_
#define FIREBASE_FIRESTORE_SRC_JNI_SET_H_

#include "firestore/src/jni/collection.h"
#include "firestore/src/jni/jni_fwd.h"

namespace firebase::firestore::jni {

class Set : public Collection {
 public:
  using Collection::Collection;
};

}  // namespace firebase::firestore::jni

#endif  // FIREBASE_FIRESTORE_SRC_JNI_SET_H_
