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

#include "functions/src/ios/callable_reference_ios.h"

#import "FIRFunctions.h"
#import "FIRHTTPSCallable.h"
#include "app/src/util_ios.h"
#include "functions/src/include/firebase/functions.h"
#include "functions/src/include/firebase/functions/common.h"
#include "functions/src/ios/functions_ios.h"

namespace firebase {
namespace functions {
namespace internal {

Error NSErrorToErrorCode(NSError* error) {
  if (!error) return kErrorNone;
  // The error codes just standard gRPC error codes, so make sure it is one.
  switch (error.code) {
    case kErrorNone:
    case kErrorCancelled:
    case kErrorUnknown:
    case kErrorInvalidArgument:
    case kErrorDeadlineExceeded:
    case kErrorNotFound:
    case kErrorAlreadyExists:
    case kErrorPermissionDenied:
    case kErrorUnauthenticated:
    case kErrorResourceExhausted:
    case kErrorFailedPrecondition:
    case kErrorAborted:
    case kErrorOutOfRange:
    case kErrorUnimplemented:
    case kErrorInternal:
    case kErrorUnavailable:
    case kErrorDataLoss:
      return static_cast<Error>(error.code);
  }
  // Default to INTERNAL if it's an unkown error code.
  return kErrorInternal;
}

enum CallableReferenceFn {
  kCallableReferenceFnCall = 0,
  kCallableReferenceFnCount,
};

HttpsCallableReferenceInternal::HttpsCallableReferenceInternal(
    FunctionsInternal* functions, UniquePtr<FIRHTTPSCallablePointer> impl)
    : functions_(functions), impl_(std::move(impl)) {
  // impl_ was newed by the caller, this class will take ownership and delete it in the destructor.
  functions_->future_manager().AllocFutureApi(this, kCallableReferenceFnCount);
}

HttpsCallableReferenceInternal::~HttpsCallableReferenceInternal() {
  functions_->future_manager().ReleaseFutureApi(this);
}

HttpsCallableReferenceInternal::HttpsCallableReferenceInternal(
    const HttpsCallableReferenceInternal& other)
    : functions_(other.functions_) {
  impl_.reset(new FIRHTTPSCallablePointer(*other.impl_));
  functions_->future_manager().AllocFutureApi(this, kCallableReferenceFnCount);
}

HttpsCallableReferenceInternal& HttpsCallableReferenceInternal::operator=(
    const HttpsCallableReferenceInternal& other) {
  functions_ = other.functions_;
  impl_.reset(new FIRHTTPSCallablePointer(*other.impl_));
  return *this;
}

#if defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)
HttpsCallableReferenceInternal::HttpsCallableReferenceInternal(
    HttpsCallableReferenceInternal&& other)
    : functions_(other.functions_), impl_(std::move(other.impl_)) {
  other.functions_ = nullptr;
  other.impl_.reset(new FIRHTTPSCallablePointer(nil));
  functions_->future_manager().MoveFutureApi(&other, this);
}

HttpsCallableReferenceInternal& HttpsCallableReferenceInternal::operator=(
    HttpsCallableReferenceInternal&& other) {
  functions_ = other.functions_;
  impl_ = std::move(other.impl_);
  other.functions_ = nullptr;
  other.impl_.reset(new FIRHTTPSCallablePointer(nil));
  functions_->future_manager().MoveFutureApi(&other, this);
  return *this;
}
#endif  // defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)

Functions* HttpsCallableReferenceInternal::functions() {
  return Functions::GetInstance(functions_->app());
}

/* static */
void HttpsCallableReferenceInternal::CompleteFuture(ReferenceCountedFutureImpl* future_impl,
                                                    SafeFutureHandle<HttpsCallableResult> handle,
                                                    FIRHTTPSCallableResult* _Nullable result,
                                                    NSError* error) {
  Error error_code = NSErrorToErrorCode(error);
  const char* error_string = GetErrorMessage(error_code);
  if (error.localizedDescription) {
    error_string = error.localizedDescription.UTF8String;
  }
  if (result) {
    Variant result_data = util::IdToVariant(result.data);
    HttpsCallableResult callable_result(std::move(result_data));
    future_impl->CompleteWithResult(handle, error_code, error_string, callable_result);
  } else {
    future_impl->Complete(handle, error_code, error_string);
  }
}

Future<HttpsCallableResult> HttpsCallableReferenceInternal::Call() {
  ReferenceCountedFutureImpl* future_impl = future();
  HttpsCallableResult null_result(Variant::Null());
  SafeFutureHandle<HttpsCallableResult> handle =
      future_impl->SafeAlloc(kCallableReferenceFnCall, null_result);
  void (^completion)(FIRHTTPSCallableResult* _Nullable, NSError* _Nullable) =
      ^(FIRHTTPSCallableResult* _Nullable result, NSError* error) {
        CompleteFuture(future_impl, handle, result, error);
      };

  [impl_.get()->get() callWithCompletion:completion];
  return CallLastResult();
}

Future<HttpsCallableResult> HttpsCallableReferenceInternal::Call(const Variant& variant) {
  id data = util::VariantToId(variant);

  ReferenceCountedFutureImpl* future_impl = future();
  HttpsCallableResult null_result(Variant::Null());
  SafeFutureHandle<HttpsCallableResult> handle =
      future_impl->SafeAlloc(kCallableReferenceFnCall, null_result);
  void (^completion)(FIRHTTPSCallableResult* _Nullable, NSError* _Nullable) =
      ^(FIRHTTPSCallableResult* _Nullable result, NSError* error) {
        CompleteFuture(future_impl, handle, result, error);
      };

  [impl_.get()->get() callWithObject:data completion:completion];
  return CallLastResult();
}

Future<HttpsCallableResult> HttpsCallableReferenceInternal::CallLastResult() {
  return static_cast<const Future<HttpsCallableResult>&>(
      future()->LastResult(kCallableReferenceFnCall));
}

ReferenceCountedFutureImpl* HttpsCallableReferenceInternal::future() {
  return functions_->future_manager().GetFutureApi(this);
}

}  // namespace internal
}  // namespace functions
}  // namespace firebase
