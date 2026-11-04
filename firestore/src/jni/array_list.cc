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

#include "firestore/src/jni/array_list.h"

#include <limits>

#include "app/src/assert.h"
#include "app/src/util_android.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/loader.h"

namespace firebase {
namespace firestore {
namespace jni {
namespace {

Constructor<ArrayList> kConstructor("()V");
Constructor<ArrayList> kConstructorWithCapacity("(I)V");

}  // namespace

void ArrayList::Initialize(Loader& loader) {
  loader.LoadFromExistingClass("java/util/ArrayList",
                               firebase::util::array_list::GetClass(), kConstructor,
                               kConstructorWithCapacity);
}

Local<ArrayList> ArrayList::Create(Env& env) { return env.New(kConstructor); }

Local<ArrayList> ArrayList::Create(Env& env, size_t initial_capacity) {
  FIREBASE_ASSERT(initial_capacity <=
                  static_cast<size_t>(std::numeric_limits<jint>::max()));
  return env.New(kConstructorWithCapacity,
                 static_cast<jint>(initial_capacity));
}

}  // namespace jni
}  // namespace firestore
}  // namespace firebase
