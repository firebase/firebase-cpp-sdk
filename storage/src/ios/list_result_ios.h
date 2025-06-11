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

#ifndef FIREBASE_STORAGE_SRC_IOS_LIST_RESULT_IOS_H_
#define FIREBASE_STORAGE_SRC_IOS_LIST_RESULT_IOS_H_

#include <string>
#include <vector>
#include <memory> // For std::unique_ptr

#include "firebase/storage/storage_reference.h"
// #include "storage/src/common/storage_internal.h" // Removed
#include "storage/src/ios/storage_ios.h" // Added for iOS StorageInternal
// #include "storage/src/common/list_result_internal_common.h" // Removed
#include "storage/src/ios/fir_storage_list_result_pointer.h" // For FIRStorageListResultPointer

// Forward declare Obj-C types
#ifdef __OBJC__
@class FIRStorageListResult;
@class FIRStorageReference;
#else
typedef struct objc_object FIRStorageListResult;
typedef struct objc_object FIRStorageReference;
#endif


namespace firebase {
namespace storage {

// Forward declaration for platform-specific ListResultInternal.
class ListResult;

namespace internal {

// Declare ListResultInternal a friend of ListResultInternalCommon for construction.
// class ListResultInternalCommon; // Removed

// Contains the iOS-specific implementation of ListResultInternal.
class ListResultInternal {
 public:
  // Constructor.
  // Takes ownership of the impl unique_ptr.
  ListResultInternal(StorageInternal* storage_internal,
                     std::unique_ptr<FIRStorageListResultPointer> impl);

  // Copy constructor.
  ListResultInternal(const ListResultInternal& other);

  // Copy assignment operator.
  ListResultInternal& operator=(const ListResultInternal& other);

  // Destructor (default is fine thanks to unique_ptr).
  ~ListResultInternal() = default;

  // Gets the items (files) in this result.
  std::vector<StorageReference> items() const;

  // Gets the prefixes (folders) in this result.
  std::vector<StorageReference> prefixes() const;

  // Gets the page token for the next page of results.
  // Returns an empty string if there are no more results.
  std::string page_token() const;

  // Returns the underlying Objective-C FIRStorageListResult object.
  FIRStorageListResult* impl() const { return impl_->get(); }

  // Returns the StorageInternal object associated with this ListResult.
  StorageInternal* storage_internal() const { return storage_internal_; }

 private:
  friend class firebase::storage::ListResult;
  // For ListResultInternalCommon's constructor and access to app_ via
  // storage_internal().
  // friend class ListResultInternalCommon; // Removed

  // Converts an NSArray of FIRStorageReference objects to a C++ vector of
  // C++ StorageReference objects.
  std::vector<StorageReference> ProcessObjectiveCReferenceArray(
      NSArray<FIRStorageReference*>* ns_array_ref) const;

  StorageInternal* storage_internal_;  // Not owned.
  // Pointer to the Objective-C FIRStorageListResult instance.
  std::unique_ptr<FIRStorageListResultPointer> impl_;
};

}  // namespace internal
}  // namespace storage
}  // namespace firebase

#endif  // FIREBASE_STORAGE_SRC_IOS_LIST_RESULT_IOS_H_
