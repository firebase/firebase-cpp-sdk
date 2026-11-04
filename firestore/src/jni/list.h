// Copyright 2020 Google LLC

#ifndef FIREBASE_FIRESTORE_SRC_JNI_LIST_H_
#define FIREBASE_FIRESTORE_SRC_JNI_LIST_H_

#include <limits>

#include "app/src/assert.h"
#include "firestore/src/jni/collection.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/jni_fwd.h"
#include "firestore/src/jni/loader.h"

namespace firebase::firestore::jni {

class List : public Collection {
 public:
  using Collection::Collection;

  static void Initialize(Loader& loader) {
    g_clazz_ = loader.LoadClass("java/util/List", kGet_, kSet_);
  }

  static Class GetClass() { return Class(g_clazz_); }

  Local<Object> Get(Env& env, size_t i) const {
    FIREBASE_ASSERT(i <= static_cast<size_t>(std::numeric_limits<jint>::max()));
    return env.Call(*this, kGet_, static_cast<jint>(i));
  }

  Local<Object> Set(Env& env, size_t i, const Object& object) {
    FIREBASE_ASSERT(i <= static_cast<size_t>(std::numeric_limits<jint>::max()));
    return env.Call(*this, kSet_, static_cast<jint>(i), object);
  }

 private:
  static inline Method<Object> kGet_{"get", "(I)Ljava/lang/Object;"};
  static inline Method<Object> kSet_{
      "set", "(ILjava/lang/Object;)Ljava/lang/Object;"};
  static inline jclass g_clazz_ = nullptr;
};

}  // namespace firebase::firestore::jni

#endif  // FIREBASE_FIRESTORE_SRC_JNI_LIST_H_
