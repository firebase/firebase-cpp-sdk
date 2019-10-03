// Copyright 2017 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "functions/src/android/callable_reference_android.h"

#include <jni.h>

#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/variant.h"
#include "app/src/util_android.h"
#include "functions/src/android/functions_android.h"
#include "functions/src/include/firebase/functions.h"
#include "functions/src/include/firebase/functions/callable_result.h"
#include "functions/src/include/firebase/functions/common.h"

namespace firebase {
namespace functions {
namespace internal {

// clang-format off
#define CALLABLE_REFERENCE_METHODS(X)                                          \
  X(Call, "call",                                                              \
    "()"                                                                       \
    "Lcom/google/android/gms/tasks/Task;"),                                    \
  X(CallWithData, "call",                                                      \
    "(Ljava/lang/Object;)"                                                     \
    "Lcom/google/android/gms/tasks/Task;")
// clang-format on

METHOD_LOOKUP_DECLARATION(callable_reference, CALLABLE_REFERENCE_METHODS)
METHOD_LOOKUP_DEFINITION(callable_reference,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/functions/HttpsCallableReference",
                         CALLABLE_REFERENCE_METHODS)

enum CallableReferenceFn {
  kCallableReferenceFnCall = 0,
  kCallableReferenceFnCount,
};

// clang-format off
#define CALLABLE_RESULT_METHODS(X)                                             \
  X(GetData, "getData",                                                        \
    "()"                                                                       \
    "Ljava/lang/Object;")
// clang-format on

METHOD_LOOKUP_DECLARATION(callable_result, CALLABLE_RESULT_METHODS)
METHOD_LOOKUP_DEFINITION(callable_result,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/functions/HttpsCallableResult",
                         CALLABLE_RESULT_METHODS)

bool HttpsCallableReferenceInternal::Initialize(App* app) {
  JNIEnv* env = app->GetJNIEnv();
  jobject activity = app->activity();
  return callable_reference::CacheMethodIds(env, activity) &&
         callable_result::CacheMethodIds(env, activity);
}

void HttpsCallableReferenceInternal::Terminate(App* app) {
  JNIEnv* env = app->GetJNIEnv();
  callable_reference::ReleaseClass(env);
  util::CheckAndClearJniExceptions(env);
}

HttpsCallableReferenceInternal::HttpsCallableReferenceInternal(
    FunctionsInternal* functions, jobject obj)
    : functions_(functions) {
  functions_->future_manager().AllocFutureApi(this, kCallableReferenceFnCount);
  obj_ = functions_->app()->GetJNIEnv()->NewGlobalRef(obj);
}

HttpsCallableReferenceInternal::HttpsCallableReferenceInternal(
    const HttpsCallableReferenceInternal& src)
    : functions_(src.functions_) {
  functions_->future_manager().AllocFutureApi(this, kCallableReferenceFnCount);
  obj_ = functions_->app()->GetJNIEnv()->NewGlobalRef(src.obj_);
}

HttpsCallableReferenceInternal& HttpsCallableReferenceInternal::operator=(
    const HttpsCallableReferenceInternal& src) {
  obj_ = functions_->app()->GetJNIEnv()->NewGlobalRef(src.obj_);
  return *this;
}

#if defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)
HttpsCallableReferenceInternal::HttpsCallableReferenceInternal(
    HttpsCallableReferenceInternal&& src)
    : functions_(src.functions_) {
  obj_ = src.obj_;
  src.obj_ = nullptr;
  functions_->future_manager().MoveFutureApi(&src, this);
}

HttpsCallableReferenceInternal& HttpsCallableReferenceInternal::operator=(
    HttpsCallableReferenceInternal&& src) {
  obj_ = src.obj_;
  src.obj_ = nullptr;
  functions_->future_manager().MoveFutureApi(&src, this);
  return *this;
}
#endif  // defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)

HttpsCallableReferenceInternal::~HttpsCallableReferenceInternal() {
  if (obj_ != nullptr) {
    functions_->app()->GetJNIEnv()->DeleteGlobalRef(obj_);
    obj_ = nullptr;
  }
  functions_->future_manager().ReleaseFutureApi(this);
}

Functions* HttpsCallableReferenceInternal::functions() {
  return Functions::GetInstance(functions_->app());
}

namespace {

struct FutureCallbackData {
  FutureCallbackData(SafeFutureHandle<HttpsCallableResult> handle_,
                     ReferenceCountedFutureImpl* impl_,
                     FunctionsInternal* functions_, CallableReferenceFn func_)
      : handle(handle_), impl(impl_), functions(functions_), func(func_) {}
  SafeFutureHandle<HttpsCallableResult> handle;
  ReferenceCountedFutureImpl* impl;
  FunctionsInternal* functions;
  CallableReferenceFn func;
};

}  // namespace

// Universal callback handler.
void HttpsCallableReferenceInternal::FutureCallback(
    JNIEnv* env, jobject java_result, util::FutureResult result_code,
    const char* status_message, void* callback_data) {
  auto data = reinterpret_cast<FutureCallbackData*>(callback_data);
  assert(data != nullptr);
  if (result_code != util::kFutureResultSuccess) {
    // Failed, so the java_result is a FunctionsException.
    std::string message;
    Error code = result_code == util::kFutureResultCancelled
                     ? kErrorCancelled
                     : data->functions->ErrorFromJavaFunctionsException(
                           java_result, &message);
    data->impl->Complete(data->handle, code, message.c_str());
  } else {
    jmethodID getData = callable_result::GetMethodId(callable_result::kGetData);
    jobject java_result_data = env->CallObjectMethod(java_result, getData);
    Variant result_data = util::JavaObjectToVariant(env, java_result_data);
    env->DeleteLocalRef(java_result_data);
    HttpsCallableResult result(result_data);
    data->impl->CompleteWithResult(data->handle, kErrorNone, status_message,
                                   result);
  }
  // TODO(klimt): Do we need to clear any references to java_result?
  delete data;

  util::CheckAndClearJniExceptions(env);
}

ReferenceCountedFutureImpl* HttpsCallableReferenceInternal::future() {
  return functions_->future_manager().GetFutureApi(this);
}

Future<HttpsCallableResult> HttpsCallableReferenceInternal::Call() {
  JNIEnv* env = functions_->app()->GetJNIEnv();
  ReferenceCountedFutureImpl* future_impl = future();
  HttpsCallableResult null_result(Variant::Null());
  SafeFutureHandle<HttpsCallableResult> handle =
      future_impl->SafeAlloc(kCallableReferenceFnCall, null_result);

  jobject task = env->CallObjectMethod(
      obj_, callable_reference::GetMethodId(callable_reference::kCall));

  util::RegisterCallbackOnTask(
      env, task, FutureCallback,
      // FutureCallback will delete the newed FutureCallbackData.
      reinterpret_cast<void*>(new FutureCallbackData(
          handle, future(), functions_, kCallableReferenceFnCall)),
      kApiIdentifier);

  util::CheckAndClearJniExceptions(env);
  env->DeleteLocalRef(task);
  return CallLastResult();
}

Future<HttpsCallableResult> HttpsCallableReferenceInternal::CallLastResult() {
  return static_cast<const Future<HttpsCallableResult>&>(
      future()->LastResult(kCallableReferenceFnCall));
}

Future<HttpsCallableResult> HttpsCallableReferenceInternal::Call(
    const Variant& data) {
  JNIEnv* env = functions_->app()->GetJNIEnv();
  ReferenceCountedFutureImpl* future_impl = future();
  HttpsCallableResult null_result(Variant::Null());
  SafeFutureHandle<HttpsCallableResult> handle =
      future_impl->SafeAlloc(kCallableReferenceFnCall, null_result);

  jobject java_data = util::VariantToJavaObject(env, data);
  jobject task = env->CallObjectMethod(
      obj_, callable_reference::GetMethodId(callable_reference::kCallWithData),
      java_data);
  env->DeleteLocalRef(java_data);

  util::RegisterCallbackOnTask(
      env, task, FutureCallback,
      // FutureCallback will delete the newed FutureCallbackData.
      reinterpret_cast<void*>(new FutureCallbackData(
          handle, future(), functions_, kCallableReferenceFnCall)),
      kApiIdentifier);

  util::CheckAndClearJniExceptions(env);
  env->DeleteLocalRef(task);
  return CallLastResult();
}

}  // namespace internal
}  // namespace functions
}  // namespace firebase
