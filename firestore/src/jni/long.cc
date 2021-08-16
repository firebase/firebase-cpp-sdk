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

#include "firestore/src/jni/long.h"

#include "app/src/util_android.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/loader.h"

namespace firebase {
namespace firestore {
namespace jni {
namespace {

constexpr char kClassName[] = "java/lang/Long";
Constructor<Long> kConstructor("(J)V");
Method<int64_t> kLongValue("longValue", "()J");
jclass g_clazz = nullptr;

}  // namespace

void Long::Initialize(Loader& loader) {
  g_clazz = util::long_class::GetClass();
  loader.LoadFromExistingClass(kClassName, g_clazz, kConstructor, kLongValue);
}

Class Long::GetClass() { return Class(g_clazz); }

Local<Long> Long::Create(Env& env, int64_t value) {
  return env.New(kConstructor, value);
}

int64_t Long::LongValue(Env& env) const { return env.Call(*this, kLongValue); }

}  // namespace jni
}  // namespace firestore
}  // namespace firebase
