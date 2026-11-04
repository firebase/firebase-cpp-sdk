// Copyright 2020 Google LLC

#ifndef FIREBASE_FIRESTORE_SRC_JNI_MAP_H_
#define FIREBASE_FIRESTORE_SRC_JNI_MAP_H_

#include "app/src/util_android.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/jni_fwd.h"
#include "firestore/src/jni/loader.h"
#include "firestore/src/jni/object.h"
#include "firestore/src/jni/set.h"

namespace firebase::firestore::jni {

class Map : public Object {
 public:
  using Object::Object;

  static void Initialize(Loader& loader) {
    g_clazz_ = firebase::util::map::GetClass();
    loader.LoadFromExistingClass("java/util/Map", g_clazz_, kSize_, kGet_,
                                 kPut_, kRemove_, kKeySet_);
  }

  static Class GetClass() { return Class(g_clazz_); }
  size_t Size(Env& env) const { return env.Call(*this, kSize_); }
  Local<Object> Get(Env& env, const Object& key) const {
    return env.Call(*this, kGet_, key);
  }
  Local<Object> Put(Env& env, const Object& key, const Object& value) {
    return env.Call(*this, kPut_, key, value);
  }
  Local<Object> Remove(Env& env, const Object& key) {
    return env.Call(*this, kRemove_, key);
  }
  Local<Set> KeySet(Env& env) const { return env.Call(*this, kKeySet_); }

 private:
  static inline Method<size_t> kSize_{"size", "()I"};
  static inline Method<Object> kGet_{
      "get", "(Ljava/lang/Object;)Ljava/lang/Object;"};
  static inline Method<Object> kPut_{
      "put", "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;"};
  static inline Method<Object> kRemove_{
      "remove", "(Ljava/lang/Object;)Ljava/lang/Object;"};
  static inline Method<Set> kKeySet_{"keySet", "()Ljava/util/Set;"};
  static inline jclass g_clazz_ = nullptr;
};

}  // namespace firebase::firestore::jni

#endif  // FIREBASE_FIRESTORE_SRC_JNI_MAP_H_
