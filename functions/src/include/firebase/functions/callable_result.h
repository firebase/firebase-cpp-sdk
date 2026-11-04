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

class HttpsCallableResult {
 public:
  HttpsCallableResult() = default;
  explicit HttpsCallableResult(const Variant& data) : data_(data) {}
  explicit HttpsCallableResult(Variant&& data) noexcept
      : data_(std::move(data)) {}
  ~HttpsCallableResult() = default;

  HttpsCallableResult(const HttpsCallableResult& other) = default;
  HttpsCallableResult& operator=(const HttpsCallableResult& other) = default;
  HttpsCallableResult(HttpsCallableResult&& other) noexcept = default;
  HttpsCallableResult& operator=(HttpsCallableResult&& other) noexcept =
      default;

  const Variant& data() const { return data_; }
  Variant& data() { return data_; }
  bool is_valid() const { return !data_.is_null(); }

  bool operator==(const HttpsCallableResult& other) const {
    return data_ == other.data_;
  }
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
