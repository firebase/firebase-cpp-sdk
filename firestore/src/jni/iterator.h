// Copyright 2020 Google LLC

#ifndef FIREBASE_FIRESTORE_SRC_JNI_ITERATOR_H_
#define FIREBASE_FIRESTORE_SRC_JNI_ITERATOR_H_

#include "app/src/util_android.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/jni_fwd.h"
#include "firestore/src/jni/loader.h"
#include "firestore/src/jni/object.h"

namespace firebase::firestore::jni {

class Iterator : public Object {
 public:
  using Object::Object;

  static void Initialize(Loader& loader) {
    loader.LoadFromExistingClass("java/util/Iterator",
                                 firebase::util::iterator::GetClass(),
                                 kHasNext_, kNext_);
  }

  bool HasNext(Env& env) const { return env.Call(*this, kHasNext_); }
  Local<Object> Next(Env& env) { return env.Call(*this, kNext_); }

 private:
  static inline Method<bool> kHasNext_{"hasNext", "()Z"};
  static inline Method<Object> kNext_{"next", "()Ljava/lang/Object;"};
};

}  // namespace firebase::firestore::jni

#endif  // FIREBASE_FIRESTORE_SRC_JNI_ITERATOR_H_
