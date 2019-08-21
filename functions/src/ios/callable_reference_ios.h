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

#ifndef FIREBASE_FUNCTIONS_CLIENT_CPP_SRC_IOS_FUNCTIONS_REFERENCE_IOS_H_
#define FIREBASE_FUNCTIONS_CLIENT_CPP_SRC_IOS_FUNCTIONS_REFERENCE_IOS_H_

#include "app/memory/unique_ptr.h"
#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/reference_counted_future_impl.h"
#include "functions/src/ios/functions_ios.h"

#ifdef __OBJC__
#import "FIRFunctions.h"
@class FIRHTTPSCallableResult;
#endif  // __OBJC__

// This defines the class FIRHTTPSCallablePointer, which is a C++-compatible
// wrapper around the FIRHTTPSCallable Obj-C class.
OBJ_C_PTR_WRAPPER(FIRHTTPSCallable);

namespace firebase {
class Variant;

namespace functions {
namespace internal {

#pragma clang assume_nonnull begin

class HttpsCallableReferenceInternal {
 public:
  explicit HttpsCallableReferenceInternal(FunctionsInternal* functions,
                                          UniquePtr<FIRHTTPSCallablePointer> impl);

  ~HttpsCallableReferenceInternal();

  // Copy constructor. It's totally okay (and efficient) to copy
  // HttpsCallableReferenceInternal instances, as they simply point to the same
  // location.
  HttpsCallableReferenceInternal(const HttpsCallableReferenceInternal& reference);

  // Copy assignment operator. It's totally okay (and efficient) to copy
  // HttpsCallableReferenceInternal instances, as they simply point to the same
  // location.
  HttpsCallableReferenceInternal& operator=(const HttpsCallableReferenceInternal& reference);

#if defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)
  // Move constructor. Moving is an efficient operation for
  // HttpsCallableReferenceInternal instances.
  HttpsCallableReferenceInternal(HttpsCallableReferenceInternal&& other);

  // Move assignment operator. Moving is an efficient operation for
  // HttpsCallableReferenceInternal instances.
  HttpsCallableReferenceInternal& operator=(HttpsCallableReferenceInternal&& other);
#endif  // defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)

  // Gets the functions to which we refer.
  Functions* functions();

  // Returns the result of the call.
  Future<HttpsCallableResult> Call();
  Future<HttpsCallableResult> CallLastResult();
  Future<HttpsCallableResult> Call(const Variant& data);

  // FunctionsInternal instance we are associated with.
  FunctionsInternal* functions_internal() const { return functions_; }

 private:
#ifdef __OBJC__
  static void CompleteFuture(ReferenceCountedFutureImpl* future_impl,
                             SafeFutureHandle<HttpsCallableResult> handle,
                             FIRHTTPSCallableResult* _Nullable result, NSError* error);
#endif  // __OBJC__

  // Get the Future for the HttpsCallableReferenceInternal.
  ReferenceCountedFutureImpl* future();

  // Keep track of the Functions object for managing Futures.
  FunctionsInternal* functions_;

  UniquePtr<FIRHTTPSCallablePointer> impl_;

  Mutex controller_init_mutex_;
};

#pragma clang assume_nonnull end

}  // namespace internal
}  // namespace functions
}  // namespace firebase

#endif  // FIREBASE_FUNCTIONS_CLIENT_CPP_SRC_IOS_FUNCTIONS_REFERENCE_IOS_H_
