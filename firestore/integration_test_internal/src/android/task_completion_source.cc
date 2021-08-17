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

#include "android/task_completion_source.h"

#include "firestore/src/jni/env.h"
#include "firestore/src/jni/loader.h"

namespace firebase {
namespace firestore {
namespace {

using jni::Constructor;
using jni::Env;
using jni::Local;
using jni::Method;
using jni::Object;
using jni::Task;

constexpr char kClassName[] =
    "com/google/android/gms/tasks/TaskCompletionSource";
Constructor<TaskCompletionSource> kConstructor("()V");
Constructor<TaskCompletionSource> kConstructorWithCancellationToken(
    "(Lcom/google/android/gms/tasks/CancellationToken;)V");
Method<Task> kGetTask("getTask", "()Lcom/google/android/gms/tasks/Task;");
Method<void> kSetException("setException", "(Ljava/lang/Exception;)V");
Method<void> kSetResult("setResult", "(Ljava/lang/Object;)V");

}  // namespace

void TaskCompletionSource::Initialize(jni::Loader& loader) {
  loader.LoadClass(kClassName, kConstructor, kConstructorWithCancellationToken,
                   kGetTask, kSetException, kSetResult);
}

Local<TaskCompletionSource> TaskCompletionSource::Create(Env& env) {
  return env.New(kConstructor);
}

Local<TaskCompletionSource> TaskCompletionSource::Create(
    Env& env, const Object& cancellation_token) {
  return env.New(kConstructorWithCancellationToken, cancellation_token);
}

Local<Task> TaskCompletionSource::GetTask(Env& env) {
  return env.Call(*this, kGetTask);
}

void TaskCompletionSource::SetException(Env& env,
                                        const jni::Throwable& result) {
  env.Call(*this, kSetException, result);
}

void TaskCompletionSource::SetResult(Env& env, const Object& result) {
  env.Call(*this, kSetResult, result);
}

}  // namespace firestore
}  // namespace firebase
