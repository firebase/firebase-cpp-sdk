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

#include "firestore/src/android/timestamp_android.h"

#include "firestore/src/jni/env.h"
#include "firestore/src/jni/loader.h"

namespace firebase {
namespace firestore {
namespace {

using jni::Class;
using jni::Constructor;
using jni::Env;
using jni::Local;
using jni::Method;

constexpr char kClassName[] =
    PROGUARD_KEEP_CLASS "com/google/firebase/Timestamp";
Constructor<TimestampInternal> kConstructor("(JI)V");
Method<int64_t> kGetSeconds("getSeconds", "()J");
Method<int32_t> kGetNanoseconds("getNanoseconds", "()I");

jclass g_clazz = nullptr;

}  // namespace

void TimestampInternal::Initialize(jni::Loader& loader) {
  g_clazz =
      loader.LoadClass(kClassName, kConstructor, kGetSeconds, kGetNanoseconds);
}

Class TimestampInternal::GetClass() { return Class(g_clazz); }

Local<TimestampInternal> TimestampInternal::Create(Env& env,
                                                   const Timestamp& timestamp) {
  return env.New(kConstructor, timestamp.seconds(), timestamp.nanoseconds());
}

Timestamp TimestampInternal::ToPublic(Env& env) const {
  int64_t seconds = env.Call(*this, kGetSeconds);
  int32_t nanoseconds = env.Call(*this, kGetNanoseconds);
  return Timestamp(seconds, nanoseconds);
}

}  // namespace firestore
}  // namespace firebase
