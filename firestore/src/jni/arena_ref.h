/*
 * Copyright 2022 Google LLC
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

#include "firestore/src/jni/env.h"
#include "firestore/src/jni/object.h"
#include "firestore/src/jni/ownership.h"

namespace firebase {
namespace firestore {
namespace jni {

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

  int64_t key_ = 0;
};

}  // namespace jni
}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_SRC_JNI_ARENA_REF_H_
