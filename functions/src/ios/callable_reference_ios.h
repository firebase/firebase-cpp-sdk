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

#ifndef FIREBASE_FUNCTIONS_SRC_IOS_CALLABLE_REFERENCE_IOS_H_
#define FIREBASE_FUNCTIONS_SRC_IOS_CALLABLE_REFERENCE_IOS_H_

#include <memory>

#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/include/firebase/variant.h"
#include "app/src/reference_counted_future_impl.h"
#include "app/src/util_ios.h"
#include "functions/src/include/firebase/functions.h"
#include "functions/src/include/firebase/functions/callable_result.h"
#include "functions/src/ios/functions_ios.h"

#ifdef __OBJC__
#import "FirebaseFunctions-Swift.h"
#endif  // __OBJC__

namespace firebase {
namespace functions {
namespace internal {

// This defines the class FIRHTTPSCallablePointer, which is a C++-compatible
// wrapper around the FIRHTTPSCallable Obj-C class.
OBJ_C_PTR_WRAPPER(FIRHTTPSCallable);

class HttpsCallableReferenceInternal {
 public:
  HttpsCallableReferenceInternal(FunctionsInternal* functions,
                                 std::unique_ptr<FIRHTTPSCallablePointer> impl);
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

#if defined(FIREBASE_USE_MOVE_OPERATORS)
  // Move constructor. Moving is an efficient operation for
  // HttpsCallableReferenceInternal instances.
  HttpsCallableReferenceInternal(
      HttpsCallableReferenceInternal&& other) noexcept;

  // Move assignment operator. Moving is an efficient operation for
  // HttpsCallableReferenceInternal instances.
  HttpsCallableReferenceInternal& operator=(
      HttpsCallableReferenceInternal&& other) noexcept;
#endif  // defined(FIREBASE_USE_MOVE_OPERATORS)

  // Gets the Functions instance to which we refer.
  Functions* functions() const;

  FunctionsInternal* functions_internal() const { return functions_; }

  // Asynchronously calls the trigger endpoint with no parameters.
  Future<HttpsCallableResult> Call();

  // Asynchronously calls the trigger endpoint with the given parameter.
  Future<HttpsCallableResult> Call(const Variant& data);

  // Get the result of the most recent Call() call.
  Future<HttpsCallableResult> CallLastResult();

  // Returns true if this HttpsCallableReference is valid, false if it is not
  // valid.
  bool is_valid() const;

  bool operator==(const HttpsCallableReferenceInternal& other) const;

 private:
#ifdef __OBJC__
  FIRHTTPSCallable* impl() const { return impl_ ? impl_->get() : nil; }
#endif  // __OBJC__

  // Get the Future for the HttpsCallableReferenceInternal.
  ReferenceCountedFutureImpl* future();

  // The FunctionsInternal object we are part of.
  FunctionsInternal* functions_;

  std::unique_ptr<FIRHTTPSCallablePointer> impl_;
};

}  // namespace internal
}  // namespace functions
}  // namespace firebase

#endif  // FIREBASE_FUNCTIONS_SRC_IOS_CALLABLE_REFERENCE_IOS_H_
