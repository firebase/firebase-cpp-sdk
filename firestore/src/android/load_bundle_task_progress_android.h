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

#ifndef FIREBASE_FIRESTORE_SRC_ANDROID_LOAD_BUNDLE_TASK_PROGRESS_ANDROID_H_
#define FIREBASE_FIRESTORE_SRC_ANDROID_LOAD_BUNDLE_TASK_PROGRESS_ANDROID_H_

#include <stdint.h>

#include "firestore/src/android/firestore_android.h"
#include "firestore/src/android/wrapper.h"
#include "firestore/src/include/firebase/firestore/load_bundle_task_progress.h"

namespace firebase {
namespace firestore {

class Firestore;

/** A C++ proxy for a Java `LoadBundleTaskProgress`. */
class LoadBundleTaskProgressInternal : public Wrapper {
 public:
  using Wrapper::Wrapper;

  static void Initialize(jni::Loader& loader);

  static jni::Class GetClass();

  LoadBundleTaskProgressInternal(FirestoreInternal* firestore,
                                 const jni::Object& object)
      : Wrapper(firestore, object) {}

  int32_t documents_loaded() const;

  int32_t total_documents() const;

  int64_t bytes_loaded() const;

  int64_t total_bytes() const;

  LoadBundleTaskProgress::State state() const;
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_SRC_ANDROID_LOAD_BUNDLE_TASK_PROGRESS_ANDROID_H_
