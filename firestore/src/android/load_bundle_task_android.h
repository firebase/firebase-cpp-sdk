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

#ifndef FIREBASE_FIRESTORE_SRC_ANDROID_LOAD_BUNDLE_TASK_ANDROID_H_
#define FIREBASE_FIRESTORE_SRC_ANDROID_LOAD_BUNDLE_TASK_ANDROID_H_

#include "firestore/src/android/wrapper.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/loader.h"
#include "firestore/src/jni/task.h"

namespace firebase {
namespace firestore {

/** A C++ proxy for a Java `LoadBundleTask`, which is a subclass of `Task`. */
class LoadBundleTaskInternal : public jni::Task {
 public:
  using Task::Task;

  static void Initialize(jni::Loader& loader);

  void AddProgressListener(jni::Env& env,
                           const jni::Object& executor,
                           const jni::Object& listener);
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_SRC_ANDROID_LOAD_BUNDLE_TASK_ANDROID_H_
