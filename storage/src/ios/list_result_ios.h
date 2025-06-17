// File: storage/src/ios/list_result_ios.h
#ifndef FIREBASE_STORAGE_CLIENT_CPP_SRC_IOS_LIST_RESULT_IOS_H_
#define FIREBASE_STORAGE_CLIENT_CPP_SRC_IOS_LIST_RESULT_IOS_H_

#include <string>
#include <vector>

#include "firebase/storage/storage_reference.h"
#include "storage/src/ios/storage_reference_ios.h"
#include "storage/src/ios/storage_internal_ios.h"

namespace firebase {
namespace storage {
namespace internal {

class ListResultInternal {
 public:
  explicit ListResultInternal(
      StorageReferenceInternal* platform_sri,
      const ListResultInternal* other_to_copy_from = nullptr);

  const std::vector<StorageReference>& items() const { return items_; }
  const std::vector<StorageReference>& prefixes() const { return prefixes_; }
  const std::string& page_token() const { return page_token_; }

  StorageReferenceInternal* storage_reference_internal() const {
    return platform_sri_;
  }
  StorageInternal* associated_storage_internal() const {
    return platform_sri_ ? platform_sri_->storage_internal() : nullptr;
  }

 private:
  ListResultInternal& operator=(const ListResultInternal&);

  StorageReferenceInternal* platform_sri_;
  std::vector<StorageReference> items_;
  std::vector<StorageReference> prefixes_;
  std::string page_token_;
};

}  // namespace internal
}  // namespace storage
}  // namespace firebase

#endif  // FIREBASE_STORAGE_CLIENT_CPP_SRC_IOS_LIST_RESULT_IOS_H_
