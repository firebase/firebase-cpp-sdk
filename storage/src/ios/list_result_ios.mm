#import "storage/src/ios/list_result_ios.h"

#import <Foundation/Foundation.h>
#include "app/src/include/firebase/app.h" // For App
#include "storage/src/ios/storage_reference_ios.h" // For StorageReferenceInternal casting, if needed.


namespace firebase {
namespace storage {
namespace internal {

ListResultIOS::ListResultIOS(StorageReferenceInternal* s_ref_internal)
    : ListResultInternal(s_ref_internal),
      items_(),
      prefixes_(),
      page_token_("") {
  // Data is already initialized to empty/default in the member initializer list.
}

ListResultIOS::~ListResultIOS() {}

const std::vector<StorageReference>& ListResultIOS::items() const {
  return items_;
}

const std::vector<StorageReference>& ListResultIOS::prefixes() const {
  return prefixes_;
}

const std::string& ListResultIOS::page_token() const {
  return page_token_;
}

ListResultInternal* ListResultIOS::Clone(StorageReferenceInternal* new_parent_sri) const {
  // For the stub, we create a new ListResultIOS.
  // It will also contain empty data, just like the original stub.
  // The new clone will be associated with the new_parent_sri for cleanup.
  ListResultIOS* clone = new ListResultIOS(new_parent_sri);
  // Since items_, prefixes_, and page_token_ are initialized to empty by the
  // constructor and this is a stub, no explicit member-wise copy is needed here.
  return clone;
}

}  // namespace internal
}  // namespace storage
}  // namespace firebase
