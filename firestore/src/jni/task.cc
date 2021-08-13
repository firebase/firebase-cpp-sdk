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

#include "firestore/src/jni/task.h"

#include "firestore/src/jni/env.h"
#include "firestore/src/jni/loader.h"

namespace firebase {
namespace firestore {
namespace jni {
namespace {

constexpr char kClass[] =
    PROGUARD_KEEP_CLASS "com/google/android/gms/tasks/Task";

Method<Object> kGetResult("getResult", "()Ljava/lang/Object;");
Method<Throwable> kGetException("getException", "()Ljava/lang/Exception;");
Method<bool> kIsComplete("isComplete", "()Z");
Method<bool> kIsSuccessful("isSuccessful", "()Z");
Method<bool> kIsCanceled("isCanceled", "()Z");

}  // namespace

void Task::Initialize(Loader& loader) {
  loader.LoadClass(kClass, kGetResult, kGetException, kIsComplete,
                   kIsSuccessful, kIsCanceled);
}

Local<Object> Task::GetResult(Env& env) const {
  return env.Call(*this, kGetResult);
}

Local<Throwable> Task::GetException(Env& env) const {
  return env.Call(*this, kGetException);
}

bool Task::IsComplete(Env& env) const { return env.Call(*this, kIsComplete); }

bool Task::IsSuccessful(Env& env) const {
  return env.Call(*this, kIsSuccessful);
}

bool Task::IsCanceled(Env& env) const { return env.Call(*this, kIsCanceled); }

}  // namespace jni
}  // namespace firestore
}  // namespace firebase
