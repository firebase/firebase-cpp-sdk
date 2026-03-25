// Copyright 2019 Google LLC
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

#include <string>
#include <vector>

#include "firebase/internal/common.h"

namespace firebase {
namespace storage {

namespace internal {
class StorageListResultInternal;
}  // namespace internal

class StorageReference;

/// @brief Contains the prefixes and items returned by a StorageReference::List
/// call.
class StorageListResult {
 public:
  /// @brief Default constructor. This creates an invalid StorageListResult.
  StorageListResult();

  /// @brief Destructor.
  ~StorageListResult();

  /// @brief Copy constructor.
  ///
  /// @param[in] other StorageListResult to copy from.
  StorageListResult(const StorageListResult& other);

  /// @brief Copy assignment operator.
  ///
  /// @param[in] other StorageListResult to copy from.
  ///
  /// @returns Reference to the destination StorageListResult.
  StorageListResult& operator=(const StorageListResult& other);

#if defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)
  /// @brief Move constructor.
  ///
  /// @param[in] other StorageListResult to move data from.
  StorageListResult(StorageListResult&& other);

  /// @brief Move assignment operator.
  ///
  /// @param[in] other StorageListResult to move data from.
  ///
  /// @returns Reference to the destination StorageListResult.
  StorageListResult& operator=(StorageListResult&& other);
#endif  // defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)

  /// @brief Returns the prefixes (folders) returned by the List operation.
  ///
  /// @returns A list of prefixes (folders).
  const std::vector<StorageReference>& prefixes() const;

  /// @brief Returns the items (files) returned by the List operation.
  ///
  /// @returns A list of items (files).
  const std::vector<StorageReference>& items() const;

  /// @brief Returns a token that can be used to resume a previous List
  /// operation.
  ///
  /// @returns A page token if more results are available, or an empty string if
  /// there are no more results.
  const std::string& next_page_token() const;

  /// @brief Returns true if this StorageListResult is valid, false if it is not
  /// valid.
  bool is_valid() const;

 private:
  /// @cond FIREBASE_APP_INTERNAL
  friend class internal::StorageListResultInternal;
  /// @endcond

  internal::StorageListResultInternal* internal_;
};

}  // namespace storage
}  // namespace firebase

#endif  // FIREBASE_STORAGE_SRC_INCLUDE_FIREBASE_STORAGE_LIST_RESULT_H_
