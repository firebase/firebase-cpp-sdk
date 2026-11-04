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

#ifndef FIREBASE_FUNCTIONS_SRC_INCLUDE_FIREBASE_FUNCTIONS_CALLABLE_REFERENCE_H_
#define FIREBASE_FUNCTIONS_SRC_INCLUDE_FIREBASE_FUNCTIONS_CALLABLE_REFERENCE_H_

#include <cstdint>

#include "firebase/functions/callable_result.h"
#include "firebase/functions/common.h"
#include "firebase/future.h"
#include "firebase/internal/common.h"

namespace firebase {
namespace functions {

class Functions;

/// @cond FIREBASE_APP_INTERNAL
namespace internal {
class FunctionsInternal;
class HttpsCallableReferenceInternal;
}  // namespace internal
/// @endcond

#if !defined(SWIG)
inline constexpr int64_t kDefaultTimeoutMilliseconds = 70000;
#else
static const int64_t kDefaultTimeoutMilliseconds = 70000;
#endif  // !defined(SWIG)

struct HttpsCallableOptions {
  HttpsCallableOptions()
      : limited_use_app_check_token(false),
        timeout_ms(kDefaultTimeoutMilliseconds) {}
  explicit HttpsCallableOptions(bool limited_use_app_check_token)
      : limited_use_app_check_token(limited_use_app_check_token),
        timeout_ms(kDefaultTimeoutMilliseconds) {}
  HttpsCallableOptions(bool limited_use_app_check_token, int64_t timeout_ms)
      : limited_use_app_check_token(limited_use_app_check_token),
        timeout_ms(timeout_ms) {}

#if !defined(SWIG)
  union {
    bool limited_use_app_check_token;
    bool limited_use_app_check_tokens;
  };
#else
  bool limited_use_app_check_token;
#endif  // !defined(SWIG)

  int64_t timeout_ms;

  bool operator==(const HttpsCallableOptions& other) const {
    return limited_use_app_check_token == other.limited_use_app_check_token &&
           timeout_ms == other.timeout_ms;
  }
  bool operator!=(const HttpsCallableOptions& other) const {
    return !(*this == other);
  }
};

class HttpsCallableReference {
 public:
  HttpsCallableReference() : internal_(nullptr) {}
  HttpsCallableReference(const HttpsCallableReference& reference);
  HttpsCallableReference& operator=(const HttpsCallableReference& reference);
  HttpsCallableReference(HttpsCallableReference&& other) noexcept;
  HttpsCallableReference& operator=(HttpsCallableReference&& other) noexcept;
  ~HttpsCallableReference();

  Functions* functions();
  Functions* functions() const;

  Future<HttpsCallableResult> Call();
  Future<HttpsCallableResult> Call() const;
  Future<HttpsCallableResult> Call(const Variant& data);
  Future<HttpsCallableResult> Call(const Variant& data) const;
  Future<HttpsCallableResult> CallLastResult() const;

  bool is_valid() const;

  bool operator==(const HttpsCallableReference& other) const;
  bool operator!=(const HttpsCallableReference& other) const {
    return !(*this == other);
  }

  /// @cond FIREBASE_APP_INTERNAL
  internal::HttpsCallableReferenceInternal* internal() { return internal_; }
  const internal::HttpsCallableReferenceInternal* internal() const {
    return internal_;
  }
  /// @endcond

 private:
  /// @cond FIREBASE_APP_INTERNAL
  friend class Functions;
  friend class internal::FunctionsInternal;
  friend class internal::HttpsCallableReferenceInternal;

  explicit HttpsCallableReference(
      internal::HttpsCallableReferenceInternal* internal);
  /// @endcond

  internal::HttpsCallableReferenceInternal* internal_;
};

}  // namespace functions
}  // namespace firebase

#endif  // FIREBASE_FUNCTIONS_SRC_INCLUDE_FIREBASE_FUNCTIONS_CALLABLE_REFERENCE_H_
