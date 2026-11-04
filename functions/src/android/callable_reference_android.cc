// Copyright 2018 Google LLC
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

#include <memory>
#include <string>
#include <utility>

#include "app/src/app_common.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/include/firebase/variant.h"
#include "app/src/reference_counted_future_impl.h"
#include "app/src/util_android.h"
#include "functions/src/android/functions_android.h"
#include "functions/src/include/firebase/functions.h"
#include "functions/src/include/firebase/functions/callable_result.h"
#include "functions/src/include/firebase/functions/common.h"

namespace firebase::functions::internal {

METHOD_LOOKUP_DEFINITION(callable_reference,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/functions/HttpsCallableReference",
                         CALLABLE_REFERENCE_METHODS)

METHOD_LOOKUP_DEFINITION(callable_result,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/functions/HttpsCallableResult",
                         CALLABLE_RESULT_METHODS)

enum HttpsCallableReferenceFn {
  kHttpsCallableReferenceFnCall = 0,
  kHttpsCallableReferenceFnCount,
};

namespace {

JNIEnv* ResolveJniEnv(FunctionsInternal* primary,
                      FunctionsInternal* secondary = nullptr) noexcept {
  if (primary && primary->app()) {
    return primary->app()->GetJNIEnv();
  }
  if (secondary && secondary->app()) {
    return secondary->app()->GetJNIEnv();
  }
  if (App* any_app = app_common::GetAnyApp()) {
    return any_app->GetJNIEnv();
  }
  return nullptr;
}

}  // namespace

HttpsCallableReferenceInternal::HttpsCallableReferenceInternal(
    FunctionsInternal* functions, jobject wrapper_obj)
    : functions_(functions), future_impl_(kHttpsCallableReferenceFnCount), obj_(nullptr) {
  if (functions_ && functions_->app()) {
    functions_->    if (wrapper_obj) {
      obj_ = functions_->app()->GetJNIEnv()->NewGlobalRef(wrapper_obj);
    }
  }
}

void HttpsCallableReferenceInternal::Cleanup(
    FunctionsInternal* fallback_functions) noexcept {
  if (obj_ != nullptr) {
    if (JNIEnv* env = ResolveJniEnv(functions_, fallback_functions)) {
      env->DeleteGlobalRef(obj_);
    }
    obj_ = nullptr;
  }
  if (functions_ != nullptr) {
    functions_->    functions_ = nullptr;
  }
}

HttpsCallableReferenceInternal::~HttpsCallableReferenceInternal() { Cleanup(); }

HttpsCallableReferenceInternal::HttpsCallableReferenceInternal(
    const HttpsCallableReferenceInternal& src)
    : functions_(src.functions_), obj_(nullptr) {
  if (functions_ && functions_->app()) {
    functions_->    if (src.obj_ != nullptr) {
      obj_ = functions_->app()->GetJNIEnv()->NewGlobalRef(src.obj_);
    }
  }
}

HttpsCallableReferenceInternal& HttpsCallableReferenceInternal::operator=(
    const HttpsCallableReferenceInternal& src) {
  if (this == &src) {
    return *this;
  }
  jobject new_obj = nullptr;
  if (src.obj_ != nullptr && src.functions_ != nullptr &&
      src.functions_->app() != nullptr) {
    new_obj = src.functions_->app()->GetJNIEnv()->NewGlobalRef(src.obj_);
  }

  if (obj_ != nullptr) {
    if (JNIEnv* env = ResolveJniEnv(functions_, src.functions_)) {
      env->DeleteGlobalRef(obj_);
    }
  }
  obj_ = new_obj;

  if (functions_ != src.functions_) {
    if (functions_) {
      functions_->    }
    functions_ = src.functions_;
      }
  return *this;
}

HttpsCallableReferenceInternal::HttpsCallableReferenceInternal(
    HttpsCallableReferenceInternal&& src) noexcept
    : functions_(std::exchange(src.functions_, nullptr)),
      obj_(std::exchange(src.obj_, nullptr)) {
  }

HttpsCallableReferenceInternal& HttpsCallableReferenceInternal::operator=(
    HttpsCallableReferenceInternal&& src) noexcept {
  if (this == &src) {
    return *this;
  }
  Cleanup(src.functions_);
  functions_ = std::exchange(src.functions_, nullptr);
  obj_ = std::exchange(src.obj_, nullptr);
    return *this;
}

Functions* HttpsCallableReferenceInternal::functions() const {
  if (!functions_) return nullptr;
  return Functions::GetInstance(functions_->app(), functions_->region());
}

bool HttpsCallableReferenceInternal::is_valid() const {
  return functions_ != nullptr && obj_ != nullptr;
}

bool HttpsCallableReferenceInternal::operator==(
    const HttpsCallableReferenceInternal& other) const {
  if (functions_ != other.functions_) return false;
  if (obj_ == other.obj_) return true;
  if (!obj_ || !other.obj_ || !functions_ || !functions_->app()) return false;
  JNIEnv* env = functions_->app()->GetJNIEnv();
  return env->IsSameObject(obj_, other.obj_) == JNI_TRUE;
}

namespace {

// Completion liveness is tied to the owning FunctionsInternal (whose
// FutureManager owns `future_impl`, including orphaned impls of destroyed
// HttpsCallableReferences), not to the HttpsCallableReferenceInternal, so a
// chained `functions->GetHttpsCallable("fn").Call()` whose temporary reference
// is destroyed immediately still completes its Future.
struct FutureCallbackData {
  FutureCallbackData(SafeFutureHandle<HttpsCallableResult> handle_,
                     ReferenceCountedFutureImpl* impl_,
                     std::shared_ptr<FunctionsLifetimeState> lifetime_state_)
      : handle(handle_),
        future_impl(impl_),
        lifetime_state(std::move(lifetime_state_)) {}
  SafeFutureHandle<HttpsCallableResult> handle;
  ReferenceCountedFutureImpl* future_impl;
  std::shared_ptr<FunctionsLifetimeState> lifetime_state;
};

}  // namespace

void HttpsCallableReferenceInternal::FutureCallback(
    JNIEnv* env, jobject result, util::FutureResult result_code,
    const char* status_message, void* callback_data) {
  std::unique_ptr<FutureCallbackData> data(
      static_cast<FutureCallbackData*>(callback_data));
  if (!data || !data->future_impl || !data->lifetime_state) return;

  MutexLock lock(data->lifetime_state->mutex);
  if (!data->lifetime_state->alive) return;

  if (result_code == util::kFutureResultCancelled) {
    data->future_impl->Complete(data->handle, kErrorCancelled,
                                GetErrorMessage(kErrorCancelled));
  } else if (result_code == util::kFutureResultFailure) {
    std::string error_msg = status_message ? status_message : "";
    Variant details;
    Error code = FunctionsInternal::ErrorFromJavaFunctionsException(
        env, result, &error_msg, &details);
    if (error_msg.empty()) {
      error_msg = GetErrorMessage(code);
    }
    HttpsCallableResult callable_result(std::move(details));
    data->future_impl->CompleteWithResult(data->handle, code, error_msg.c_str(),
                                          callable_result);
  } else if (!result) {
    data->future_impl->Complete(data->handle, kErrorInternal,
                                GetErrorMessage(kErrorInternal));
  } else {
    jobject java_result = env->CallObjectMethod(
        result, callable_result::GetMethodId(callable_result::kGetData));
    if (util::CheckAndClearJniExceptions(env)) {
      data->future_impl->Complete(data->handle, kErrorInternal,
                                  GetErrorMessage(kErrorInternal));
    } else {
      Variant result_variant = util::JavaObjectToVariant(env, java_result);
      util::CheckAndClearJniExceptions(env);
      if (java_result != nullptr) {
        env->DeleteLocalRef(java_result);
      }
      HttpsCallableResult callable_result(std::move(result_variant));
      data->future_impl->CompleteWithResult(data->handle, kErrorNone, "",
                                            callable_result);
    }
  }
}

Future<HttpsCallableResult> HttpsCallableReferenceInternal::Call() {
  ReferenceCountedFutureImpl* future_api = future();
  if (!future_api || !functions_ || !functions_->app() || !obj_) {
    return Future<HttpsCallableResult>();
  }
  SafeFutureHandle<HttpsCallableResult> handle =
      future_api->SafeAlloc<HttpsCallableResult>(kHttpsCallableReferenceFnCall);
  JNIEnv* env = functions_->app()->GetJNIEnv();

  jobject task = env->CallObjectMethod(
      obj_, callable_reference::GetMethodId(callable_reference::kCall));
  if (util::CheckAndClearJniExceptions(env) || !task) {
    future_api->Complete(handle, kErrorInternal,
                         GetErrorMessage(kErrorInternal));
    return MakeFuture(future_api, handle);
  }

  FutureCallbackData* callback_data = new FutureCallbackData(
      handle, future_api, functions_->lifetime_state());
  util::RegisterCallbackOnTask(env, task, FutureCallback, callback_data,
                               functions_->jni_task_id().c_str());
  env->DeleteLocalRef(task);
  util::CheckAndClearJniExceptions(env);

  return MakeFuture(future_api, handle);
}

Future<HttpsCallableResult> HttpsCallableReferenceInternal::Call(
    const Variant& data) {
  ReferenceCountedFutureImpl* future_api = future();
  if (!future_api || !functions_ || !functions_->app() || !obj_) {
    return Future<HttpsCallableResult>();
  }
  SafeFutureHandle<HttpsCallableResult> handle =
      future_api->SafeAlloc<HttpsCallableResult>(kHttpsCallableReferenceFnCall);
  JNIEnv* env = functions_->app()->GetJNIEnv();

  jobject java_object = util::VariantToJavaObject(env, data);
  if (util::CheckAndClearJniExceptions(env)) {
    if (java_object) env->DeleteLocalRef(java_object);
    future_api->Complete(handle, kErrorInvalidArgument,
                         GetErrorMessage(kErrorInvalidArgument));
    return MakeFuture(future_api, handle);
  }

  jobject task = env->CallObjectMethod(
      obj_, callable_reference::GetMethodId(callable_reference::kCallWithParam),
      java_object);
  if (java_object) env->DeleteLocalRef(java_object);
  if (util::CheckAndClearJniExceptions(env) || !task) {
    future_api->Complete(handle, kErrorInternal,
                         GetErrorMessage(kErrorInternal));
    return MakeFuture(future_api, handle);
  }

  FutureCallbackData* callback_data = new FutureCallbackData(
      handle, future_api, functions_->lifetime_state());
  util::RegisterCallbackOnTask(env, task, FutureCallback, callback_data,
                               functions_->jni_task_id().c_str());
  env->DeleteLocalRef(task);
  util::CheckAndClearJniExceptions(env);

  return MakeFuture(future_api, handle);
}

Future<HttpsCallableResult> HttpsCallableReferenceInternal::CallLastResult() {
  ReferenceCountedFutureImpl* future_api = future();
  return future_api ? static_cast<const Future<HttpsCallableResult>&>(
                          future_api->LastResult(kHttpsCallableReferenceFnCall))
                    : Future<HttpsCallableResult>();
}


}  // namespace firebase::functions::internal
