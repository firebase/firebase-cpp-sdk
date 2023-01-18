/*
 * Copyright 2023 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef FIREBASE_FIRESTORE_SRC_JNI_ARENA_REF_H_
#define FIREBASE_FIRESTORE_SRC_JNI_ARENA_REF_H_

#include <jni.h>

#include <cstdlib>

#include "firestore/src/jni/jni_fwd.h"
#include "firestore/src/jni/object.h"
#include "firestore/src/jni/ownership.h"

namespace firebase {
namespace firestore {
namespace jni {

/**
 * ArenaRef is an RAII wrapper which serves the same purpose as Global<T>, both
 * of them manage the lifetime of JNI reference. Compared to Global<T>, the
 * advantage of ArenaRef is it gets rid of the restriction of total number of
 * JNI global reference (the pointer inside Global<T>) need to be under 51,200,
 * since after that number the app will crash. The drawback is ArenaRef holds
 * JNI Reference indirectly using HashMap so there is a efficiency lost against
 * Global<T>.
 */
class ArenaRef {
 public:
  ArenaRef() = default;
  ArenaRef(Env&, const Object&);
  ArenaRef(const ArenaRef& other);
  ArenaRef(ArenaRef&& other);
  ArenaRef& operator=(const ArenaRef& other);
  ArenaRef& operator=(ArenaRef&& other);
  ~ArenaRef();

  static void Initialize(Env&);

  Local<Object> get(Env&) const;

 private:
  Local<Long> key_object(Env&) const;

  int64_t key_ = -1;
};

}  // namespace jni
}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_SRC_JNI_ARENA_REF_H_
