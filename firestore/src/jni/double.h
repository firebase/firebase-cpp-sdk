// Copyright 2020 Google LLC

#ifndef FIREBASE_FIRESTORE_SRC_JNI_DOUBLE_H_
#define FIREBASE_FIRESTORE_SRC_JNI_DOUBLE_H_

#include "app/src/util_android.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/jni_fwd.h"
#include "firestore/src/jni/loader.h"
#include "firestore/src/jni/object.h"

namespace firebase::firestore::jni {

class Double : public Object {
 public:
  using Object::Object;

  static void Initialize(Loader& loader) {
    g_clazz_ = firebase::util::double_class::GetClass();
    loader.LoadFromExistingClass("java/lang/Double", g_clazz_, kValueOf_,
                                 kDoubleValue_);
  }

  static Class GetClass() { return Class(g_clazz_); }
  static Local<Double> Create(Env& env, double value) {
    return env.Call(kValueOf_, value);
  }
  double DoubleValue(Env& env) const { return env.Call(*this, kDoubleValue_); }

 private:
  static inline StaticMethod<Double> kValueOf_{
      "valueOf", "(D)Ljava/lang/Double;"};
  static inline Method<double> kDoubleValue_{"doubleValue", "()D"};
  static inline jclass g_clazz_ = nullptr;
};

}  // namespace firebase::firestore::jni

#endif  // FIREBASE_FIRESTORE_SRC_JNI_DOUBLE_H_
