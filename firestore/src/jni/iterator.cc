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

#include "firestore/src/jni/iterator.h"

#include "app/src/util_android.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/loader.h"

namespace firebase {
namespace firestore {
namespace jni {
namespace {

constexpr char kClass[] = "java/util/Iterator";
Method<bool> kHasNext("hasNext", "()Z");
Method<Object> kNext("next", "()Ljava/lang/Object;");

}  // namespace

void Iterator::Initialize(Loader& loader) {
  loader.LoadFromExistingClass(kClass, util::iterator::GetClass(), kHasNext,
                               kNext);
}

bool Iterator::HasNext(Env& env) const { return env.Call(*this, kHasNext); }

Local<Object> Iterator::Next(Env& env) { return env.Call(*this, kNext); }

}  // namespace jni
}  // namespace firestore
}  // namespace firebase
