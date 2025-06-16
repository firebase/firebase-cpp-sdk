// File: storage/src/desktop/list_result_desktop.h
#ifndef FIREBASE_STORAGE_CLIENT_CPP_SRC_DESKTOP_LIST_RESULT_DESKTOP_H_
#define FIREBASE_STORAGE_CLIENT_CPP_SRC_DESKTOP_LIST_RESULT_DESKTOP_H_

#include <string>
#include <vector>

#include "firebase/storage/storage_reference.h" // For firebase::storage::StorageReference
// Required for StorageReferenceInternal and StorageInternal forward declarations or includes.
#include "storage/src/desktop/storage_reference_desktop.h" // Defines firebase::storage::internal::StorageReferenceInternal for desktop
#include "storage/src/desktop/storage_internal_desktop.h"   // Defines firebase::storage::internal::StorageInternal for desktop

namespace firebase {
namespace storage {
namespace internal {

// This is the Desktop platform's specific PIMPL class for ListResult.
// It does not inherit from any common PIMPL base.
// Its lifecycle is managed by the public ListResult via ListResultInternalCommon static helpers.
class ListResultInternal {
 public:
  // Constructor:
  // - platform_sri: The platform-specific StorageReferenceInternal this list result
  //                 is associated with. Used to get context (e.g., StorageInternal for cleanup).
  // - other_to_copy_from: If not null, this new instance will be a copy of other_to_copy_from.
  //                       Used by ListResult's copy constructor.
  explicit ListResultInternal(
      StorageReferenceInternal* platform_sri,
      const ListResultInternal* other_to_copy_from = nullptr);

  // No virtual destructor needed as it's not a base class for polymorphism here.
  // Cleanup is handled externally. ~ListResultInternal() {}

  const std::vector<StorageReference>& items() const { return items_; }
  const std::vector<StorageReference>& prefixes() const { return prefixes_; }
  const std::string& page_token() const { return page_token_; }

  // Provides access to the StorageReferenceInternal this object is associated with.
  StorageReferenceInternal* storage_reference_internal() const {
    return platform_sri_;
  }

  // Provides access to the StorageInternal context for cleanup registration.
  // This is the method ListResultInternalCommon::GetStorageInternalContext will call.
  StorageInternal* associated_storage_internal() const {
    return platform_sri_ ? platform_sri_->storage_internal() : nullptr;
  }

 private:
  // Disallow direct copy assignment (copy construction is handled via constructor)
  ListResultInternal& operator=(const ListResultInternal&);

  StorageReferenceInternal* platform_sri_; // For context, not owned.

  // Stub data
  std::vector<StorageReference> items_;
  std::vector<StorageReference> prefixes_;
  std::string page_token_;
};

}  // namespace internal
}  // namespace storage
}  // namespace firebase

#endif  // FIREBASE_STORAGE_CLIENT_CPP_SRC_DESKTOP_LIST_RESULT_DESKTOP_H_
