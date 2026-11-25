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

#include <cassert>
#include <utility>

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

bool HttpsCallableReferenceInternal::Initialize(const App& app) {
  JNIEnv* env = app.GetJNIEnv();
  jobject activity = app.activity();
  return callable_reference::CacheMethodIds(env, activity) &&
         callable_result::CacheMethodIds(env, activity);
}

void HttpsCallableReferenceInternal::Terminate(JNIEnv* env) {
  callable_reference::ReleaseClass(env);
  callable_result::ReleaseClass(env);
  util::CheckAndClearJniExceptions(env);
}

HttpsCallableReferenceInternal::HttpsCallableReferenceInternal(
    std::shared_ptr<FunctionsInternal> functions, jobject obj)
    : functions_(std::move(functions)) {
  functions_->future_manager().AllocFutureApi(this, kCallableReferenceFnCount);
  obj_ = functions_->GetJNIEnv()->NewGlobalRef(obj);
}

HttpsCallableReferenceInternal::HttpsCallableReferenceInternal(
    const HttpsCallableReferenceInternal& src)
    : functions_(src.functions_) {
  if (functions_) {
    functions_->future_manager().AllocFutureApi(this,
                                                kCallableReferenceFnCount);
    obj_ = functions_->GetJNIEnv()->NewGlobalRef(src.obj_);
  }
}

HttpsCallableReferenceInternal& HttpsCallableReferenceInternal::operator=(
    const HttpsCallableReferenceInternal& src) {
  if (this == &src) return *this;
  if (obj_ != nullptr && functions_) {
    functions_->GetJNIEnv()->DeleteGlobalRef(obj_);
    obj_ = nullptr;
  }
  if (functions_ != src.functions_) {
    if (functions_) {
      functions_->future_manager().ReleaseFutureApi(this);
    }
    functions_ = src.functions_;
    if (functions_) {
      functions_->future_manager().AllocFutureApi(this,
                                                  kCallableReferenceFnCount);
    }
  }
  if (functions_ && src.obj_ != nullptr) {
    obj_ = functions_->GetJNIEnv()->NewGlobalRef(src.obj_);
  }
  return *this;
}

#if defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)
HttpsCallableReferenceInternal::HttpsCallableReferenceInternal(
    HttpsCallableReferenceInternal&& src)
    : functions_(std::move(src.functions_)), obj_(src.obj_) {
  src.obj_ = nullptr;
  if (functions_) {
    functions_->future_manager().MoveFutureApi(&src, this);
  }
}

HttpsCallableReferenceInternal& HttpsCallableReferenceInternal::operator=(
    HttpsCallableReferenceInternal&& src) {
  if (this == &src) return *this;
  if (obj_ != nullptr && functions_) {
    functions_->GetJNIEnv()->DeleteGlobalRef(obj_);
    obj_ = nullptr;
  }
  if (functions_) {
    functions_->future_manager().ReleaseFutureApi(this);
  }
  functions_ = std::move(src.functions_);
  obj_ = src.obj_;
  src.obj_ = nullptr;
  if (functions_) {
    functions_->future_manager().MoveFutureApi(&src, this);
  }
  return *this;
}
#endif  // defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)

HttpsCallableReferenceInternal::~HttpsCallableReferenceInternal() {
  if (obj_ != nullptr) {
    if (JNIEnv* env = util::GetJNIEnvFromApp()) {
      env->DeleteGlobalRef(obj_);
    }
    obj_ = nullptr;
  }
  if (functions_) {
    functions_->future_manager().ReleaseFutureApi(this);
  }
}

Functions HttpsCallableReferenceInternal::functions() const {
  return Functions(functions_);
}

bool HttpsCallableReferenceInternal::Equals(
    const HttpsCallableReferenceInternal& other) const {
  if (functions_ != other.functions_) return false;
  if (obj_ == other.obj_) return true;
  if (obj_ == nullptr || other.obj_ == nullptr || !functions_) return false;
  JNIEnv* env = functions_->GetJNIEnv();
  return env != nullptr && env->IsSameObject(obj_, other.obj_) == JNI_TRUE;
}

namespace {

struct FutureCallbackData {
  FutureCallbackData(SafeFutureHandle<HttpsCallableResult> handle_,
                     std::shared_ptr<detail::FutureImplCore> future_core_,
                     std::weak_ptr<FunctionsInternal> functions_,
                     CallableReferenceFn func_)
      : handle(handle_),
        future_core(std::move(future_core_)),
        functions(std::move(functions_)),
        func(func_) {}
  SafeFutureHandle<HttpsCallableResult> handle;
  std::shared_ptr<detail::FutureImplCore> future_core;
  std::weak_ptr<FunctionsInternal> functions;
  CallableReferenceFn func;
};

}  // namespace

// Universal callback handler.
void HttpsCallableReferenceInternal::FutureCallback(
    JNIEnv* env, jobject java_result, util::FutureResult result_code,
    const char* status_message, void* callback_data) {
  std::unique_ptr<FutureCallbackData> data(
      reinterpret_cast<FutureCallbackData*>(callback_data));
  assert(data != nullptr);
  auto functions = data->functions.lock();
  if (!functions) {
    util::CheckAndClearJniExceptions(env);
    return;
  }
  auto future_core = std::move(data->future_core);
  SafeFutureHandle<HttpsCallableResult> handle = data->handle;
  std::string status_msg = status_message ? status_message : "";

  if (result_code != util::kFutureResultSuccess) {
    // Failed, so the java_result is a FunctionsException.
    std::string message;
    Error code =
        result_code == util::kFutureResultCancelled
            ? kErrorCancelled
            : functions->ErrorFromJavaFunctionsException(java_result, &message);
    util::CheckAndClearJniExceptions(env);
    functions.reset();
    ReferenceCountedFutureImpl::CompleteCore(future_core, handle, code,
                                             message.c_str());
  } else {
    jmethodID getData = callable_result::GetMethodId(callable_result::kGetData);
    jobject java_result_data = env->CallObjectMethod(java_result, getData);
    Variant result_data = util::JavaObjectToVariant(env, java_result_data);
    env->DeleteLocalRef(java_result_data);
    util::CheckAndClearJniExceptions(env);
    HttpsCallableResult result(result_data);
    functions.reset();
    ReferenceCountedFutureImpl::CompleteCoreWithResult(
        future_core, handle, kErrorNone, status_msg.c_str(), result);
  }
}

ReferenceCountedFutureImpl* HttpsCallableReferenceInternal::future() {
  return functions_ ? functions_->future_manager().GetFutureApi(this) : nullptr;
}

Future<HttpsCallableResult> HttpsCallableReferenceInternal::Call() {
  ReferenceCountedFutureImpl* future_impl = future();
  if (!functions_ || !future_impl || obj_ == nullptr) {
    return Future<HttpsCallableResult>();
  }
  JNIEnv* env = functions_->GetJNIEnv();
  HttpsCallableResult null_result(Variant::Null());
  SafeFutureHandle<HttpsCallableResult> handle =
      future_impl->SafeAlloc(kCallableReferenceFnCall, null_result);

  jobject task = env->CallObjectMethod(
      obj_, callable_reference::GetMethodId(callable_reference::kCall));

  util::RegisterCallbackOnTask(
      env, task, FutureCallback,
      // FutureCallback will delete the newed FutureCallbackData.
      reinterpret_cast<void*>(new FutureCallbackData(
          handle, future_impl->core(), functions_, kCallableReferenceFnCall)),
      functions_->jni_task_id());

  util::CheckAndClearJniExceptions(env);
  env->DeleteLocalRef(task);
  return CallLastResult();
}

Future<HttpsCallableResult> HttpsCallableReferenceInternal::CallLastResult() {
  ReferenceCountedFutureImpl* future_impl = future();
  return future_impl ? static_cast<const Future<HttpsCallableResult>&>(
                           future_impl->LastResult(kCallableReferenceFnCall))
                     : Future<HttpsCallableResult>();
}

Future<HttpsCallableResult> HttpsCallableReferenceInternal::Call(
    const Variant& data) {
  ReferenceCountedFutureImpl* future_impl = future();
  if (!functions_ || !future_impl || obj_ == nullptr) {
    return Future<HttpsCallableResult>();
  }
  JNIEnv* env = functions_->GetJNIEnv();
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
          handle, future_impl->core(), functions_, kCallableReferenceFnCall)),
      functions_->jni_task_id());

  util::CheckAndClearJniExceptions(env);
  env->DeleteLocalRef(task);
  return CallLastResult();
}

}  // namespace internal
}  // namespace functions
}  // namespace firebase
