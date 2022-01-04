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

#include "firestore/src/android/source_android.h"

#include "firestore/src/jni/env.h"
#include "firestore/src/jni/loader.h"

namespace firebase {
namespace firestore {
namespace {

using jni::Env;
using jni::Local;
using jni::Method;
using jni::Object;
using jni::StaticField;
using jni::String;

constexpr char kClass[] =
    PROGUARD_KEEP_CLASS "com/google/firebase/firestore/Source";
StaticField<Object> kDefault("DEFAULT",
                             "Lcom/google/firebase/firestore/Source;");
StaticField<Object> kServer("SERVER", "Lcom/google/firebase/firestore/Source;");
StaticField<Object> kCache("CACHE", "Lcom/google/firebase/firestore/Source;");

}  // namespace

void SourceInternal::Initialize(jni::Loader& loader) {
  loader.LoadClass(kClass, kDefault, kServer, kCache);
}

Local<Object> SourceInternal::Create(Env& env, Source source) {
  switch (source) {
    case Source::kDefault:
      return env.Get(kDefault);
    case Source::kServer:
      return env.Get(kServer);
    case Source::kCache:
      return env.Get(kCache);
  }
}

}  // namespace firestore
}  // namespace firebase
