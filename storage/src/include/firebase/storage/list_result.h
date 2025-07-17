// Copyright 2025 Google LLC
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

#include "firebase/storage/storage_reference.h"

namespace firebase {
namespace storage {

// Forward declaration for internal class.
namespace internal {
class ListResultInternal;
}  // namespace internal

/// @brief ListResult contains a list of items and prefixes from a list()
/// call.
///
/// This is a result from a list() call on a StorageReference.
class ListResult {
 public:
  /// @brief Creates an invalid ListResult.
  ListResult();

  /// @brief Creates a ListResult from an internal ListResult object.
  ///
  /// This constructor is not intended for public use.
  explicit ListResult(internal::ListResultInternal* internal);

  /// @brief Copy constructor.
  ListResult(const ListResult& other);

  /// @brief Copy assignment operator.
  ListResult& operator=(const ListResult& other);

  /// @brief Move constructor.
  ListResult(ListResult&& other);

  /// @brief Move assignment operator.
  ListResult& operator=(ListResult&& other);

  /// @brief Destructor.
  ~ListResult();

  /// @brief Returns the items (files) in this result.
  std::vector<firebase::storage::StorageReference> items() const;

  /// @brief Returns the prefixes (folders) in this result.
  std::vector<firebase::storage::StorageReference> prefixes() const;

  /// @brief If set, there are more results to retrieve.
  ///
  /// Pass this token to list() to retrieve the next page of results.
  std::string page_token() const;

  /// @brief Returns true if this ListResult is valid, false if it is not
  /// valid. An invalid ListResult indicates that the operation that was
  /// to create this ListResult failed.
  bool is_valid() const;

 private:
  /// @cond FIREBASE_APP_INTERNAL
  // Method called by the CleanupNotifier global callback to perform cleanup.
  void ClearInternalForCleanup();
  friend void GlobalCleanupListResult(void* list_result_void);
  /// @endcond

  internal::ListResultInternal* internal_;
};

}  // namespace storage
}  // namespace firebase

#endif  // FIREBASE_STORAGE_SRC_INCLUDE_FIREBASE_STORAGE_LIST_RESULT_H_
