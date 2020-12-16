#include "firestore/src/tests/android/task_completion_source.h"

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

constexpr char kClassName[] =
    PROGUARD_KEEP_CLASS "com/google/android/gms/tasks/TaskCompletionSource";
Constructor<TaskCompletionSource> kConstructor(
    "(Lcom/google/android/gms/tasks/CancellationToken;)V");
Method<Object> kGetTask("getTask", "()Lcom/google/android/gms/tasks/Task;");
Method<void> kSetException("setException", "(Ljava/lang/Exception;)V");
Method<void> kSetResult("setResult", "(Ljava/lang/Object;)V");

}  // namespace

void TaskCompletionSource::Initialize(jni::Loader& loader) {
  loader.LoadClass(kClassName, kConstructor, kGetTask, kSetException,
                   kSetResult);
}

Local<TaskCompletionSource> TaskCompletionSource::Create(
    Env& env, const Object& cancellation_token) {
  return env.New(kConstructor, cancellation_token);
}

Local<Object> TaskCompletionSource::GetTask(Env& env) {
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
