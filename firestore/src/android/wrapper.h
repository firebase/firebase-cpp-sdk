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

#ifndef FIREBASE_FIRESTORE_SRC_ANDROID_WRAPPER_H_
#define FIREBASE_FIRESTORE_SRC_ANDROID_WRAPPER_H_

#include "firestore/src/jni/arena_ref.h"
#include "firestore/src/jni/jni_fwd.h"
#include "firestore/src/jni/object.h"
#include "firestore/src/jni/ownership.h"

namespace firebase {
namespace firestore {

class FirestoreInternal;

// This is the generalized wrapper base class which contains a FirestoreInternal
// client instance as well as a jobject, around which this is a wrapper.
class Wrapper {
 public:
  Wrapper(FirestoreInternal* firestore, const jni::Object& obj);

  Wrapper(const Wrapper& wrapper);
  Wrapper(Wrapper&& wrapper) noexcept;

  virtual ~Wrapper();

  // Assignment operators are not needed and are deleted.
  Wrapper& operator=(const Wrapper& wrapper) = delete;
  Wrapper& operator=(Wrapper&& wrapper) = delete;

  FirestoreInternal* firestore_internal() const { return firestore_; }
  jni::Local<jni::Object> ToJava() const;

 protected:
  jni::Env GetEnv() const;

  FirestoreInternal* firestore_ = nullptr;  // not owning
  jni::ArenaRef obj_;
};

}  // namespace firestore
}  // namespace firebase
#endif  // FIREBASE_FIRESTORE_SRC_ANDROID_WRAPPER_H_
