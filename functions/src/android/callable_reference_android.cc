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

#include <string>
#include <utility>

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

namespace firebase {
namespace functions {
namespace internal {

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

HttpsCallableReferenceInternal::HttpsCallableReferenceInternal(
    FunctionsInternal* functions, jobject wrapper_obj)
    : functions_(functions), obj_(nullptr) {
  if (functions_ && functions_->app()) {
    functions_->future_manager().AllocFutureApi(this,
                                                kHttpsCallableReferenceFnCount);
    if (wrapper_obj) {
      obj_ = functions_->app()->GetJNIEnv()->NewGlobalRef(wrapper_obj);
    }
  }
}

HttpsCallableReferenceInternal::~HttpsCallableReferenceInternal() {
  if (obj_ != nullptr && functions_ != nullptr &&
      functions_->app() != nullptr) {
    functions_->app()->GetJNIEnv()->DeleteGlobalRef(obj_);
    obj_ = nullptr;
  }
  if (functions_ != nullptr) {
    functions_->future_manager().ReleaseFutureApi(this);
    functions_ = nullptr;
  }
}

HttpsCallableReferenceInternal::HttpsCallableReferenceInternal(
    const HttpsCallableReferenceInternal& src)
    : functions_(src.functions_), obj_(nullptr) {
  if (functions_ && functions_->app()) {
    functions_->future_manager().AllocFutureApi(this,
                                                kHttpsCallableReferenceFnCount);
    if (src.obj_ != nullptr) {
      obj_ = functions_->app()->GetJNIEnv()->NewGlobalRef(src.obj_);
    }
  }
}

HttpsCallableReferenceInternal& HttpsCallableReferenceInternal::operator=(
    const HttpsCallableReferenceInternal& src) {
  if (this == &src) {
    return *this;
  }
  if (obj_ != nullptr && functions_ != nullptr &&
      functions_->app() != nullptr) {
    functions_->app()->GetJNIEnv()->DeleteGlobalRef(obj_);
    obj_ = nullptr;
  }
  if (functions_ != src.functions_) {
    if (functions_) {
      functions_->future_manager().ReleaseFutureApi(this);
    }
    functions_ = src.functions_;
    if (functions_) {
      functions_->future_manager().AllocFutureApi(
          this, kHttpsCallableReferenceFnCount);
    }
  }
  if (src.obj_ != nullptr && functions_ != nullptr &&
      functions_->app() != nullptr) {
    obj_ = functions_->app()->GetJNIEnv()->NewGlobalRef(src.obj_);
  }
  return *this;
}

#if defined(FIREBASE_USE_MOVE_OPERATORS)
HttpsCallableReferenceInternal::HttpsCallableReferenceInternal(
    HttpsCallableReferenceInternal&& src) noexcept
    : functions_(src.functions_), obj_(src.obj_) {
  src.functions_ = nullptr;
  src.obj_ = nullptr;
  if (functions_) {
    functions_->future_manager().MoveFutureApi(&src, this);
  }
}

HttpsCallableReferenceInternal& HttpsCallableReferenceInternal::operator=(
    HttpsCallableReferenceInternal&& src) noexcept {
  if (this == &src) {
    return *this;
  }
  if (obj_ != nullptr && functions_ != nullptr &&
      functions_->app() != nullptr) {
    functions_->app()->GetJNIEnv()->DeleteGlobalRef(obj_);
    obj_ = nullptr;
  }
  if (functions_) {
    functions_->future_manager().ReleaseFutureApi(this);
  }
  functions_ = src.functions_;
  obj_ = src.obj_;
  src.functions_ = nullptr;
  src.obj_ = nullptr;
  if (functions_) {
    functions_->future_manager().MoveFutureApi(&src, this);
  }
  return *this;
}
#endif  // defined(FIREBASE_USE_MOVE_OPERATORS)

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

struct FutureCallbackData {
  FutureCallbackData(SafeFutureHandle<HttpsCallableResult> handle_,
                     ReferenceCountedFutureImpl* impl_,
                     FunctionsInternal* functions_)
      : handle(handle_), future_impl(impl_), functions(functions_) {}
  SafeFutureHandle<HttpsCallableResult> handle;
  ReferenceCountedFutureImpl* future_impl;
  FunctionsInternal* functions;
};

}  // namespace

void HttpsCallableReferenceInternal::FutureCallback(
    JNIEnv* env, jobject result, util::FutureResult result_code,
    const char* status_message, void* callback_data) {
  FutureCallbackData* data = static_cast<FutureCallbackData*>(callback_data);
  if (!data) return;

  if (data->future_impl != nullptr) {
    if (result_code == util::kFutureResultCancelled) {
      data->future_impl->Complete(data->handle, kErrorCancelled,
                                  GetErrorMessage(kErrorCancelled));
    } else if (result_code == util::kFutureResultFailure) {
      std::string error_msg = status_message ? status_message : "";
      Variant details;
      Error code = kErrorInternal;
      if (data->functions != nullptr) {
        code = data->functions->ErrorFromJavaFunctionsException(
            env, result, &error_msg, &details);
      }
      if (error_msg.empty()) {
        error_msg = GetErrorMessage(code);
      }
      HttpsCallableResult callable_result(std::move(details));
      data->future_impl->CompleteWithResult(data->handle, code,
                                            error_msg.c_str(), callable_result);
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
  delete data;
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

  FutureCallbackData* callback_data =
      new FutureCallbackData(handle, future_api, functions_);
  util::RegisterCallbackOnTask(env, task, FutureCallback, callback_data,
                               "Functions");
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

  FutureCallbackData* callback_data =
      new FutureCallbackData(handle, future_api, functions_);
  util::RegisterCallbackOnTask(env, task, FutureCallback, callback_data,
                               "Functions");
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

ReferenceCountedFutureImpl* HttpsCallableReferenceInternal::future() {
  return functions_ ? functions_->future_manager().GetFutureApi(this) : nullptr;
}

}  // namespace internal
}  // namespace functions
}  // namespace firebase
