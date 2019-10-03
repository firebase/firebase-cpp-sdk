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

#ifndef FIREBASE_FUNCTIONS_CLIENT_CPP_SRC_ANDROID_CALLABLE_REFERENCE_ANDROID_H_
#define FIREBASE_FUNCTIONS_CLIENT_CPP_SRC_ANDROID_CALLABLE_REFERENCE_ANDROID_H_

#include <jni.h>

#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/reference_counted_future_impl.h"
#include "app/src/util_android.h"
#include "functions/src/android/functions_android.h"
#include "functions/src/include/firebase/functions/callable_reference.h"

namespace firebase {
class HttpsCallableResult;

namespace functions {
namespace internal {

class HttpsCallableReferenceInternal {
 public:
  // CallableReferenceInternal will create its own global reference to ref_obj,
  // so you should delete the object you passed in after creating the
  // CallableReferenceInternal instance.
  HttpsCallableReferenceInternal(FunctionsInternal* functions, jobject ref_obj);

  HttpsCallableReferenceInternal(const HttpsCallableReferenceInternal& ref);

  HttpsCallableReferenceInternal& operator=(
      const HttpsCallableReferenceInternal& ref);

#if defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)
  HttpsCallableReferenceInternal(HttpsCallableReferenceInternal&& ref);

  HttpsCallableReferenceInternal& operator=(
      HttpsCallableReferenceInternal&& ref);
#endif  // defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)

  ~HttpsCallableReferenceInternal();

  // Gets the functions to which we refer.
  Functions* functions();

  // Asynchronously calls this CallableReferenceInternal.
  Future<HttpsCallableResult> Call();
  Future<HttpsCallableResult> CallLastResult();
  Future<HttpsCallableResult> Call(const Variant& data);

  // Initialize JNI bindings for this class.
  static bool Initialize(App* app);
  static void Terminate(App* app);

  // FunctionsInternal instance we are associated with.
  FunctionsInternal* functions_internal() const { return functions_; }

 private:
  static void FutureCallback(JNIEnv* env, jobject result,
                             util::FutureResult result_code,
                             const char* status_message, void* callback_data);

  ReferenceCountedFutureImpl* future();

  FunctionsInternal* functions_;
  jobject obj_;
};

}  // namespace internal
}  // namespace functions
}  // namespace firebase

#endif  // FIREBASE_FUNCTIONS_CLIENT_CPP_SRC_ANDROID_CALLABLE_REFERENCE_ANDROID_H_
