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

#include "firestore/src/jni/object.h"

#include "firestore/src/jni/class.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/loader.h"

namespace firebase::firestore::jni {
namespace {

Method<bool> kEquals("equals", "(Ljava/lang/Object;)Z");
Method<String> kToString("toString", "()Ljava/lang/String;");
jclass object_class = nullptr;

}  // namespace

void Object::Initialize(Loader& loader) {
  jclass clazz = util::object::GetClass();
  if (clazz != nullptr) {
    object_class = clazz;
    loader.LoadFromExistingClass("java/lang/Object", object_class, kEquals,
                                 kToString);
  } else {
    object_class = loader.LoadClass("java/lang/Object", kEquals, kToString);
  }
}

Class Object::GetClass() { return Class(object_class); }

std::string Object::ToString(Env& env) const {
  if (!*this || !env.ok()) {
    return "";
  }
  Local<String> java_string = env.Call(*this, kToString);
  if (!env.ok() || !java_string) {
    return "";
  }
  return java_string.ToString(env);
}

bool Object::Equals(Env& env, const Object& other) const {
  if (!env.ok()) return false;
  if (!*this && !other) return true;
  if (!*this || !other) return false;
  return env.Call(*this, kEquals, other);
}

bool Object::Equals(Env& env, const Object& lhs, const Object& rhs) {
  if (!env.ok()) return false;

  // Most likely only happens when comparing one with itself or both are null.
  if (lhs.get() == rhs.get()) return true;

  // If only one of them is nullptr, then they cannot equal.
  if (!lhs || !rhs) return false;

  return lhs.Equals(env, rhs);
}

}  // namespace firebase::firestore::jni
