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

#include "firestore/src/jni/env.h"
#include "firestore/src/jni/loader.h"

namespace firebase {
namespace firestore {
namespace jni {
namespace {

constexpr char kClassName[] = "java/lang/Integer";
StaticMethod<Integer> kValueOf("valueOf", "(I)Ljava/lang/Integer;");

}  // namespace

void Integer::Initialize(Loader& loader) {
  loader.LoadFromExistingClass(kClassName, util::integer_class::GetClass(),
                               kValueOf);
}

Local<Integer> Integer::Create(Env& env, int32_t value) {
  return env.Call(kValueOf, value);
}

}  // namespace jni
}  // namespace firestore
}  // namespace firebase
