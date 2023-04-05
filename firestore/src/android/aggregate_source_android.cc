/*
 * Copyright 2023 Google LLC
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

#include "firestore/src/android/aggregate_source_android.h"

#include "firestore/src/jni/env.h"
#include "firestore/src/jni/loader.h"

namespace firebase {
namespace firestore {
namespace {

using jni::Env;
using jni::Local;
using jni::Object;
using jni::StaticField;

constexpr char kClass[] =
    PROGUARD_KEEP_CLASS "com/google/firebase/firestore/AggregateSource";
StaticField<Object> kServer("SERVER",
                            "Lcom/google/firebase/firestore/AggregateSource;");

}  // namespace

void AggregateSourceInternal::Initialize(jni::Loader& loader) {
  loader.LoadClass(kClass, kServer);
}

Local<Object> AggregateSourceInternal::Create(
    Env& env, AggregateSource aggregate_source) {
  switch (aggregate_source) {
    case AggregateSource::kServer:
      return env.Get(kServer);
  }
}

}  // namespace firestore
}  // namespace firebase
