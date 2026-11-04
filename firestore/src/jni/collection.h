// Copyright 2020 Google LLC

#ifndef FIREBASE_FIRESTORE_SRC_JNI_COLLECTION_H_
#define FIREBASE_FIRESTORE_SRC_JNI_COLLECTION_H_

#include <cstddef>

#include "firestore/src/jni/env.h"
#include "firestore/src/jni/iterator.h"
#include "firestore/src/jni/jni_fwd.h"
#include "firestore/src/jni/loader.h"
#include "firestore/src/jni/object.h"

namespace firebase::firestore::jni {

class Collection : public Object {
 public:
  using Object::Object;

  static void Initialize(Loader& loader) {
    loader.LoadClass("java/util/Collection", kAdd_, kIterator_, kSize_);
  }

  bool Add(Env& env, const Object& object) {
    return env.Call(*this, kAdd_, object);
  }
  Local<class Iterator> Iterator(Env& env) const {
    return env.Call(*this, kIterator_);
  }
  size_t Size(Env& env) const { return env.Call(*this, kSize_); }

 private:
  static inline Method<bool> kAdd_{"add", "(Ljava/lang/Object;)Z"};
  static inline Method<class Iterator> kIterator_{
      "iterator", "()Ljava/util/Iterator;"};
  static inline Method<size_t> kSize_{"size", "()I"};
};

}  // namespace firebase::firestore::jni

#endif  // FIREBASE_FIRESTORE_SRC_JNI_COLLECTION_H_
