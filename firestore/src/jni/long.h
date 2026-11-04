// Copyright 2020 Google LLC

#ifndef FIREBASE_FIRESTORE_SRC_JNI_LONG_H_
#define FIREBASE_FIRESTORE_SRC_JNI_LONG_H_

#include <cstdint>

#include "app/src/util_android.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/jni_fwd.h"
#include "firestore/src/jni/loader.h"
#include "firestore/src/jni/object.h"

namespace firebase::firestore::jni {

class Long : public Object {
 public:
  using Object::Object;

  static void Initialize(Loader& loader) {
    g_clazz_ = firebase::util::long_class::GetClass();
    loader.LoadFromExistingClass("java/lang/Long", g_clazz_, kValueOf_,
                                 kLongValue_);
  }

  static Class GetClass() { return Class(g_clazz_); }
  static Local<Long> Create(Env& env, int64_t value) {
    return env.Call(kValueOf_, value);
  }
  int64_t LongValue(Env& env) const { return env.Call(*this, kLongValue_); }

 private:
  static inline StaticMethod<Long> kValueOf_{"valueOf", "(J)Ljava/lang/Long;"};
  static inline Method<int64_t> kLongValue_{"longValue", "()J"};
  static inline jclass g_clazz_ = nullptr;
};

}  // namespace firebase::firestore::jni

#endif  // FIREBASE_FIRESTORE_SRC_JNI_LONG_H_
