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

#ifndef FIREBASE_FUNCTIONS_SRC_INCLUDE_FIREBASE_FUNCTIONS_CALLABLE_RESULT_H_
#define FIREBASE_FUNCTIONS_SRC_INCLUDE_FIREBASE_FUNCTIONS_CALLABLE_RESULT_H_

#include <utility>

#include "firebase/functions/common.h"
#include "firebase/variant.h"

namespace firebase {
namespace functions {

/// @cond FIREBASE_APP_INTERNAL
namespace internal {
class HttpsCallableReferenceInternal;
}  // namespace internal
/// @endcond

/// @brief An HttpsCallableResult contains the result of calling a
/// HttpsCallable.
class HttpsCallableResult {
 public:
  /// @brief Creates an empty HttpsCallableResult with `Variant::Null()` data.
  HttpsCallableResult() = default;

  /// @brief Creates an HttpsCallableResult containing the given data.
  explicit HttpsCallableResult(const Variant& data) : data_(data) {}

  /// @brief Creates an HttpsCallableResult by moving the given data.
  explicit HttpsCallableResult(Variant&& data) noexcept
      : data_(std::move(data)) {}

  ~HttpsCallableResult() = default;

  /// @brief Copy constructor. Capable of copying an empty HttpsCallableResult.
  ///
  /// @param[in] other HttpsCallableResult to copy from.
  HttpsCallableResult(const HttpsCallableResult& other) = default;

  /// @brief Assignment operator. Capable of copying an empty
  /// HttpsCallableResult.
  ///
  /// @param[in] other HttpsCallableResult to copy from.
  /// @return Reference to this HttpsCallableResult.
  HttpsCallableResult& operator=(const HttpsCallableResult& other) = default;

  /// @brief Move constructor. Capable of moving an empty HttpsCallableResult.
  ///
  /// @param[in] other HttpsCallableResult to move into this one.
  HttpsCallableResult(HttpsCallableResult&& other) noexcept = default;

  /// @brief Move assignment operator. Capable of moving an empty
  /// HttpsCallableResult.
  ///
  /// @param[in] other HttpsCallableResult to move into this one.
  /// @return Reference to this HttpsCallableResult.
  HttpsCallableResult& operator=(HttpsCallableResult&& other) noexcept =
      default;

  /// @brief Returns the data that was returned from the call to the
  /// HttpsCallable.
  ///
  /// @returns The data from the call, as a Variant.
  const Variant& data() const { return data_; }

  /// @brief Returns a mutable reference to the data returned from the call.
  Variant& data() { return data_; }

  /// @brief Returns true if the result contains non-null data.
  bool is_valid() const { return !data_.is_null(); }

  /// @brief Equality operator.
  bool operator==(const HttpsCallableResult& other) const {
    return data_ == other.data_;
  }

  /// @brief Inequality operator.
  bool operator!=(const HttpsCallableResult& other) const {
    return !(*this == other);
  }

 private:
  /// @cond FIREBASE_APP_INTERNAL
  friend class internal::HttpsCallableReferenceInternal;
  /// @endcond

  Variant data_;
};

}  // namespace functions
}  // namespace firebase

#endif  // FIREBASE_FUNCTIONS_SRC_INCLUDE_FIREBASE_FUNCTIONS_CALLABLE_RESULT_H_
