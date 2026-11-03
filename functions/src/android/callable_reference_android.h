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

#ifndef FIREBASE_FUNCTIONS_SRC_ANDROID_CALLABLE_REFERENCE_ANDROID_H_
#define FIREBASE_FUNCTIONS_SRC_ANDROID_CALLABLE_REFERENCE_ANDROID_H_

#include <jni.h>

#include <memory>

#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/include/firebase/variant.h"
#include "app/src/reference_counted_future_impl.h"
#include "app/src/util_android.h"
#include "functions/src/android/functions_android.h"
#include "functions/src/include/firebase/functions.h"

namespace firebase {
namespace functions {

class HttpsCallableResult;

namespace internal {

// clang-format off
#define CALLABLE_REFERENCE_METHODS(X)                                          \
  X(Call, "call", "()Lcom/google/android/gms/tasks/Task;"),                    \
  X(CallWithParam, "call",                                                     \
    "(Ljava/lang/Object;)Lcom/google/android/gms/tasks/Task;"),                \
  X(SetTimeout, "setTimeout", "(JLjava/util/concurrent/TimeUnit;)V",           \
    util::kMethodTypeInstance, util::kMethodOptional)
// clang-format on

METHOD_LOOKUP_DECLARATION(callable_reference, CALLABLE_REFERENCE_METHODS)

// clang-format off
#define CALLABLE_RESULT_METHODS(X)                                             \
  X(GetData, "getData", "()Ljava/lang/Object;")
// clang-format on

METHOD_LOOKUP_DECLARATION(callable_result, CALLABLE_RESULT_METHODS)

struct CallableReferenceAndroidLifetimeState {
  Mutex mutex;
  bool alive = true;
};

class HttpsCallableReferenceInternal {
 public:
  HttpsCallableReferenceInternal(FunctionsInternal* functions,
                                 jobject wrapper_obj);
  ~HttpsCallableReferenceInternal();

  // Copy constructor. It's totally okay (and efficient) to copy
  // HttpsCallableReferenceInternal instances, as they simply point to the same
  // location in the Cloud Functions service.
  HttpsCallableReferenceInternal(const HttpsCallableReferenceInternal& other);

  // Copy assignment operator. It's totally okay (and efficient) to copy
  // HttpsCallableReferenceInternal instances, as they simply point to the same
  // location in the Cloud Functions service.
  HttpsCallableReferenceInternal& operator=(
      const HttpsCallableReferenceInternal& other);

  // Move constructor. Moving is an efficient operation for
  // HttpsCallableReferenceInternal instances.
  HttpsCallableReferenceInternal(
      HttpsCallableReferenceInternal&& other) noexcept;

  // Move assignment operator. Moving is an efficient operation for
  // HttpsCallableReferenceInternal instances.
  HttpsCallableReferenceInternal& operator=(
      HttpsCallableReferenceInternal&& other) noexcept;

  // Gets the Functions instance to which we refer.
  [[nodiscard]] Functions* functions() const;

  [[nodiscard]] FunctionsInternal* functions_internal() const {
    return functions_;
  }

  // Asynchronously calls the trigger endpoint with no parameters.
  Future<HttpsCallableResult> Call();

  // Asynchronously calls the trigger endpoint with the given parameter.
  Future<HttpsCallableResult> Call(const Variant& data);

  // Get the result of the most recent Call() call.
  Future<HttpsCallableResult> CallLastResult();

  // Returns true if this HttpsCallableReference is valid, false if it is not
  // valid.
  [[nodiscard]] bool is_valid() const;

  bool operator==(const HttpsCallableReferenceInternal& other) const;

 private:
  static void FutureCallback(JNIEnv* env, jobject result,
                             util::FutureResult result_code,
                             const char* status_message, void* callback_data);

  void Cleanup(FunctionsInternal* fallback_functions = nullptr) noexcept;

  // Get the Future for the HttpsCallableReferenceInternal.
  ReferenceCountedFutureImpl* future();

  // The FunctionsInternal object we are part of.
  FunctionsInternal* functions_ = nullptr;

  // The Java HttpsCallableReference instance.
  jobject obj_ = nullptr;

  // Shared lifetime state guarding asynchronous JNI completion callbacks.
  std::shared_ptr<CallableReferenceAndroidLifetimeState> lifetime_state_;
};

}  // namespace internal
}  // namespace functions
}  // namespace firebase

#endif  // FIREBASE_FUNCTIONS_SRC_ANDROID_CALLABLE_REFERENCE_ANDROID_H_
