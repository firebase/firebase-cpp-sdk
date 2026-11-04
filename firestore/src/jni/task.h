// Copyright 2021 Google LLC

#ifndef FIREBASE_FIRESTORE_SRC_JNI_TASK_H_
#define FIREBASE_FIRESTORE_SRC_JNI_TASK_H_

#include "firestore/src/jni/env.h"
#include "firestore/src/jni/jni_fwd.h"
#include "firestore/src/jni/loader.h"
#include "firestore/src/jni/object.h"

namespace firebase::firestore::jni {

class Task : public Object {
 public:
  using Object::Object;

  static void Initialize(Loader& loader) {
    loader.LoadClass(PROGUARD_KEEP_CLASS "com/google/android/gms/tasks/Task",
                     kGetResult_, kGetException_, kIsComplete_, kIsSuccessful_,
                     kIsCanceled_);
  }

  Local<Object> GetResult(Env& env) const { return env.Call(*this, kGetResult_); }
  Local<Throwable> GetException(Env& env) const {
    return env.Call(*this, kGetException_);
  }
  bool IsComplete(Env& env) const { return env.Call(*this, kIsComplete_); }
  bool IsSuccessful(Env& env) const { return env.Call(*this, kIsSuccessful_); }
  bool IsCanceled(Env& env) const { return env.Call(*this, kIsCanceled_); }

 private:
  static inline Method<Object> kGetResult_{
      "getResult", "()Ljava/lang/Object;"};
  static inline Method<Throwable> kGetException_{
      "getException", "()Ljava/lang/Exception;"};
  static inline Method<bool> kIsComplete_{"isComplete", "()Z"};
  static inline Method<bool> kIsSuccessful_{"isSuccessful", "()Z"};
  static inline Method<bool> kIsCanceled_{"isCanceled", "()Z"};
};

}  // namespace firebase::firestore::jni

#endif  // FIREBASE_FIRESTORE_SRC_JNI_TASK_H_
