// Copyright 2021 Google LLC

#ifndef FIREBASE_FIRESTORE_SRC_JNI_TASK_H_
#define FIREBASE_FIRESTORE_SRC_JNI_TASK_H_

#include "firestore/src/jni/jni_fwd.h"
#include "firestore/src/jni/object.h"

namespace firebase {
namespace firestore {
namespace jni {

/**
 * A C++ proxy for a Java `Task`, which represents the status and result of an
 * asynchronous operation.
 */
class Task : public Object {
 public:
  using Object::Object;

  static void Initialize(Loader& loader);

  Local<Object> GetResult(Env& env) const;
  Local<Throwable> GetException(Env& env) const;
  bool IsComplete(Env& env) const;
  bool IsSuccessful(Env& env) const;
  bool IsCanceled(Env& env) const;
};

}  // namespace jni
}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_SRC_JNI_TASK_H_
