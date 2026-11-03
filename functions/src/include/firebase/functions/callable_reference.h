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

/// Default timeout for callable invocations in milliseconds (70 seconds).
inline constexpr int64_t kDefaultTimeoutMilliseconds = 70000;

/// Represents the options for configuring the HttpsCallableReference.
struct HttpsCallableOptions {
  /// Default Constructor.
  HttpsCallableOptions() = default;

  /// Constructor with limited_use_app_check_tokens.
  explicit HttpsCallableOptions(bool limited_use_app_check_tokens)
      : limited_use_app_check_tokens(limited_use_app_check_tokens) {}

  /// Constructor with limited_use_app_check_tokens and timeout in milliseconds.
  HttpsCallableOptions(bool limited_use_app_check_tokens, int64_t timeout_ms)
      : limited_use_app_check_tokens(limited_use_app_check_tokens),
        timeout_ms(timeout_ms) {}

  /// Decides whether or not to use limited-use App Check tokens when invoking
  /// the associated function. Notice that the jeopardy of using single-use
  /// tokens is that it lacks replay protection. By default, this is false.
  bool limited_use_app_check_tokens = false;

  /// Timeout in milliseconds for invoking the function. Defaults to 70000ms.
  int64_t timeout_ms = kDefaultTimeoutMilliseconds;

  bool operator==(const HttpsCallableOptions& other) const {
    return limited_use_app_check_tokens == other.limited_use_app_check_tokens &&
           timeout_ms == other.timeout_ms;
  }

  bool operator!=(const HttpsCallableOptions& other) const {
    return !(*this == other);
  }
};

/// Represents a reference to a Cloud Functions object.
/// Developers can call the Cloud Task once they acquire a Reference to it.
class HttpsCallableReference {
 public:
  /// @brief Default constructor. This creates an invalid
  /// HttpsCallableReference. Attempting to perform any operations on this
  /// reference will fail unless a valid HttpsCallableReference has been
  /// assigned to it.
  HttpsCallableReference() : internal_(nullptr) {}

  /// @brief Copy constructor. It's totally okay (and efficient) to copy
  /// HttpsCallableReference instances, as they simply point to the same
  /// location in the Cloud Functions service.
  ///
  /// @param[in] reference HttpsCallableReference to copy from.
  HttpsCallableReference(const HttpsCallableReference& reference);

  /// @brief Copy assignment operator. It's totally okay (and efficient) to copy
  /// HttpsCallableReference instances, as they simply point to the same
  /// location in the Cloud Functions service.
  ///
  /// @param[in] reference HttpsCallableReference to copy from.
  ///
  /// @returns Reference to the destination HttpsCallableReference.
  HttpsCallableReference& operator=(const HttpsCallableReference& reference);

  /// @brief Move constructor. Moving is an efficient operation for
  /// HttpsCallableReference instances.
  ///
  /// @param[in] other HttpsCallableReference to move data from.
  HttpsCallableReference(HttpsCallableReference&& other) noexcept;

  /// @brief Move assignment operator. Moving is an efficient operation for
  /// HttpsCallableReference instances.
  ///
  /// @param[in] other HttpsCallableReference to move data from.
  ///
  /// @returns Reference to the destination HttpsCallableReference.
  HttpsCallableReference& operator=(HttpsCallableReference&& other) noexcept;

  ~HttpsCallableReference();

  /// @brief Gets the Functions instance to which we refer.
  ///
  /// The pointer will remain valid indefinitely.
  ///
  /// @returns The Functions instance that this HttpsCallableReference refers
  /// to.
  Functions* functions();
  Functions* functions() const;

  /// @brief Asynchronously calls the trigger endpoint with no parameters.
  ///
  /// @returns The result of the call.
  Future<HttpsCallableResult> Call();
  Future<HttpsCallableResult> Call() const;

  /// @brief Asynchronously calls the trigger endpoint with the given
  /// parameter.
  ///
  /// @returns The result of the call as a Future<HttpsCallableResult>.
  Future<HttpsCallableResult> Call(const Variant& data);
  Future<HttpsCallableResult> Call(const Variant& data) const;

  /// @brief Get the result of the most recent Call() call.
  ///
  /// @returns The result of the most recent Call() call.
  Future<HttpsCallableResult> CallLastResult() const;

  /// @brief Returns true if this HttpsCallableReference is valid, false if it
  /// is not valid. An invalid HttpsCallableReference could be returned if, for
  /// example, the Functions instance gets deleted, or if the Reference was
  /// default-constructed.
  ///
  /// @returns true if this HttpsCallableReference is valid, false if this
  /// HttpsCallableReference is invalid.
  bool is_valid() const;

  /// @brief Equality operator.
  bool operator==(const HttpsCallableReference& other) const;

  /// @brief Inequality operator.
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
