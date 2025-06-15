#ifndef FIREBASE_STORAGE_CLIENT_CPP_SRC_IOS_LIST_RESULT_IOS_H_
#define FIREBASE_STORAGE_CLIENT_CPP_SRC_IOS_LIST_RESULT_IOS_H_

#include <string>
#include <vector>

#include "storage/src/common/list_result_internal.h"
#include "firebase/storage/storage_reference.h" // Required for StorageReference

namespace firebase {
namespace storage {
namespace internal {

class StorageReferenceInternal; // Forward declaration

class ListResultIOS : public ListResultInternal {
 public:
  explicit ListResultIOS(StorageReferenceInternal* S_ref_internal);
  ~ListResultIOS() override;

  const std::vector<StorageReference>& items() const override;
  const std::vector<StorageReference>& prefixes() const override;
  const std::string& page_token() const override;

      ListResultInternal* Clone(StorageReferenceInternal* new_parent_sri) const override;

 private:
  std::vector<StorageReference> items_;
  std::vector<StorageReference> prefixes_;
  std::string page_token_; // Empty for stub
};

}  // namespace internal
}  // namespace storage
}  // namespace firebase

#endif  // FIREBASE_STORAGE_CLIENT_CPP_SRC_IOS_LIST_RESULT_IOS_H_
