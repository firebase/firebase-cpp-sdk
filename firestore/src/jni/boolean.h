// Copyright 2020 Google LLC

#ifndef FIREBASE_FIRESTORE_SRC_JNI_BOOLEAN_H_
#define FIREBASE_FIRESTORE_SRC_JNI_BOOLEAN_H_

#include "app/src/util_android.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/jni_fwd.h"
#include "firestore/src/jni/loader.h"
#include "firestore/src/jni/object.h"

namespace firebase::firestore::jni {

class Boolean : public Object {
 public:
  using Object::Object;

  static void Initialize(Loader& loader) {
    g_clazz_ = firebase::util::boolean_class::GetClass();
    loader.LoadFromExistingClass("java/lang/Boolean", g_clazz_, kValueOf_,
                                 kBooleanValue_);
  }

  static Class GetClass() { return Class(g_clazz_); }
  static Local<Boolean> Create(Env& env, bool value) {
    return env.Call(kValueOf_, value);
  }
  bool BooleanValue(Env& env) const { return env.Call(*this, kBooleanValue_); }

 private:
  static inline StaticMethod<Boolean> kValueOf_{
      "valueOf", "(Z)Ljava/lang/Boolean;"};
  static inline Method<bool> kBooleanValue_{"booleanValue", "()Z"};
  static inline jclass g_clazz_ = nullptr;
};

}  // namespace firebase::firestore::jni

#endif  // FIREBASE_FIRESTORE_SRC_JNI_BOOLEAN_H_
