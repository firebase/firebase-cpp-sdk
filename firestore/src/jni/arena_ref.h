// Copyright 2023 Google LLC

#ifndef FIREBASE_FIRESTORE_SRC_JNI_ARENA_REF_H_
#define FIREBASE_FIRESTORE_SRC_JNI_ARENA_REF_H_

#include <jni.h>

#include <memory>

#include "firestore/src/jni/jni_fwd.h"

namespace firebase::firestore::jni {

class ArenaRef final {
 public:
  ArenaRef() = default;
  ~ArenaRef() = default;
  ArenaRef(Env&, jobject);

  ArenaRef(const ArenaRef&) = default;
  ArenaRef(ArenaRef&&) = default;
  ArenaRef& operator=(const ArenaRef&) = default;
  ArenaRef& operator=(ArenaRef&&) = default;

  Local<Object> get(Env&) const;
  void reset(Env& env, const Object&);
  static void Initialize(Loader&);

 private:
  class ObjectArenaEntry;
  void reset(Env& env, jobject);
  std::shared_ptr<ObjectArenaEntry> entry_;
};

}  // namespace firebase::firestore::jni

#endif  // FIREBASE_FIRESTORE_SRC_JNI_ARENA_REF_H_
