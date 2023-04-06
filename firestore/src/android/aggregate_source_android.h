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

#ifndef FIREBASE_FIRESTORE_SRC_ANDROID_AGGREGATE_SOURCE_ANDROID_H_
#define FIREBASE_FIRESTORE_SRC_ANDROID_AGGREGATE_SOURCE_ANDROID_H_

#include "firestore/src/include/firebase/firestore/aggregate_source.h"
#include "firestore/src/jni/jni_fwd.h"

namespace firebase {
namespace firestore {

class AggregateSourceInternal {
 public:
  static void Initialize(jni::Loader& loader);

  static jni::Local<jni::Object> Create(jni::Env& env, AggregateSource source);
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_SRC_ANDROID_AGGREGATE_SOURCE_ANDROID_H_
