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

#include "firestore/src/android/direction_android.h"

#include "firestore/src/jni/env.h"
#include "firestore/src/jni/loader.h"

namespace firebase {
namespace firestore {
namespace {

using jni::Env;
using jni::Local;
using jni::Object;
using jni::StaticField;

constexpr char kClass[] =
    PROGUARD_KEEP_CLASS "com/google/firebase/firestore/Query$Direction";
StaticField<Object> kAscending(
    "ASCENDING", "Lcom/google/firebase/firestore/Query$Direction;");
StaticField<Object> kDescending(
    "DESCENDING", "Lcom/google/firebase/firestore/Query$Direction;");

}  // namespace

void DirectionInternal::Initialize(jni::Loader& loader) {
  loader.LoadClass(kClass, kAscending, kDescending);
}

Local<Object> DirectionInternal::Create(Env& env, Query::Direction direction) {
  if (direction == Query::Direction::kAscending) {
    return env.Get(kAscending);
  } else {
    return env.Get(kDescending);
  }
}

}  // namespace firestore
}  // namespace firebase
