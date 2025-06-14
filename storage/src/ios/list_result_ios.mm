#include "storage/src/ios/list_result_ios.h"

#import <FirebaseStorage/FirebaseStorage.h>

#include "app/src/assert.h"
#include "firebase/storage/storage_reference.h"
#include "storage/src/ios/storage_reference_ios.h" // For StorageReferenceInternalIOS constructor
#include "storage/src/ios/util_ios.h"              // For NSStringToStdString

namespace firebase {
namespace storage {
namespace internal {

ListResultInternalIOS::ListResultInternalIOS(
    StorageInternal* storage_internal, FIRStorageListResult* list_result_objc)
    : storage_internal_(storage_internal),
      list_result_objc_ptr_(new FIRStorageListResultPointer(list_result_objc)), // Assumes list_result_objc is retained by FIRStorageListResultPointer
      items_converted_(false),
      page_token_converted_(false) {
  FIREBASE_ASSERT(storage_internal_ != nullptr);
  // list_result_objc can be nil if an error occurred, which is handled by is_valid()
}

ListResultInternalIOS::~ListResultInternalIOS() {}

const std::vector<StorageReference>& ListResultInternalIOS::items() const {
  if (!items_converted_ && list_result_objc_ptr_ && list_result_objc_ptr_->get()) {
    @autoreleasepool {
      FIRStorageListResult* list_result = list_result_objc_ptr_->get();
      NSArray<FIRStorageReference*>* items_objc = list_result.items;
      items_cpp_.reserve(items_objc.count);
      for (FIRStorageReference* item_objc in items_objc) {
        // Each item is an FIRStorageReference. Create C++ StorageReference from it.
        // This requires the StorageInternal pointer.
        // The StorageReferenceInternalIOS constructor takes ownership (retains).
        StorageReferenceInternalIOS* sr_internal = new StorageReferenceInternalIOS(
            storage_internal_,
            std::make_unique<FIRStorageReferencePointer>(item_objc));
        items_cpp_.emplace_back(sr_internal);
      }
      items_converted_ = true;
    }
  }
  return items_cpp_;
}

const std::string& ListResultInternalIOS::page_token() const {
  if (!page_token_converted_ && list_result_objc_ptr_ && list_result_objc_ptr_->get()) {
    @autoreleasepool {
      FIRStorageListResult* list_result = list_result_objc_ptr_->get();
      NSString* page_token_nsstring = list_result.pageToken;
      if (page_token_nsstring) {
        page_token_cpp_ = NSStringToStdString(page_token_nsstring);
      }
      page_token_converted_ = true;
    }
  }
  return page_token_cpp_;
}

bool ListResultInternalIOS::is_valid() const {
  return list_result_objc_ptr_ && list_result_objc_ptr_->get() != nullptr &&
         storage_internal_ != nullptr;
}

ListResultInternal* ListResultInternalIOS::Clone() {
  if (!is_valid()) return nullptr;
  // FIRStorageListResult might conform to NSCopying. If so, use it.
  // Otherwise, a shallow copy (new C++ wrapper around same ObjC object) is made.
  // For now, assume shallow copy is acceptable (ObjC object is immutable or shared).
  FIRStorageListResult* list_result_objc = list_result_objc_ptr_->get();
  // If FIRStorageListResult is copyable:
  // FIRStorageListResult* cloned_list_result_objc = [list_result_objc copy];
  // return new ListResultInternalIOS(storage_internal_, cloned_list_result_objc);
  // [cloned_list_result_objc release]; // if copy returns +1 retain count and not ARC by constructor
  // For now, re-wrap the same pointer (effectively a shared_ptr like behavior if ObjC object is immutable)
  return new ListResultInternalIOS(storage_internal_, list_result_objc);
}

}  // namespace internal
}  // namespace storage
}  // namespace firebase
