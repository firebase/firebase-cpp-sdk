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

#ifndef FIREBASE_FIRESTORE_SRC_JNI_ARRAY_LIST_H_
#define FIREBASE_FIRESTORE_SRC_JNI_ARRAY_LIST_H_

#include "firestore/src/jni/list.h"

namespace firebase {
namespace firestore {
namespace jni {

/** A C++ proxy for a Java `ArrayList`. */
class ArrayList : public List {
 public:
  using List::List;

  static void Initialize(Loader& loader);

  static Local<ArrayList> Create(Env& env);
  static Local<ArrayList> Create(Env& env, size_t size);
};

}  // namespace jni
}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_SRC_JNI_ARRAY_LIST_H_
