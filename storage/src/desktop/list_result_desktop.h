// Copyright 2021 Google LLC
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

#ifndef FIREBASE_STORAGE_SRC_DESKTOP_LIST_RESULT_DESKTOP_H_
#define FIREBASE_STORAGE_SRC_DESKTOP_LIST_RESULT_DESKTOP_H_

#include <string>
#include <vector>

#include "firebase/storage/storage_reference.h"
#include "storage/src/common/storage_internal.h"
// It's okay for platform specific internal headers to include common internal headers.
#include "storage/src/common/list_result_internal_common.h"


namespace firebase {
namespace storage {

// Forward declaration for platform-specific ListResultInternal.
class ListResult;

namespace internal {

// Declare ListResultInternal a friend of ListResultInternalCommon for construction.
class ListResultInternalCommon;

// Contains the Desktop-specific implementation of ListResultInternal (stubs).
class ListResultInternal {
 public:
  // Constructor.
  explicit ListResultInternal(StorageInternal* storage_internal);

  // Constructor that can take pre-populated data (though stubs won't use it).
  ListResultInternal(StorageInternal* storage_internal,
                     const std::vector<StorageReference>& items,
                     const std::vector<StorageReference>& prefixes,
                     const std::string& page_token);


  // Destructor (default is fine).
  ~ListResultInternal() = default;

  // Copy constructor.
  ListResultInternal(const ListResultInternal& other);

  // Copy assignment operator.
  ListResultInternal& operator=(const ListResultInternal& other);


  // Gets the items (files) in this result (stub).
  std::vector<StorageReference> items() const {
    return std::vector<StorageReference>();
  }

  // Gets the prefixes (folders) in this result (stub).
  std::vector<StorageReference> prefixes() const {
    return std::vector<StorageReference>();
  }

  // Gets the page token for the next page of results (stub).
  std::string page_token() const { return ""; }

  // Returns the StorageInternal object associated with this ListResult.
  StorageInternal* storage_internal() const { return storage_internal_; }

 private:
  friend class firebase::storage::ListResult;
  // For ListResultInternalCommon's constructor and access to app_ via
  // storage_internal().
  friend class ListResultInternalCommon;

  StorageInternal* storage_internal_; // Not owned.
  // Desktop stubs don't actually store these, but defined to match constructor.
  std::vector<StorageReference> items_stub_;
  std::vector<StorageReference> prefixes_stub_;
  std::string page_token_stub_;
};

}  // namespace internal
}  // namespace storage
}  // namespace firebase

#endif  // FIREBASE_STORAGE_SRC_DESKTOP_LIST_RESULT_DESKTOP_H_
