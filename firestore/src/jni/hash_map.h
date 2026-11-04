// Copyright 2020 Google LLC

#ifndef FIREBASE_FIRESTORE_SRC_JNI_HASH_MAP_H_
#define FIREBASE_FIRESTORE_SRC_JNI_HASH_MAP_H_

#include "app/src/util_android.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/jni_fwd.h"
#include "firestore/src/jni/loader.h"
#include "firestore/src/jni/map.h"

namespace firebase::firestore::jni {

class HashMap : public Map {
 public:
  using Map::Map;

  static void Initialize(Loader& loader) {
    loader.LoadFromExistingClass("java/util/HashMap",
                                 firebase::util::hash_map::GetClass(),
                                 kConstructor_);
  }

  static Local<HashMap> Create(Env& env) { return env.New(kConstructor_); }

 private:
  static inline Constructor<HashMap> kConstructor_{"()V"};
};

}  // namespace firebase::firestore::jni

#endif  // FIREBASE_FIRESTORE_SRC_JNI_HASH_MAP_H_
