// File: storage/src/common/list_result.cc

#include "firebase/storage/list_result.h"       // For ListResult public class
#include "app/src/include/firebase/internal/platform.h" // For FIREBASE_PLATFORM defines
#include "app/src/cleanup_notifier.h"           // For CleanupNotifier
#include "app/src/log.h"                        // For LogDebug, LogWarning
#include "firebase/storage/storage_reference.h" // For StorageReference (used by ListResult members)

// Platform-specific headers that define internal::ListResultInternal (the PIMPL class)
// and internal::StorageInternal (for CleanupNotifier context).
#if FIREBASE_PLATFORM_ANDROID
#include "storage/src/android/list_result_android.h"
#include "storage/src/android/storage_internal_android.h"
// It's assumed storage_reference_android.h is included by list_result_android.h if needed for StorageReferenceInternal type
#elif FIREBASE_PLATFORM_IOS || FIREBASE_PLATFORM_TVOS
#include "storage/src/ios/list_result_ios.h"
#include "storage/src/ios/storage_internal_ios.h"
// It's assumed storage_reference_ios.h is included by list_result_ios.h if needed for StorageReferenceInternal type
#else // Desktop
#include "storage/src/desktop/list_result_desktop.h"
#include "storage/src/desktop/storage_internal_desktop.h"
// It's assumed storage_reference_desktop.h is included by list_result_desktop.h if needed for StorageReferenceInternal type
#endif


namespace firebase {
namespace storage {

// Forward declaration of the PIMPL class.
// The actual definition comes from one of the platform headers included above.
namespace internal {
class ListResultInternal;
// StorageReferenceInternal is needed by ListResultInternal constructor,
// and StorageInternal by ListResultInternalCommon helpers.
// These should be defined by the platform headers like storage_internal_*.h and list_result_*.h
class StorageReferenceInternal;
class StorageInternal;
}  // namespace internal

namespace internal {

// ListResultInternalCommon: Provides static helper methods for managing the
// lifecycle of the ListResultInternal PIMPL object, mirroring the pattern
// used by MetadataInternalCommon for Metadata.
class ListResultInternalCommon {
 public:
  static StorageInternal* GetStorageInternalContext(
      ListResultInternal* pimpl_obj) {
    if (!pimpl_obj) {
        // LogDebug("GetStorageInternalContext: PIMPL object is null.");
        return nullptr;
    }
    // This relies on ListResultInternal (platform specific) having associated_storage_internal()
    StorageInternal* storage_ctx = pimpl_obj->associated_storage_internal();
    if (storage_ctx == nullptr) {
        LogWarning("ListResultInternal %p has no associated StorageInternal for cleanup.", pimpl_obj);
    }
    return storage_ctx;
  }

  static void CleanupPublicListResultObject(void* public_obj_void) {
    ListResult* public_obj = reinterpret_cast<ListResult*>(public_obj_void);
    if (public_obj) {
        LogDebug("CleanupPublicListResultObject called for ListResult %p via app cleanup.", public_obj);
        DeleteInternalPimpl(public_obj);
    } else {
        LogWarning("CleanupPublicListResultObject called with null object.");
    }
  }

  static void RegisterForCleanup(ListResult* public_obj,
                                 ListResultInternal* pimpl_obj) {
    FIREBASE_ASSERT(public_obj != nullptr);
    if (!pimpl_obj) {
        LogDebug("Not registering ListResult %p for cleanup: PIMPL object is null.", public_obj);
        return;
    }
    StorageInternal* storage_ctx = GetStorageInternalContext(pimpl_obj);
    if (storage_ctx) {
      storage_ctx->cleanup().RegisterObject(public_obj, CleanupPublicListResultObject);
      LogDebug("ListResult %p (PIMPL %p) registered for cleanup with StorageInternal %p.",
               public_obj, pimpl_obj, storage_ctx);
    } else {
      LogWarning("Could not register ListResult %p for cleanup: no StorageInternal context from PIMPL %p.",
                 public_obj, pimpl_obj);
    }
  }

  static void UnregisterFromCleanup(ListResult* public_obj,
                                    ListResultInternal* pimpl_obj) {
    FIREBASE_ASSERT(public_obj != nullptr);
    if (!pimpl_obj) {
        LogDebug("Not unregistering ListResult %p: PIMPL object for context is null.", public_obj);
        return;
    }
    StorageInternal* storage_ctx = GetStorageInternalContext(pimpl_obj);
    if (storage_ctx) {
      storage_ctx->cleanup().UnregisterObject(public_obj);
      LogDebug("ListResult %p (associated with PIMPL %p) unregistered from cleanup from StorageInternal %p.",
               public_obj, pimpl_obj, storage_ctx);
    } else {
        LogWarning("Could not unregister ListResult %p: no StorageInternal context from PIMPL %p.", public_obj, pimpl_obj);
    }
  }

  static void DeleteInternalPimpl(ListResult* public_obj) {
    FIREBASE_ASSERT(public_obj != nullptr);
    if (!public_obj->internal_) {
      // LogDebug("DeleteInternalPimpl called for ListResult %p, but internal_ is already null.", public_obj);
      return;
    }
    ListResultInternal* pimpl_to_delete = public_obj->internal_;
    // LogDebug("ListResult %p: Preparing to delete PIMPL %p.", public_obj, pimpl_to_delete);
    UnregisterFromCleanup(public_obj, pimpl_to_delete);
    public_obj->internal_ = nullptr;
    delete pimpl_to_delete;
    // LogDebug("PIMPL object %p deleted for ListResult %p.", pimpl_to_delete, public_obj);
  }
};

}  // namespace internal

// --- Public ListResult Method Implementations ---

const std::vector<StorageReference> ListResult::s_empty_items_;
const std::vector<StorageReference> ListResult::s_empty_prefixes_;
const std::string ListResult::s_empty_page_token_;

ListResult::ListResult() : internal_(nullptr) {
  LogDebug("ListResult %p default constructed (invalid).", this);
}

ListResult::ListResult(internal::ListResultInternal* internal_pimpl)
    : internal_(internal_pimpl) {
  if (internal_) {
    internal::ListResultInternalCommon::RegisterForCleanup(this, internal_);
  } else {
    LogWarning("ListResult %p constructed with null PIMPL.", this);
  }
}

ListResult::~ListResult() {
  LogDebug("ListResult %p destructor, deleting PIMPL %p.", this, internal_);
  internal::ListResultInternalCommon::DeleteInternalPimpl(this);
}

ListResult::ListResult(const ListResult& other) : internal_(nullptr) {
  if (other.internal_) {
    internal::StorageReferenceInternal* sri_context =
        other.internal_->storage_reference_internal();
    // The second argument to ListResultInternal constructor is other.internal_ (for copying data)
    internal_ = new internal::ListResultInternal(sri_context, other.internal_);
    internal::ListResultInternalCommon::RegisterForCleanup(this, internal_);
     LogDebug("ListResult %p copy constructed from %p (PIMPL %p copied to new PIMPL %p).",
             this, &other, other.internal_, internal_);
  } else {
    LogDebug("ListResult %p copy constructed from invalid ListResult %p.", this, &other);
  }
}

ListResult& ListResult::operator=(const ListResult& other) {
  if (this == &other) {
    return *this;
  }
  internal::ListResultInternalCommon::DeleteInternalPimpl(this); // Clean up current

  if (other.internal_) {
    internal::StorageReferenceInternal* sri_context =
        other.internal_->storage_reference_internal();
    internal_ = new internal::ListResultInternal(sri_context, other.internal_);
    internal::ListResultInternalCommon::RegisterForCleanup(this, internal_);
    LogDebug("ListResult %p copy assigned from %p (PIMPL %p copied to new PIMPL %p).",
             this, &other, other.internal_, internal_);
  } else {
    LogDebug("ListResult %p copy assigned from invalid ListResult %p.", this, &other);
    // internal_ is already nullptr from DeleteInternalPimpl
  }
  return *this;
}

#if defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)
ListResult::ListResult(ListResult&& other) : internal_(other.internal_) {
  other.internal_ = nullptr; // Other no longer owns the PIMPL.
  if (internal_) {
    // New public object 'this' takes ownership. Unregister 'other', register 'this'.
    // Pass 'internal_' (which is the original other.internal_) as context for unregistering 'other'.
    internal::ListResultInternalCommon::UnregisterFromCleanup(&other, internal_);
    internal::ListResultInternalCommon::RegisterForCleanup(this, internal_);
    LogDebug("ListResult %p move constructed from %p (PIMPL %p transferred).",
             this, &other, internal_);
  } else {
    LogDebug("ListResult %p move constructed from invalid ListResult %p.", this, &other);
  }
}

ListResult& ListResult::operator=(ListResult&& other) {
  if (this == &other) {
    return *this;
  }
  internal::ListResultInternalCommon::DeleteInternalPimpl(this); // Clean up current

  internal_ = other.internal_; // Take ownership of other's PIMPL.
  other.internal_ = nullptr;   // Other no longer owns it.

  if (internal_) {
    // Similar to move constructor: unregister 'other', register 'this'.
    internal::ListResultInternalCommon::UnregisterFromCleanup(&other, internal_);
    internal::ListResultInternalCommon::RegisterForCleanup(this, internal_);
     LogDebug("ListResult %p move assigned from %p (PIMPL %p transferred).",
             this, &other, internal_);
  } else {
    LogDebug("ListResult %p move assigned from invalid ListResult %p.", this, &other);
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
