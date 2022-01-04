/*
 * Copyright 2021 Google LLC
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

#ifndef FIREBASE_FIRESTORE_SRC_JNI_TASK_H_
#define FIREBASE_FIRESTORE_SRC_JNI_TASK_H_

#include "firestore/src/jni/jni_fwd.h"
#include "firestore/src/jni/object.h"

namespace firebase {
namespace firestore {
namespace jni {

/**
 * A C++ proxy for a Java `Task`, which represents the status and result of an
 * asynchronous operation.
 */
class Task : public Object {
 public:
  using Object::Object;

  static void Initialize(Loader& loader);

  Local<Object> GetResult(Env& env) const;
  Local<Throwable> GetException(Env& env) const;
  bool IsComplete(Env& env) const;
  bool IsSuccessful(Env& env) const;
  bool IsCanceled(Env& env) const;
};

}  // namespace jni
}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_SRC_JNI_TASK_H_
