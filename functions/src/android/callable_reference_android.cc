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
#include "app/src/jni/declaration.h"
#include "app/src/jni/exception.h"
#include "app/src/jni/ownership.h"
#include "app/src/reference_counted_future_impl.h"
#include "app/src/util_android.h"
#include "functions/src/android/functions_android.h"
#include "functions/src/include/firebase/functions.h"
#include "functions/src/include/firebase/functions/callable_result.h"
#include "functions/src/include/firebase/functions/common.h"

namespace firebase::functions::internal {

enum HttpsCallableReferenceFn {
  kHttpsCallableReferenceFnCall = 0,
  kHttpsCallableReferenceFnCount,
};

namespace {

JNIEnv* ResolveJniEnv(FunctionsInternal* primary,
                      FunctionsInternal* secondary = nullptr) noexcept {
  if (primary && primary->app()) return primary->app()->GetJNIEnv();
  if (secondary && secondary->app()) return secondary->app()->GetJNIEnv();
  if (App* any_app = app_common::GetAnyApp()) return any_app->GetJNIEnv();
  return nullptr;
}

struct FutureCallbackData {
  SafeFutureHandle<HttpsCallableResult> handle;
  std::shared_ptr<ReferenceCountedFutureImpl> future_impl;
  std::shared_ptr<FunctionsLifetimeState> lifetime_state;
};

}  // namespace

bool HttpsCallableReferenceInternal::Initialize(App* app) {
  if (!app) return false;
  JNIEnv* env = app->GetJNIEnv();
  jobject activity = app->activity();
  return callable_reference::CacheMethodIds(env, activity) &&
         callable_result::CacheMethodIds(env, activity);
}

void HttpsCallableReferenceInternal::Terminate(App* app) {
  if (!app) return;
  JNIEnv* env = app->GetJNIEnv();
  callable_reference::ReleaseClass(env);
  callable_result::ReleaseClass(env);
  util::CheckAndClearJniExceptions(env);
}

HttpsCallableReferenceInternal::HttpsCallableReferenceInternal(
    FunctionsInternal* functions, jobject wrapper_obj)
    : future_impl_(std::make_shared<ReferenceCountedFutureImpl>(
          kHttpsCallableReferenceFnCount)),
      functions_(functions) {
  if (functions_ && functions_->app() && wrapper_obj) {
    obj_ = functions_->app()->GetJNIEnv()->NewGlobalRef(wrapper_obj);
  }
}

void HttpsCallableReferenceInternal::Cleanup(
    FunctionsInternal* fallback_functions) noexcept {
  if (obj_) {
    if (JNIEnv* env = ResolveJniEnv(functions_, fallback_functions)) {
      env->DeleteGlobalRef(obj_);
    }
    obj_ = nullptr;
  }
  functions_ = nullptr;
}

HttpsCallableReferenceInternal::~HttpsCallableReferenceInternal() { Cleanup(); }

HttpsCallableReferenceInternal::HttpsCallableReferenceInternal(
    const HttpsCallableReferenceInternal& src)
    : HttpsCallableReferenceInternal(src.functions_, src.obj_) {}

HttpsCallableReferenceInternal& HttpsCallableReferenceInternal::operator=(
    const HttpsCallableReferenceInternal& src) {
  if (this == &src) return *this;
  jobject new_obj =
      (src.obj_ && src.functions_ && src.functions_->app())
          ? src.functions_->app()->GetJNIEnv()->NewGlobalRef(src.obj_)
          : nullptr;
  Cleanup(src.functions_);
  obj_ = new_obj;
  functions_ = src.functions_;
  if (!future_impl_) {
    future_impl_ = std::make_shared<ReferenceCountedFutureImpl>(
        kHttpsCallableReferenceFnCount);
  }
  return *this;
}

HttpsCallableReferenceInternal::HttpsCallableReferenceInternal(
    HttpsCallableReferenceInternal&& src) noexcept
    : future_impl_(std::move(src.future_impl_)),
      functions_(std::exchange(src.functions_, nullptr)),
      obj_(std::exchange(src.obj_, nullptr)) {}

HttpsCallableReferenceInternal& HttpsCallableReferenceInternal::operator=(
    HttpsCallableReferenceInternal&& src) noexcept {
  if (this != &src) {
    Cleanup(src.functions_);
    future_impl_ = std::move(src.future_impl_);
    functions_ = std::exchange(src.functions_, nullptr);
    obj_ = std::exchange(src.obj_, nullptr);
  }
  return *this;
}

Functions* HttpsCallableReferenceInternal::functions() const {
  return functions_
             ? Functions::GetInstance(functions_->app(), functions_->region())
             : nullptr;
}

bool HttpsCallableReferenceInternal::is_valid() const {
  return functions_ != nullptr && obj_ != nullptr;
}

bool HttpsCallableReferenceInternal::operator==(
    const HttpsCallableReferenceInternal& other) const {
  if (functions_ != other.functions_) return false;
  if (obj_ == other.obj_) return true;
  if (!obj_ || !other.obj_ || !functions_ || !functions_->app()) return false;
  return functions_->app()->GetJNIEnv()->IsSameObject(obj_, other.obj_) ==
         JNI_TRUE;
}

void HttpsCallableReferenceInternal::FutureCallback(
    JNIEnv* env, jobject result, firebase::util::FutureResult result_code,
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
    if (error_msg.empty()) error_msg = GetErrorMessage(code);
    data->future_impl->CompleteWithResult(
        data->handle, code, error_msg.c_str(),
        HttpsCallableResult(std::move(details)));
  } else if (!result) {
    data->future_impl->Complete(data->handle, kErrorInternal,
                                GetErrorMessage(kErrorInternal));
  } else {
    jobject java_result = env->CallObjectMethod(
        result, callable_result::GetMethodId(callable_result::kGetData));
    if (jni::CheckAndClearException(env)) {
      data->future_impl->Complete(data->handle, kErrorInternal,
                                  GetErrorMessage(kErrorInternal));
    } else {
      Variant v = firebase::util::JavaObjectToVariant(env, java_result);
      jni::CheckAndClearException(env);
      if (java_result) env->DeleteLocalRef(java_result);
      data->future_impl->CompleteWithResult(
          data->handle, kErrorNone, "", HttpsCallableResult(std::move(v)));
    }
  }
}

Future<HttpsCallableResult> HttpsCallableReferenceInternal::DispatchTask(
    JNIEnv* env, SafeFutureHandle<HttpsCallableResult> handle, jobject task) {
  if (jni::CheckAndClearException(env) || !task) {
    future_impl_->Complete(handle, kErrorInternal,
                           GetErrorMessage(kErrorInternal));
    return MakeFuture(future_impl_.get(), handle);
  }
  auto* cb_data = new FutureCallbackData{handle, future_impl_,
                                         functions_->lifetime_state()};
  firebase::util::RegisterCallbackOnTask(env, task, FutureCallback, cb_data,
                                         functions_->jni_task_id().c_str());
  env->DeleteLocalRef(task);
  jni::CheckAndClearException(env);
  return MakeFuture(future_impl_.get(), handle);
}

Future<HttpsCallableResult> HttpsCallableReferenceInternal::Call() {
  if (!future_impl_ || !functions_ || !functions_->app() || !obj_) {
    return Future<HttpsCallableResult>();
  }
  auto handle = future_impl_->SafeAlloc<HttpsCallableResult>(
      kHttpsCallableReferenceFnCall);
  JNIEnv* env = functions_->app()->GetJNIEnv();
  jobject task = env->CallObjectMethod(
      obj_, callable_reference::GetMethodId(callable_reference::kCall));
  return DispatchTask(env, handle, task);
}

Future<HttpsCallableResult> HttpsCallableReferenceInternal::Call(
    const Variant& data) {
  if (!future_impl_ || !functions_ || !functions_->app() || !obj_) {
    return Future<HttpsCallableResult>();
  }
  auto handle = future_impl_->SafeAlloc<HttpsCallableResult>(
      kHttpsCallableReferenceFnCall);
  JNIEnv* env = functions_->app()->GetJNIEnv();
  jobject java_object = firebase::util::VariantToJavaObject(env, data);
  if (jni::CheckAndClearException(env)) {
    if (java_object) env->DeleteLocalRef(java_object);
    future_impl_->Complete(handle, kErrorInvalidArgument,
                           GetErrorMessage(kErrorInvalidArgument));
    return MakeFuture(future_impl_.get(), handle);
  }
  jobject task = env->CallObjectMethod(
      obj_, callable_reference::GetMethodId(callable_reference::kCallWithParam),
      java_object);
  if (java_object) env->DeleteLocalRef(java_object);
  return DispatchTask(env, handle, task);
}

Future<HttpsCallableResult> HttpsCallableReferenceInternal::CallLastResult() {
  return future_impl_
             ? static_cast<const Future<HttpsCallableResult>&>(
                   future_impl_->LastResult(kHttpsCallableReferenceFnCall))
             : Future<HttpsCallableResult>();
}

}  // namespace firebase::functions::internal
