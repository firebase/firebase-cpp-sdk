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

#include "firestore/src/jni/list.h"

#include <limits>

#include "app/src/assert.h"
#include "app/src/util_android.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/loader.h"

namespace firebase {
namespace firestore {
namespace jni {
namespace {

Method<Object> kGet("get", "(I)Ljava/lang/Object;");
Method<Object> kSet("set", "(ILjava/lang/Object;)Ljava/lang/Object;");

}  // namespace

void List::Initialize(Loader& loader) {
  loader.LoadClass("java/util/List", kGet, kSet);
}

Class List::GetClass() { return Class(kGet.clazz()); }

Local<Object> List::Get(Env& env, size_t i) const {
  FIREBASE_ASSERT(i <= static_cast<size_t>(std::numeric_limits<jint>::max()));
  return env.Call(*this, kGet, static_cast<jint>(i));
}

Local<Object> List::Set(Env& env, size_t i, const Object& object) {
  FIREBASE_ASSERT(i <= static_cast<size_t>(std::numeric_limits<jint>::max()));
  return env.Call(*this, kSet, static_cast<jint>(i), object);
}

}  // namespace jni
}  // namespace firestore
}  // namespace firebase
