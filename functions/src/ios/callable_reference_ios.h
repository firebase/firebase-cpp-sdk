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
@class FIRHTTPSCallable;
#endif  // __OBJC__

namespace firebase::functions::internal {

OBJ_C_PTR_WRAPPER(FIRHTTPSCallable);

class HttpsCallableReferenceInternal {
 public:
  HttpsCallableReferenceInternal(FunctionsInternal* functions,
                                 std::unique_ptr<FIRHTTPSCallablePointer> impl);
  ~HttpsCallableReferenceInternal() = default;

  HttpsCallableReferenceInternal(const HttpsCallableReferenceInternal& other);
  HttpsCallableReferenceInternal& operator=(
      const HttpsCallableReferenceInternal& other);
  HttpsCallableReferenceInternal(
      HttpsCallableReferenceInternal&& other) noexcept;
  HttpsCallableReferenceInternal& operator=(
      HttpsCallableReferenceInternal&& other) noexcept;

  [[nodiscard]] Functions* functions() const;
  [[nodiscard]] FunctionsInternal* functions_internal() const {
    if (!functions_ || !functions_lifetime_) return nullptr;
    MutexLock fn_lock(functions_lifetime_->mutex);
    return functions_lifetime_->alive ? functions_ : nullptr;
  }

  Future<HttpsCallableResult> Call();
  Future<HttpsCallableResult> Call(const Variant& data);
  Future<HttpsCallableResult> CallLastResult();

  [[nodiscard]] bool is_valid() const;
  bool operator==(const HttpsCallableReferenceInternal& other) const;

 private:
#ifdef __OBJC__
  FIRHTTPSCallable* impl() const { return impl_ ? impl_->get() : nil; }
#endif  // __OBJC__

  ReferenceCountedFutureImpl* future() { return future_impl_.get(); }

  FunctionsInternal* functions_ = nullptr;
  std::shared_ptr<FunctionsLifetimeState> functions_lifetime_;
  std::shared_ptr<ReferenceCountedFutureImpl> future_impl_;
  std::unique_ptr<FIRHTTPSCallablePointer> impl_;
};

}  // namespace firebase::functions::internal

#endif  // FIREBASE_FUNCTIONS_SRC_IOS_CALLABLE_REFERENCE_IOS_H_
