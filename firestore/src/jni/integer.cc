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

#include "firestore/src/jni/integer.h"

#include "app/src/util_android.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/loader.h"

namespace firebase {
namespace firestore {
namespace jni {
namespace {

constexpr char kClassName[] = "java/lang/Integer";
Constructor<Integer> kConstructor("(I)V");
Method<int32_t> kIntValue("intValue", "()I");
jclass g_clazz = nullptr;

}  // namespace

void Integer::Initialize(Loader& loader) {
  g_clazz = util::integer_class::GetClass();
  loader.LoadFromExistingClass(kClassName, g_clazz, kConstructor, kIntValue);
}

Class Integer::GetClass() { return Class(g_clazz); }

Local<Integer> Integer::Create(Env& env, int32_t value) {
  return env.New(kConstructor, value);
}

int32_t Integer::IntValue(Env& env) const { return env.Call(*this, kIntValue); }

}  // namespace jni
}  // namespace firestore
}  // namespace firebase
