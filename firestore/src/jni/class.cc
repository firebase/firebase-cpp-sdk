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

#include "firestore/src/jni/class.h"

#include "firestore/src/jni/env.h"
#include "firestore/src/jni/loader.h"

namespace firebase {
namespace firestore {
namespace jni {
namespace {

constexpr char kClass[] = "java/lang/Class";
Method<String> kGetName("getName", "()Ljava/lang/String;");
Method<bool> kIsArray("isArray", "()Z");

}  // namespace

void Class::Initialize(Loader& loader) {
  loader.LoadClass(kClass, kGetName, kIsArray);
}

std::string Class::GetName(Env& env) const {
  if (!*this || !env.ok()) {
    return "";
  }
  Local<String> name = env.Call(*this, kGetName);
  if (!env.ok() || !name) {
    return "";
  }
  return name.ToString(env);
}

std::string Class::GetClassName(Env& env, const Object& object) {
  if (!object || !env.ok()) {
    return "";
  }
  Local<Class> clazz = env.GetObjectClass(object);
  if (!env.ok() || !clazz) {
    return "";
  }
  return clazz.GetName(env);
}

bool Class::IsArray(Env& env) const {
  if (!*this || !env.ok()) {
    return false;
  }
  return env.Call(*this, kIsArray);
}

}  // namespace jni
}  // namespace firestore
}  // namespace firebase
