// File: storage/src/android/list_result_android.h
#ifndef FIREBASE_STORAGE_CLIENT_CPP_SRC_ANDROID_LIST_RESULT_ANDROID_H_
#define FIREBASE_STORAGE_CLIENT_CPP_SRC_ANDROID_LIST_RESULT_ANDROID_H_

#include <string>
#include <vector>

#include "firebase/storage/storage_reference.h"
#include "storage/src/android/storage_internal_android.h"
#include "storage/src/android/storage_reference_android.h"

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

#endif  // FIREBASE_STORAGE_CLIENT_CPP_SRC_ANDROID_LIST_RESULT_ANDROID_H_
