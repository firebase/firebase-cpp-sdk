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

#include "firestore/src/jni/boolean.h"

#include "app/src/util_android.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/loader.h"

namespace firebase {
namespace firestore {
namespace jni {
namespace {

constexpr char kClassName[] = "java/lang/Boolean";
Constructor<Boolean> kConstructor("(Z)V");
Method<bool> kBooleanValue("booleanValue", "()Z");
jclass g_clazz = nullptr;

}  // namespace

void Boolean::Initialize(Loader& loader) {
  g_clazz = util::boolean_class::GetClass();
  loader.LoadFromExistingClass(kClassName, g_clazz, kConstructor,
                               kBooleanValue);
}

Class Boolean::GetClass() { return Class(g_clazz); }

Local<Boolean> Boolean::Create(Env& env, bool value) {
  return env.New(kConstructor, value);
}

bool Boolean::BooleanValue(Env& env) const {
  return env.Call(*this, kBooleanValue);
}

}  // namespace jni
}  // namespace firestore
}  // namespace firebase
