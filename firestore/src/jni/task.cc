// Copyright 2021 Google LLC

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
