// File: storage/src/common/list_result.cc

#include "firebase/storage/list_result.h"

#include "app/src/cleanup_notifier.h"
#include "app/src/include/firebase/internal/platform.h"
#include "app/src/log.h"  // For LogWarning, LogDebug (used sparingly)
#include "firebase/storage/storage_reference.h"

// Platform-specific headers that define internal::ListResultInternal (the PIMPL
// class) and internal::StorageInternal (for CleanupNotifier context).
#if FIREBASE_PLATFORM_ANDROID
#include "storage/src/android/list_result_android.h"
#include "storage/src/android/storage_internal_android.h"
#elif FIREBASE_PLATFORM_IOS || FIREBASE_PLATFORM_TVOS
#include "storage/src/ios/list_result_ios.h"
#include "storage/src/ios/storage_internal_ios.h"
#else  // Desktop
#include "storage/src/desktop/list_result_desktop.h"
#include "storage/src/desktop/storage_internal_desktop.h"
#endif

namespace firebase {
namespace storage {

// Forward declaration of the PIMPL class and related internal types.
namespace internal {
class ListResultInternal;
class StorageReferenceInternal;
class StorageInternal;
}  // namespace internal

namespace internal {

// ListResultInternalCommon: Provides static helper methods for managing the
// lifecycle of the ListResultInternal PIMPL object.
class ListResultInternalCommon {
 public:
  // Retrieves the StorageInternal context from the ListResultInternal object.
  static StorageInternal* GetStorageInternalContext(
      ListResultInternal* pimpl_obj) {
    if (!pimpl_obj) return nullptr;
    StorageInternal* storage_ctx = pimpl_obj->associated_storage_internal();
    if (storage_ctx == nullptr) {
      LogWarning(
          "ListResultInternal %p has no associated StorageInternal for "
          "cleanup context.",
          pimpl_obj);
    }
    return storage_ctx;
  }

  // Callback for CleanupNotifier, invoked when the App is being destroyed.
  static void CleanupPublicListResultObject(void* public_obj_void) {
    ListResult* public_obj = reinterpret_cast<ListResult*>(public_obj_void);
    if (public_obj) {
      LogDebug(
          "CleanupNotifier: Cleaning up ListResult %p due to App shutdown.",
          public_obj);
      DeleteInternal(public_obj);  // Ensure PIMPL is deleted.
    } else {
      LogWarning(
          "CleanupNotifier: CleanupPublicListResultObject called with null "
          "object.");
    }
  }

  static void RegisterForCleanup(ListResult* public_obj,
                                 ListResultInternal* pimpl_obj) {
    FIREBASE_ASSERT(public_obj != nullptr);
    if (!pimpl_obj) return;
    StorageInternal* storage_ctx = GetStorageInternalContext(pimpl_obj);
    if (storage_ctx) {
      storage_ctx->cleanup().RegisterObject(public_obj,
                                            CleanupPublicListResultObject);
    } else {
      LogWarning(  // Kept this warning as it indicates a potential issue.
          "Could not register ListResult %p for cleanup: no StorageInternal "
          "context.",
          public_obj);
    }
  }

  static void UnregisterFromCleanup(ListResult* public_obj,
                                    ListResultInternal* pimpl_obj) {
    FIREBASE_ASSERT(public_obj != nullptr);
    if (!pimpl_obj)
      return;  // If no PIMPL, it couldn't have been registered with its
               // context.
    StorageInternal* storage_ctx = GetStorageInternalContext(pimpl_obj);
    if (storage_ctx) {
      storage_ctx->cleanup().UnregisterObject(public_obj);
    }
  }

  // Deletes the PIMPL object, unregisters from cleanup, and nulls the pointer
  // in public_obj.
  static void DeleteInternal(ListResult* public_obj) {
    FIREBASE_ASSERT(public_obj != nullptr);
    if (!public_obj->internal_) return;

    ListResultInternal* pimpl_to_delete = public_obj->internal_;
    // Unregister the public object using the context from the PIMPL about to be
    // deleted.
    UnregisterFromCleanup(public_obj, pimpl_to_delete);
    public_obj->internal_ = nullptr;
    delete pimpl_to_delete;
  }
};

}  // namespace internal

// --- Public ListResult Method Implementations ---

const std::vector<StorageReference> ListResult::s_empty_items_;
const std::vector<StorageReference> ListResult::s_empty_prefixes_;
const std::string ListResult::s_empty_page_token_;

ListResult::ListResult() : internal_(nullptr) {}

ListResult::ListResult(internal::ListResultInternal* internal_pimpl)
    : internal_(internal_pimpl) {
  if (internal_) {
    internal::ListResultInternalCommon::RegisterForCleanup(this, internal_);
  }
}

ListResult::~ListResult() {
  internal::ListResultInternalCommon::DeleteInternal(this);
}

ListResult::ListResult(const ListResult& other) : internal_(nullptr) {
  if (other.internal_) {
    internal::StorageReferenceInternal* sri_context =
        other.internal_->storage_reference_internal();
    internal_ = new internal::ListResultInternal(sri_context, other.internal_);
    internal::ListResultInternalCommon::RegisterForCleanup(this, internal_);
  }
}

ListResult& ListResult::operator=(const ListResult& other) {
  if (this == &other) {
    return *this;
  }
  internal::ListResultInternalCommon::DeleteInternal(this);

  if (other.internal_) {
    internal::StorageReferenceInternal* sri_context =
        other.internal_->storage_reference_internal();
    internal_ = new internal::ListResultInternal(sri_context, other.internal_);
    internal::ListResultInternalCommon::RegisterForCleanup(this, internal_);
  }
  return *this;
}

#if defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)
ListResult::ListResult(ListResult&& other) : internal_(other.internal_) {
  other.internal_ = nullptr;
  if (internal_) {
    // The new public object (this) takes ownership of the PIMPL.
    // Unregister the old public object (other) from cleanup.
    internal::ListResultInternalCommon::UnregisterFromCleanup(&other,
                                                              internal_);
    // Register the new public object (this) for cleanup.
    internal::ListResultInternalCommon::RegisterForCleanup(this, internal_);
  }
}

ListResult& ListResult::operator=(ListResult&& other) {
  if (this == &other) {
    return *this;
  }
  internal::ListResultInternalCommon::DeleteInternal(
      this);  // Clean up current state.

  internal_ = other.internal_;
  other.internal_ = nullptr;

  if (internal_) {
    // Similar to move constructor logic.
    internal::ListResultInternalCommon::UnregisterFromCleanup(&other,
                                                              internal_);
    internal::ListResultInternalCommon::RegisterForCleanup(this, internal_);
  }
  return *this;
}
#endif  // defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)

const std::vector<StorageReference>& ListResult::items() const {
  if (!is_valid()) return s_empty_items_;
  return internal_->items();
}

const std::vector<StorageReference>& ListResult::prefixes() const {
  if (!is_valid()) return s_empty_prefixes_;
  return internal_->prefixes();
}

const std::string& ListResult::page_token() const {
  if (!is_valid()) return s_empty_page_token_;
  return internal_->page_token();
}

bool ListResult::is_valid() const { return internal_ != nullptr; }

}  // namespace storage
}  // namespace firebase
