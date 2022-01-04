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

#ifndef FIREBASE_FIRESTORE_INTEGRATION_TEST_INTERNAL_SRC_ANDROID_TASK_COMPLETION_SOURCE_H_
#define FIREBASE_FIRESTORE_INTEGRATION_TEST_INTERNAL_SRC_ANDROID_TASK_COMPLETION_SOURCE_H_

#include <string>

#include "firestore/src/jni/jni_fwd.h"
#include "firestore/src/jni/object.h"
#include "firestore/src/jni/task.h"
#include "firestore/src/jni/throwable.h"

namespace firebase {
namespace firestore {

/** A C++ proxy for a Java `TaskCompletionSource` from the Tasks API. */
class TaskCompletionSource : public jni::Object {
 public:
  using jni::Object::Object;

  static void Initialize(jni::Loader& loader);

  /** Creates a C++ proxy for a Java `TaskCompletionSource` object. */
  static jni::Local<TaskCompletionSource> Create(jni::Env& env);

  /** Creates a C++ proxy for a Java `TaskCompletionSource` object. */
  static jni::Local<TaskCompletionSource> Create(
      jni::Env& env, const Object& cancellation_token);

  /**
   * Invokes `getTask()` on the wrapped Java `TaskCompletionSource` object.
   */
  jni::Local<jni::Task> GetTask(jni::Env& env);

  /**
   * Invokes `setException()` on the wrapped Java `TaskCompletionSource` object.
   */
  void SetException(jni::Env& env, const jni::Throwable& result);

  /**
   * Invokes `setResult()` on the wrapped Java `TaskCompletionSource` object.
   */
  void SetResult(jni::Env& env, const Object& result);
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_INTEGRATION_TEST_INTERNAL_SRC_ANDROID_TASK_COMPLETION_SOURCE_H_
