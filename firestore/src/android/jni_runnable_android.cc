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

#include "firestore/src/android/jni_runnable_android.h"

#include "app/src/assert.h"
#include "app/src/util_android.h"
#include "firestore/src/jni/declaration.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/loader.h"
#include "firestore/src/jni/object.h"
#include "firestore/src/jni/ownership.h"
#include "firestore/src/jni/task.h"

namespace firebase {
namespace firestore {
namespace {

using jni::Constructor;
using jni::Env;
using jni::ExceptionClearGuard;
using jni::Local;
using jni::Method;
using jni::Object;
using jni::Task;

constexpr char kJniRunnableClassName[] =
    "com/google/firebase/firestore/internal/cpp/JniRunnable";
Method<void> kDetach("detach", "()V");
Method<Task> kRunOnMainThread("runOnMainThread",
                              "()Lcom/google/android/gms/tasks/Task;");
Method<Task> kRunOnNewThread("runOnNewThread",
                             "()Lcom/google/android/gms/tasks/Task;");
Constructor<Object> kConstructor("(J)V");

void NativeRun(JNIEnv* env, jobject java_object, jlong data) {
  FIREBASE_ASSERT_MESSAGE(data != 0, "NativeRun() invoked with data==0");
  reinterpret_cast<JniRunnableBase*>(data)->Run();
}

}  // namespace

void JniRunnableBase::Initialize(jni::Loader& loader) {
  loader.LoadClass(kJniRunnableClassName, kConstructor, kDetach,
                   kRunOnMainThread, kRunOnNewThread);
  static const JNINativeMethod kJniRunnableNatives[] = {
      {"nativeRun", "(J)V", reinterpret_cast<void*>(&NativeRun)}};
  loader.RegisterNatives(kJniRunnableNatives,
                         FIREBASE_ARRAYSIZE(kJniRunnableNatives));
}

JniRunnableBase::JniRunnableBase(Env& env)
    : java_runnable_(env.New(kConstructor, reinterpret_cast<jlong>(this))) {}

JniRunnableBase::~JniRunnableBase() {
  Env env;
  Detach(env);
}

void JniRunnableBase::Detach(Env& env) {
  ExceptionClearGuard exception_clear_guard(env);
  env.Call(java_runnable_, kDetach);
}

Local<Object> JniRunnableBase::GetJavaRunnable() const {
  return java_runnable_;
}

Local<Task> JniRunnableBase::RunOnMainThread(Env& env) {
  return env.Call(java_runnable_, kRunOnMainThread);
}

Local<Task> JniRunnableBase::RunOnNewThread(Env& env) {
  return env.Call(java_runnable_, kRunOnNewThread);
}

}  // namespace firestore
}  // namespace firebase
