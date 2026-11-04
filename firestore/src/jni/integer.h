// Copyright 2020 Google LLC

#ifndef FIREBASE_FIRESTORE_SRC_JNI_INTEGER_H_
#define FIREBASE_FIRESTORE_SRC_JNI_INTEGER_H_

#include <cstdint>

#include "app/src/util_android.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/jni_fwd.h"
#include "firestore/src/jni/loader.h"
#include "firestore/src/jni/object.h"

namespace firebase::firestore::jni {

class Integer : public Object {
 public:
  using Object::Object;

  static void Initialize(Loader& loader) {
    loader.LoadFromExistingClass("java/lang/Integer",
                                 firebase::util::integer_class::GetClass(),
                                 kValueOf_);
  }

  static Local<Integer> Create(Env& env, int32_t value) {
    return env.Call(kValueOf_, value);
  }

 private:
  static inline StaticMethod<Integer> kValueOf_{
      "valueOf", "(I)Ljava/lang/Integer;"};
};

}  // namespace firebase::firestore::jni

#endif  // FIREBASE_FIRESTORE_SRC_JNI_INTEGER_H_
