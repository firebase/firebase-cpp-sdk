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

#include "storage/src/ios/list_result_ios.h"

#import <FirebaseStorage/FIRStorageListResult.h>
#import <FirebaseStorage/FIRStorageReference.h>
#import <Foundation/Foundation.h>

#include "app/src/assert.h"
#include "app/src/ios/c_string_manager.h" // For CStringManager
#include "storage/src/ios/converter_ios.h" // For NSStringToStdString
#include "storage/src/ios/storage_ios.h"   // For StorageInternal
#include "storage/src/ios/storage_reference_ios.h" // For StorageReferenceInternal and FIRStorageReferencePointer

namespace firebase {
namespace storage {
namespace internal {

ListResultInternal::ListResultInternal(
    StorageInternal* storage_internal,
    std::unique_ptr<FIRStorageListResultPointer> impl)
    : storage_internal_(storage_internal), impl_(std::move(impl)) {
  FIREBASE_ASSERT(storage_internal_ != nullptr);
  FIREBASE_ASSERT(impl_ != nullptr && impl_->get() != nullptr);
}

ListResultInternal::ListResultInternal(const ListResultInternal& other)
    : storage_internal_(other.storage_internal_), impl_(nullptr) {
  FIREBASE_ASSERT(storage_internal_ != nullptr);
  if (other.impl_ && other.impl_->get()) {
    // FIRStorageListResult does not conform to NSCopying.
    // To "copy" it, we'd typically re-fetch or if it's guaranteed immutable,
    // we could retain the original. However, unique_ptr implies single ownership.
    // For now, this copy constructor will create a ListResultInternal that
    // shares the *same* underlying Objective-C object by retaining it and
    // creating a new FIRStorageListResultPointer.
    // This is generally not safe if the object is mutable or if true deep copy semantics
    // are expected by the C++ ListResult's copy constructor.
    // Given ListResult is usually a snapshot, sharing might be acceptable.
    // TODO(b/180010117): Clarify copy semantics for ListResultInternal on iOS.
    // A truly safe copy would involve creating a new FIRStorageListResult with the same contents.
    // For now, we are making the unique_ptr point to the same ObjC object.
    // This is done by getting the raw pointer, creating a new unique_ptr that points to it,
    // and relying on FIRStorageListResultPointer's constructor to retain it.
    // This breaks unique_ptr's unique ownership if the original unique_ptr still exists and manages it.
    // A better approach for copy would be to create a new FIRStorageListResult with the same properties.
    // As a placeholder, we will make a "copy" that points to the same Obj-C object,
    // which is what FIRStorageListResultPointer(other.impl_->get()) would do.
    impl_ = std::make_unique<FIRStorageListResultPointer>(other.impl_->get());
  }
}

ListResultInternal& ListResultInternal::operator=(
    const ListResultInternal& other) {
  if (&other == this) {
    return *this;
  }
  storage_internal_ = other.storage_internal_;
  FIREBASE_ASSERT(storage_internal_ != nullptr);
  if (other.impl_ && other.impl_->get()) {
    // See notes in copy constructor regarding shared ownership.
    impl_ = std::make_unique<FIRStorageListResultPointer>(other.impl_->get());
  } else {
    impl_.reset();
  }
  return *this;
}

std::vector<StorageReference>
ListResultInternal::ProcessObjectiveCReferenceArray(
    NSArray<FIRStorageReference*>* ns_array_ref) const {
  std::vector<StorageReference> cpp_references;
  if (ns_array_ref == nil) {
    return cpp_references;
  }
  for (FIRStorageReference* objc_ref in ns_array_ref) {
    FIREBASE_ASSERT(objc_ref != nil);
    // The StorageReferenceInternal constructor takes ownership of the pointer if unique_ptr is used directly.
    // Here, FIRStorageReferencePointer constructor will retain the objc_ref.
    auto sfr_internal = new StorageReferenceInternal(
        storage_internal_,
        std::make_unique<FIRStorageReferencePointer>(objc_ref));
    cpp_references.push_back(StorageReference(sfr_internal));
  }
  return cpp_references;
}

std::vector<StorageReference> ListResultInternal::items() const {
  FIREBASE_ASSERT(impl_ != nullptr && impl_->get() != nullptr);
  FIRStorageListResult* list_result_objc = impl_->get();
  return ProcessObjectiveCReferenceArray(list_result_objc.items);
}

std::vector<StorageReference> ListResultInternal::prefixes() const {
  FIREBASE_ASSERT(impl_ != nullptr && impl_->get() != nullptr);
  FIRStorageListResult* list_result_objc = impl_->get();
  return ProcessObjectiveCReferenceArray(list_result_objc.prefixes);
}

std::string ListResultInternal::page_token() const {
  FIREBASE_ASSERT(impl_ != nullptr && impl_->get() != nullptr);
  FIRStorageListResult* list_result_objc = impl_->get();
  if (list_result_objc.pageToken == nil) {
    return "";
  }
  return NSStringToStdString(list_result_objc.pageToken);
}

}  // namespace internal
}  // namespace storage
}  // namespace firebase
