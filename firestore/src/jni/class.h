// Copyright 2020 Google LLC

#ifndef FIREBASE_FIRESTORE_SRC_JNI_CLASS_H_
#define FIREBASE_FIRESTORE_SRC_JNI_CLASS_H_

#include <string>

#include "firestore/src/jni/jni_fwd.h"
#include "firestore/src/jni/object.h"

namespace firebase::firestore::jni {

class Class : public Object {
 public:
  Class() = default;
  explicit Class(jclass clazz) : Object(clazz) {}

  jclass get() const override { return static_cast<jclass>(object_); }

  std::string GetName(Env& env) const;
  static std::string GetClassName(Env& env, const Object& object);
  bool IsArray(Env& env) const;

 private:
  friend class Loader;
  static void Initialize(Loader& loader);
};

}  // namespace firebase::firestore::jni

#endif  // FIREBASE_FIRESTORE_SRC_JNI_CLASS_H_
