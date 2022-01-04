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

#ifndef FIREBASE_FIRESTORE_SRC_ANDROID_FIELD_PATH_ANDROID_H_
#define FIREBASE_FIRESTORE_SRC_ANDROID_FIELD_PATH_ANDROID_H_

#include "firestore/src/include/firebase/firestore/field_path.h"
#include "firestore/src/jni/jni_fwd.h"

namespace firebase {
namespace firestore {

// This helper class provides conversion between the C++ type and Java type. In
// addition, we also need proper initializer and terminator for the Java class
// cache/uncache.
class FieldPathConverter {
 public:
  static void Initialize(jni::Loader& loader);

  /** Creates a Java FieldPath from  a C++ FieldPath. */
  static jni::Local<jni::Object> Create(jni::Env& env, const FieldPath& path);

  // We do not need to convert Java FieldPath back to C++ FieldPath since there
  // is no public API that returns a FieldPath yet.
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_SRC_ANDROID_FIELD_PATH_ANDROID_H_
