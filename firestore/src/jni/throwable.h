// Copyright 2020 Google LLC

#ifndef FIREBASE_FIRESTORE_SRC_JNI_THROWABLE_H_
#define FIREBASE_FIRESTORE_SRC_JNI_THROWABLE_H_

#include <string>

#include "firestore/src/jni/object.h"

namespace firebase::firestore::jni {

class Throwable : public Object {
 public:
  Throwable() = default;
  explicit Throwable(jthrowable throwable) : Object(throwable) {}

  jthrowable get() const override { return static_cast<jthrowable>(object_); }
  std::string GetMessage(Env& env) const;
};

}  // namespace firebase::firestore::jni

#endif  // FIREBASE_FIRESTORE_SRC_JNI_THROWABLE_H_
