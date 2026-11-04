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
    : functions_(functions), future_impl_(kHttpsCallableReferenceFnCount), impl_(std::move(impl)) {
  if (functions_) {
    functions_->  }
}

HttpsCallableReferenceInternal::~HttpsCallableReferenceInternal() {
  if (functions_) {
    functions_->  }
}

HttpsCallableReferenceInternal::HttpsCallableReferenceInternal(
    const HttpsCallableReferenceInternal& other)
    : functions_(other.functions_), future_impl_(kHttpsCallableReferenceFnCount),
      impl_(std::make_unique<FIRHTTPSCallablePointer>(other.impl())) {
  if (functions_) {
    functions_->  }
}

HttpsCallableReferenceInternal& HttpsCallableReferenceInternal::operator=(
    const HttpsCallableReferenceInternal& other) {
  if (this == &other) {
    return *this;
  }
  auto new_impl = std::make_unique<FIRHTTPSCallablePointer>(other.impl());
  if (functions_ != other.functions_) {
    if (functions_) {
      functions_->    }
    functions_ = other.functions_;
      }
  impl_ = std::move(new_impl);
  return *this;
}

HttpsCallableReferenceInternal::HttpsCallableReferenceInternal(
    HttpsCallableReferenceInternal&& other) noexcept
    : functions_(std::exchange(other.functions_, nullptr)), future_impl_(kHttpsCallableReferenceFnCount),
      impl_(std::move(other.impl_)) {
  }

HttpsCallableReferenceInternal& HttpsCallableReferenceInternal::operator=(
    HttpsCallableReferenceInternal&& other) noexcept {
  if (this == &other) {
    return *this;
  }
  if (functions_) {
    functions_->  }
  functions_ = std::exchange(other.functions_, nullptr);
  impl_ = std::move(other.impl_);
    return *this;
}

Functions* HttpsCallableReferenceInternal::functions() const {
  if (!functions_) return nullptr;
  return Functions::GetInstance(functions_->app(), functions_->region());
}

bool HttpsCallableReferenceInternal::is_valid() const {
  return functions_ != nullptr && impl() != nil;
}

bool HttpsCallableReferenceInternal::operator==(
    const HttpsCallableReferenceInternal& other) const {
  if (functions_ != other.functions_) return false;
  if (impl() == other.impl()) return true;
  if (impl() == nil || other.impl() == nil) return false;
  return [impl() isEqual:other.impl()] == YES;
}

namespace {
void CompleteFromCompletionBlock(
    const std::shared_ptr<FunctionsLifetimeState>& functions_lifetime,
    ReferenceCountedFutureImpl* future_impl,
    SafeFutureHandle<HttpsCallableResult> handle,
    FIRHTTPSCallableResult* result, NSError* error) {
  if (!functions_lifetime || !future_impl) return;
  MutexLock fn_lock(functions_lifetime->mutex);
  if (!functions_lifetime->alive) return;

  if (error) {
    Error code = FunctionsInternal::ErrorFromNSError(error);
    const char* error_desc = error.localizedDescription.UTF8String;
    if (!error_desc || error_desc[0] == '\0') {
      error_desc = GetErrorMessage(code);
    }
    Variant details;
    id raw_details = error.userInfo[@"details"];
    if (raw_details) {
      details = util::IdToVariant(raw_details);
    }
    HttpsCallableResult callable_result(std::move(details));
    future_impl->CompleteWithResult(handle, code, error_desc, callable_result);
  } else {
    Variant data = result ? util::IdToVariant(result.data) : Variant::Null();
    HttpsCallableResult callable_result(std::move(data));
    future_impl->CompleteWithResult(handle, kErrorNone, "", callable_result);
  }
}
}  // namespace

Future<HttpsCallableResult> HttpsCallableReferenceInternal::Call() {
  ReferenceCountedFutureImpl* future_impl = future();
  if (!future_impl || !functions_ || impl() == nil) {
    return Future<HttpsCallableResult>();
  }
  SafeFutureHandle<HttpsCallableResult> handle =
      future_impl->SafeAlloc<HttpsCallableResult>(kHttpsCallableReferenceFnCall);
  std::shared_ptr<FunctionsLifetimeState> fn_lifetime =
      functions_->lifetime_state();
  @autoreleasepool {
    [impl() callWithCompletion:^(FIRHTTPSCallableResult* _Nullable result,
                                 NSError* _Nullable error) {
      CompleteFromCompletionBlock(fn_lifetime, future_impl, handle, result,
                                  error);
    }];
  }
  return MakeFuture(future_impl, handle);
}

Future<HttpsCallableResult> HttpsCallableReferenceInternal::Call(
    const Variant& data) {
  ReferenceCountedFutureImpl* future_impl = future();
  if (!future_impl || !functions_ || impl() == nil) {
    return Future<HttpsCallableResult>();
  }
  SafeFutureHandle<HttpsCallableResult> handle =
      future_impl->SafeAlloc<HttpsCallableResult>(kHttpsCallableReferenceFnCall);
  std::shared_ptr<FunctionsLifetimeState> fn_lifetime =
      functions_->lifetime_state();
  @autoreleasepool {
    id objc_data = util::VariantToId(data);
    [impl() callWithObject:objc_data
                completion:^(FIRHTTPSCallableResult* _Nullable result,
                             NSError* _Nullable error) {
                  CompleteFromCompletionBlock(fn_lifetime, future_impl, handle,
                                              result, error);
                }];
  }
  return MakeFuture(future_impl, handle);
}

Future<HttpsCallableResult> HttpsCallableReferenceInternal::CallLastResult() {
  ReferenceCountedFutureImpl* future_api = future();
  return future_api ? static_cast<const Future<HttpsCallableResult>&>(
                          future_api->LastResult(kHttpsCallableReferenceFnCall))
                    : Future<HttpsCallableResult>();
}


}  // namespace firebase::functions::internal
