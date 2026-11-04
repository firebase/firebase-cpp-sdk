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

#include "functions/src/ios/callable_reference_ios.h"

#import "FirebaseFunctions-Swift.h"

#include <memory>
#include <utility>

#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/include/firebase/variant.h"
#include "app/src/reference_counted_future_impl.h"
#include "app/src/util_ios.h"
#include "functions/src/include/firebase/functions.h"
#include "functions/src/include/firebase/functions/callable_result.h"
#include "functions/src/include/firebase/functions/common.h"
#include "functions/src/ios/functions_ios.h"

namespace firebase::functions::internal {

enum HttpsCallableReferenceFn {
  kHttpsCallableReferenceFnCall = 0,
  kHttpsCallableReferenceFnCount,
};

HttpsCallableReferenceInternal::HttpsCallableReferenceInternal(
    FunctionsInternal* functions, std::unique_ptr<FIRHTTPSCallablePointer> impl)
    : functions_(functions),
      functions_lifetime_(functions_ ? functions_->lifetime_state() : nullptr),
      future_impl_(std::make_shared<ReferenceCountedFutureImpl>(
          kHttpsCallableReferenceFnCount)),
      impl_(std::move(impl)) {}

HttpsCallableReferenceInternal::HttpsCallableReferenceInternal(
    const HttpsCallableReferenceInternal& other)
    : HttpsCallableReferenceInternal(
          other.functions_,
          std::make_unique<FIRHTTPSCallablePointer>(other.impl())) {}

HttpsCallableReferenceInternal& HttpsCallableReferenceInternal::operator=(
    const HttpsCallableReferenceInternal& other) {
  if (this != &other) {
    functions_ = other.functions_;
    functions_lifetime_ = other.functions_lifetime_;
    future_impl_ = std::make_shared<ReferenceCountedFutureImpl>(
        kHttpsCallableReferenceFnCount);
    impl_ = std::make_unique<FIRHTTPSCallablePointer>(other.impl());
  }
  return *this;
}

HttpsCallableReferenceInternal::HttpsCallableReferenceInternal(
    HttpsCallableReferenceInternal&& other) noexcept
    : functions_(std::exchange(other.functions_, nullptr)),
      functions_lifetime_(std::move(other.functions_lifetime_)),
      future_impl_(std::move(other.future_impl_)),
      impl_(std::move(other.impl_)) {}

HttpsCallableReferenceInternal& HttpsCallableReferenceInternal::operator=(
    HttpsCallableReferenceInternal&& other) noexcept {
  if (this != &other) {
    functions_ = std::exchange(other.functions_, nullptr);
    functions_lifetime_ = std::move(other.functions_lifetime_);
    future_impl_ = std::move(other.future_impl_);
    impl_ = std::move(other.impl_);
  }
  return *this;
}

Functions* HttpsCallableReferenceInternal::functions() const {
  FunctionsInternal* fn = functions_internal();
  return fn ? Functions::GetInstance(fn->app(), fn->region()) : nullptr;
}

bool HttpsCallableReferenceInternal::is_valid() const {
  return impl() != nil && functions_internal() != nullptr;
}

bool HttpsCallableReferenceInternal::operator==(
    const HttpsCallableReferenceInternal& other) const {
  if (functions_ != other.functions_) return false;
  if (impl() == other.impl()) return true;
  return impl() != nil && other.impl() != nil &&
         [impl() isEqual:other.impl()] == YES;
}

namespace {
void CompleteFromCompletionBlock(
    const std::shared_ptr<FunctionsLifetimeState>& functions_lifetime,
    const std::shared_ptr<ReferenceCountedFutureImpl>& future_impl,
    SafeFutureHandle<HttpsCallableResult> handle,
    FIRHTTPSCallableResult* result, NSError* error) {
  if (!functions_lifetime || !future_impl) return;
  MutexLock fn_lock(functions_lifetime->mutex);
  if (!functions_lifetime->alive) return;

  if (error) {
    Error code = FunctionsInternal::ErrorFromNSError(error);
    const char* error_desc = error.localizedDescription.UTF8String;
    if (!error_desc || error_desc[0] == '\0') error_desc = GetErrorMessage(code);
    id raw_details = error.userInfo[@"details"];
    Variant details = raw_details ? util::IdToVariant(raw_details) : Variant();
    future_impl->CompleteWithResult(
        handle, code, error_desc, HttpsCallableResult(std::move(details)));
  } else {
    Variant data = result ? util::IdToVariant(result.data) : Variant::Null();
    future_impl->CompleteWithResult(handle, kErrorNone, "",
                                    HttpsCallableResult(std::move(data)));
  }
}
}  // namespace

Future<HttpsCallableResult> HttpsCallableReferenceInternal::Call() {
  if (!future_impl_ || !is_valid()) return Future<HttpsCallableResult>();
  auto handle = future_impl_->SafeAlloc<HttpsCallableResult>(
      kHttpsCallableReferenceFnCall);
  auto fn_lifetime = functions_lifetime_;
  auto future_impl = future_impl_;
  @autoreleasepool {
    [impl() callWithCompletion:^(FIRHTTPSCallableResult* _Nullable result,
                                 NSError* _Nullable error) {
      CompleteFromCompletionBlock(fn_lifetime, future_impl, handle, result,
                                  error);
    }];
  }
  return MakeFuture(future_impl_.get(), handle);
}

Future<HttpsCallableResult> HttpsCallableReferenceInternal::Call(
    const Variant& data) {
  if (!future_impl_ || !is_valid()) return Future<HttpsCallableResult>();
  auto handle = future_impl_->SafeAlloc<HttpsCallableResult>(
      kHttpsCallableReferenceFnCall);
  auto fn_lifetime = functions_lifetime_;
  auto future_impl = future_impl_;
  @autoreleasepool {
    [impl() callWithObject:util::VariantToId(data)
                completion:^(FIRHTTPSCallableResult* _Nullable result,
                             NSError* _Nullable error) {
                  CompleteFromCompletionBlock(fn_lifetime, future_impl, handle,
                                              result, error);
                }];
  }
  return MakeFuture(future_impl_.get(), handle);
}

Future<HttpsCallableResult> HttpsCallableReferenceInternal::CallLastResult() {
  return future_impl_
             ? static_cast<const Future<HttpsCallableResult>&>(
                   future_impl_->LastResult(kHttpsCallableReferenceFnCall))
             : Future<HttpsCallableResult>();
}

}  // namespace firebase::functions::internal
