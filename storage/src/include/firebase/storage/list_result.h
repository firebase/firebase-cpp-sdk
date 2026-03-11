// Copyright 2024 Google LLC
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

#ifndef FIREBASE_STORAGE_SRC_INCLUDE_FIREBASE_STORAGE_LIST_RESULT_H_
#define FIREBASE_STORAGE_SRC_INCLUDE_FIREBASE_STORAGE_LIST_RESULT_H_

#include <vector>

namespace firebase {
namespace storage {

class StorageReference;

namespace internal {
class StorageListResultInternal;
class StorageReferenceInternal;
}  // namespace internal

/// @brief Represents the result of calling List on a StorageReference.
class StorageListResult {
 public:
  StorageListResult();
  ~StorageListResult();
  /// @brief Copy constructor.
  StorageListResult(const StorageListResult& other);

  /// @brief Copy assignment operator.
  StorageListResult& operator=(const StorageListResult& other);

#if defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)
  /// @brief Move constructor.
  StorageListResult(StorageListResult&& other);

  /// @brief Move assignment operator.
  StorageListResult& operator=(StorageListResult&& other);
#endif  // defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)

  /// @brief The prefixes (folders) returned by the List operation.
  const std::vector<StorageReference>& prefixes() const;

  /// @brief The items (files) returned by the List operation.
  const std::vector<StorageReference>& items() const;

  /// @brief Returns a token that can be used to resume a previous List
  /// operation.
  ///
  /// `nullptr` if there are no more results.
  const char* next_page_token() const;

 private:
  /// @cond FIREBASE_APP_INTERNAL
  friend class StorageReference;
  friend class internal::StorageReferenceInternal;

  StorageListResult(internal::StorageListResultInternal* internal);

  internal::StorageListResultInternal* internal_;
  /// @endcond
};

}  // namespace storage
}  // namespace firebase

#endif  // FIREBASE_STORAGE_SRC_INCLUDE_FIREBASE_STORAGE_LIST_RESULT_H_
