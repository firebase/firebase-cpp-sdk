/*
 * Copyright 2020 Google LLC
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

#ifndef FIREBASE_FIRESTORE_SRC_JNI_ARRAY_H_
#define FIREBASE_FIRESTORE_SRC_JNI_ARRAY_H_

#include "firestore/src/jni/env.h"
#include "firestore/src/jni/jni_fwd.h"
#include "firestore/src/jni/object.h"
#include "firestore/src/jni/traits.h"

namespace firebase {
namespace firestore {
namespace jni {

template <typename T>
class Array : public Object {
 public:
  using jni_type = JniType<Array<T>>;

  Array() = default;
  explicit Array(jni_type array) : Object(array) {}

  jni_type get() const override { return static_cast<jni_type>(Object::get()); }

  size_t Size(Env& env) const { return env.GetArrayLength(*this); }

  Local<T> Get(Env& env, size_t i) const {
    return env.GetArrayElement<T>(*this, i);
  }

  void Set(Env& env, size_t i, const T& value) {
    env.SetArrayElement<T>(*this, i, value);
  }
};

}  // namespace jni
}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_SRC_JNI_ARRAY_H_
