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

#include "firestore/src/jni/string.h"

#include <cstring>

#include "app/src/log.h"
#include "app/src/util_android.h"
#include "firestore/src/jni/array.h"
#include "firestore/src/jni/class.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/loader.h"

namespace firebase {
namespace firestore {
namespace jni {
namespace {

Constructor<String> kNewFromBytes("([BLjava/lang/String;)V");
Method<Array<uint8_t>> kGetBytes("getBytes", "(Ljava/lang/String;)[B");
jclass string_class = nullptr;
jstring utf8_string = nullptr;

}  // namespace

void String::Initialize(Env& env, Loader& loader) {
  string_class = util::string::GetClass();
  loader.LoadFromExistingClass("java/lang/String", string_class, kNewFromBytes,
                               kGetBytes);

  FIREBASE_DEV_ASSERT(utf8_string == nullptr);
  Local<String> utf8(env.get(), env.get()->NewStringUTF("UTF-8"));
  if (!env.ok()) return;

  utf8_string = Global<String>(utf8).release();
}

void String::Terminate(Env& env) {
  if (utf8_string) {
    env.get()->DeleteGlobalRef(utf8_string);
    utf8_string = nullptr;
  }
}

Class String::GetClass() { return Class(string_class); }

String String::GetUtf8() { return String(utf8_string); }

Local<String> String::Create(Env& env,
                             const Array<uint8_t>& bytes,
                             const String& encoding) {
  return env.New(kNewFromBytes, bytes, encoding);
}

Local<Array<uint8_t>> String::GetBytes(Env& env, const String& encoding) const {
  return env.Call(*this, kGetBytes, encoding);
}

std::string String::ToString(Env& env) const { return env.ToStringUtf(*this); }

}  // namespace jni
}  // namespace firestore
}  // namespace firebase
