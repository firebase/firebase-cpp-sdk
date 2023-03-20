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

#include "firestore/src/android/load_bundle_task_progress_android.h"

#include "firestore/src/jni/env.h"
#include "firestore/src/jni/loader.h"
#include "firestore/src/jni/string.h"

namespace firebase {
namespace firestore {
namespace {

using jni::Class;
using jni::Env;
using jni::Local;
using jni::Method;
using jni::Object;
using jni::StaticField;
using jni::String;

constexpr char kClassName[] =
    PROGUARD_KEEP_CLASS "com/google/firebase/firestore/LoadBundleTaskProgress";
Method<int32_t> kGetDocumentsLoaded("getDocumentsLoaded", "()I");
Method<int32_t> kGetTotalDocuments("getTotalDocuments", "()I");
Method<int64_t> kGetBytesLoaded("getBytesLoaded", "()J");
Method<int64_t> kGetTotalBytes("getTotalBytes", "()J");
Method<Object> kGetTaskState(
    "getTaskState",
    "()Lcom/google/firebase/firestore/LoadBundleTaskProgress$TaskState;");

constexpr char kStateEnumName[] = PROGUARD_KEEP_CLASS
    "com/google/firebase/firestore/LoadBundleTaskProgress$TaskState";
StaticField<Object> kTaskStateSuccess(
    "SUCCESS",
    "Lcom/google/firebase/firestore/LoadBundleTaskProgress$TaskState;");
StaticField<Object> kTaskStateRunning(
    "RUNNING",
    "Lcom/google/firebase/firestore/LoadBundleTaskProgress$TaskState;");

jclass g_clazz = nullptr;

}  // namespace

void LoadBundleTaskProgressInternal::Initialize(jni::Loader& loader) {
  g_clazz =
      loader.LoadClass(kClassName, kGetDocumentsLoaded, kGetTotalDocuments,
                       kGetBytesLoaded, kGetTotalBytes, kGetTaskState);
  loader.LoadClass(kStateEnumName, kTaskStateSuccess, kTaskStateRunning);
}

Class LoadBundleTaskProgressInternal::GetClass() { return Class(g_clazz); }

int32_t LoadBundleTaskProgressInternal::documents_loaded() const {
  Env env = GetEnv();
  return env.Call(obj_, kGetDocumentsLoaded);
}

int32_t LoadBundleTaskProgressInternal::total_documents() const {
  Env env = GetEnv();
  return env.Call(obj_, kGetTotalDocuments);
}

int64_t LoadBundleTaskProgressInternal::bytes_loaded() const {
  Env env = GetEnv();
  return env.Call(obj_, kGetBytesLoaded);
}

int64_t LoadBundleTaskProgressInternal::total_bytes() const {
  Env env = GetEnv();
  return env.Call(obj_, kGetTotalBytes);
}

LoadBundleTaskProgress::State LoadBundleTaskProgressInternal::state() const {
  Env env = GetEnv();
  Local<Object> state = env.Call(obj_, kGetTaskState);
  Local<Object> running_state = env.Get(kTaskStateRunning);
  Local<Object> success_state = env.Get(kTaskStateSuccess);

  if (Object::Equals(env, state, success_state)) {
    return LoadBundleTaskProgress::State::kSuccess;
  } else if (Object::Equals(env, state, running_state)) {
    return LoadBundleTaskProgress::State::kInProgress;
  } else {  // "ERROR"
    return LoadBundleTaskProgress::State::kError;
  }
}

}  // namespace firestore
}  // namespace firebase
