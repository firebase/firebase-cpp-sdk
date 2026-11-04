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

#ifndef FIREBASE_FUNCTIONS_SRC_DESKTOP_CALLABLE_REFERENCE_DESKTOP_H_
#define FIREBASE_FUNCTIONS_SRC_DESKTOP_CALLABLE_REFERENCE_DESKTOP_H_

#include <functional>
#include <string>
#include <string_view>

#include "app/rest/request.h"
#include "app/rest/response.h"
#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/include/firebase/variant.h"
#include "app/src/reference_counted_future_impl.h"
#include "functions/src/desktop/functions_desktop.h"
#include "functions/src/include/firebase/functions.h"
#include "functions/src/include/firebase/functions/callable_reference.h"
#include "functions/src/include/firebase/functions/callable_result.h"
#include "functions/src/include/firebase/functions/common.h"

namespace firebase::functions::internal {

class FunctionsCallableRequest : public rest::Request {
 public:
  explicit FunctionsCallableRequest(const char* url);
  void set_url(const char* url);
};

class FunctionsCallableResponse : public rest::Response {
 public:
  using Callback = std::function<void()>;

  FunctionsCallableResponse() = default;
  void SetCallback(Callback callback);
  [[nodiscard]] bool failed() const { return failed_; }

  void MarkCompleted() override;
  void MarkFailed() override;

 private:
  void InvokeCallback();
  bool failed_ = false;
  Callback callback_;
};

[[nodiscard]] Error ErrorFromHttpStatus(int status);
[[nodiscard]] Error ErrorFromStatus(std::string_view status);

void ResolveFuture(ReferenceCountedFutureImpl* future_api,
                   SafeFutureHandle<HttpsCallableResult> handle,
                   const FunctionsCallableResponse* response);
void ResolveFuture(ReferenceCountedFutureImpl* future_api,
                   SafeFutureHandle<HttpsCallableResult> handle,
                   rest::Response* response);

class HttpsCallableReferenceInternal {
 public:
  HttpsCallableReferenceInternal(FunctionsInternal* functions, const char* url);
  HttpsCallableReferenceInternal(FunctionsInternal* functions,
                                 const char* target, bool is_url);
  HttpsCallableReferenceInternal(FunctionsInternal* functions, const char* url,
                                 const HttpsCallableOptions& options);
  HttpsCallableReferenceInternal(FunctionsInternal* functions,
                                 const char* target, bool is_url,
                                 const HttpsCallableOptions& options);
  ~HttpsCallableReferenceInternal() = default;

  HttpsCallableReferenceInternal(const HttpsCallableReferenceInternal& other) =
      default;
  HttpsCallableReferenceInternal& operator=(
      const HttpsCallableReferenceInternal& other) = default;
  HttpsCallableReferenceInternal(
      HttpsCallableReferenceInternal&& other) noexcept;
  HttpsCallableReferenceInternal& operator=(
      HttpsCallableReferenceInternal&& other) noexcept;

  [[nodiscard]] Functions* functions() const;
  [[nodiscard]] FunctionsInternal* functions_internal() const {
    return functions_;
  }

  Future<HttpsCallableResult> Call();
  Future<HttpsCallableResult> Call(const Variant& data);
  Future<HttpsCallableResult> CallLastResult();

  [[nodiscard]] bool is_valid() const;
  [[nodiscard]] std::string url() const;
  [[nodiscard]] const HttpsCallableOptions& options() const { return options_; }

  bool operator==(const HttpsCallableReferenceInternal& other) const {
    if (functions_ != other.functions_ || !(options_ == other.options_)) {
      return false;
    }
    return is_url_ == other.is_url_ ? target_ == other.target_
                                    : url() == other.url();
  }

 private:
  ReferenceCountedFutureImpl* future() {
    return functions_ ? functions_->future() : nullptr;
  }

  FunctionsInternal* functions_;
  std::string target_;
  bool is_url_;
  HttpsCallableOptions options_;
};

}  // namespace firebase::functions::internal

#endif  // FIREBASE_FUNCTIONS_SRC_DESKTOP_CALLABLE_REFERENCE_DESKTOP_H_
