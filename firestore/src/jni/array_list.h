// Copyright 2020 Google LLC

#ifndef FIREBASE_FIRESTORE_SRC_JNI_ARRAY_LIST_H_
#define FIREBASE_FIRESTORE_SRC_JNI_ARRAY_LIST_H_

#include <limits>

#include "app/src/assert.h"
#include "app/src/util_android.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/list.h"
#include "firestore/src/jni/loader.h"

namespace firebase::firestore::jni {

class ArrayList : public List {
 public:
  using List::List;

  static void Initialize(Loader& loader) {
    loader.LoadFromExistingClass("java/util/ArrayList",
                                 firebase::util::array_list::GetClass(),
                                 kConstructor_, kConstructorWithCapacity_);
  }

  static Local<ArrayList> Create(Env& env) { return env.New(kConstructor_); }

  static Local<ArrayList> Create(Env& env, size_t initial_capacity) {
    FIREBASE_ASSERT(initial_capacity <=
                    static_cast<size_t>(std::numeric_limits<jint>::max()));
    return env.New(kConstructorWithCapacity_,
                   static_cast<jint>(initial_capacity));
  }

 private:
  static inline Constructor<ArrayList> kConstructor_{"()V"};
  static inline Constructor<ArrayList> kConstructorWithCapacity_{"(I)V"};
};

}  // namespace firebase::firestore::jni

#endif  // FIREBASE_FIRESTORE_SRC_JNI_ARRAY_LIST_H_
