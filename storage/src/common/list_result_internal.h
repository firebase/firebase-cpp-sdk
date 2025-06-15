#ifndef FIREBASE_STORAGE_CLIENT_CPP_SRC_COMMON_LIST_RESULT_INTERNAL_H_
#define FIREBASE_STORAGE_CLIENT_CPP_SRC_COMMON_LIST_RESULT_INTERNAL_H_

#include <string>
#include <vector>

#include "app/src/cleanup_notifier.h"
#include "app/src/include/firebase/internal/common.h"
#include "firebase/storage/storage_reference.h"
// For FIREBASE_ASSERT_RETURN_VOID
#include "app/src/assert.h"
// For LogDebug - ensure this path is correct or remove if not strictly needed for stub
// #include "app/src/log.h"


namespace firebase {
namespace storage {
namespace internal {

// Forward declaration
class StorageReferenceInternal;

class ListResultInternal : public firebase::internal::InternalCleanupNotifierInterface {
 public:
  ListResultInternal(StorageReferenceInternal*  storage_reference_internal) :
      storage_reference_internal_(storage_reference_internal) {
    FIREBASE_ASSERT_RETURN_VOID(storage_reference_internal_ != nullptr);
    // Null check for storage_reference_internal_ is implicitly handled by FIREBASE_ASSERT_RETURN_VOID

    CleanupNotifier* notifier = GetCleanupNotifier();
    if (notifier) {
        notifier->RegisterObject(this, [](void* object) {
        ListResultInternal* internal_obj =
            reinterpret_cast<ListResultInternal*>(object);
        // Perform any specific cleanup for ListResultInternal if needed in the future.
        // For now, it's mainly for managing the object's lifecycle with CleanupNotifier.
        // LogDebug("ListResultInternal object %p cleaned up.", internal_obj); // LogDebug might require app/src/log.h
      });
    }
  }

  ~ListResultInternal() override {
    if (storage_reference_internal_ != nullptr) {
        CleanupNotifier* notifier = GetCleanupNotifier();
        if (notifier) {
            notifier->UnregisterObject(this);
        }
    }
    storage_reference_internal_ = nullptr;
  }

  virtual const std::vector<StorageReference>& items() const = 0;
  virtual const std::vector<StorageReference>& prefixes() const = 0;
  virtual const std::string& page_token() const = 0;

  // Clones the ListResultInternal object.
  // The caller takes ownership of the returned pointer.
  // The new_parent_sri is the StorageReferenceInternal that will "own"
  // the cleanup of this new cloned ListResultInternal.
  virtual ListResultInternal* Clone(StorageReferenceInternal* new_parent_sri) const = 0;

  CleanupNotifier* GetCleanupNotifier() const {
    // Using plain assert for const methods or ensure validity before calling.
    // Or, if FIREBASE_ASSERT_RETURN can take a non-void return like nullptr:
    // FIREBASE_ASSERT_RETURN(nullptr, storage_reference_internal_ != nullptr);
    if (storage_reference_internal_ == nullptr) return nullptr;
    return CleanupNotifier::FindByOwner(storage_reference_internal_);
  }

 protected:
  StorageReferenceInternal* storage_reference_internal_;
};

}  // namespace internal
}  // namespace storage
}  // namespace firebase

#endif  // FIREBASE_STORAGE_CLIENT_CPP_SRC_COMMON_LIST_RESULT_INTERNAL_H_
