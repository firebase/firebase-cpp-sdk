/*
 * Copyright 2021 Google LLC
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

#include "firestore/src/android/load_bundle_task_android.h"

#include "firestore/src/jni/env.h"
#include "firestore/src/jni/loader.h"
#include "firestore/src/jni/object.h"

namespace firebase {
namespace firestore {
namespace {

using jni::Env;
using jni::Global;
using jni::Loader;
using jni::Local;
using jni::Object;

constexpr char kLoadBundleTaskClassName[] =
    PROGUARD_KEEP_CLASS "com/google/firebase/firestore/LoadBundleTask";
jni::Method<LoadBundleTaskInternal> kAddProgressListener(
    "addOnProgressListener",
    "(Ljava/util/concurrent/Executor;Lcom/google/firebase/firestore/"
    "OnProgressListener;)"
    "Lcom/google/firebase/firestore/LoadBundleTask;");
}  // namespace

void LoadBundleTaskInternal::Initialize(Loader& loader) {
  loader.LoadClass(kLoadBundleTaskClassName, kAddProgressListener);
}

void LoadBundleTaskInternal::AddProgressListener(Env& env,
                                                 const Object& executor,
                                                 const Object& listener) {
  env.Call(*this, kAddProgressListener, executor, listener);
}

}  // namespace firestore
}  // namespace firebase
