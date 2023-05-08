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

#include <cstdint>

#include <jni.h>

#include "firestore/src/jni/env.h"
#include "firestore/src/jni/loader.h"
#include "firestore/src/jni/object.h"
#include "firestore/src/jni/ownership.h"

namespace firebase {
namespace firestore {
namespace jni {

class ArenaRef final {
 public:
  ArenaRef() = default;
  ArenaRef(Env&, jobject, AdoptExisting);

  ~ArenaRef();

  ArenaRef(const ArenaRef&);
  ArenaRef(ArenaRef&&) noexcept;
  ArenaRef& operator=(const ArenaRef&);
  ArenaRef& operator=(ArenaRef&&) = delete;

  static void Initialize(Loader&);

  bool is_valid() const {
    return valid_;
  }

  Local<Object> get(Env&) const;

 private:
  ArenaRef(JNIEnv*, jobject);

  bool valid_ = false;
  jlong id_ = -1;
};

}  // namespace jni
}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_SRC_JNI_ARENA_REF_H_
