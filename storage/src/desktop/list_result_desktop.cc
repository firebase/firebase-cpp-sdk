// File: storage/src/desktop/list_result_desktop.cc
#include "storage/src/desktop/list_result_desktop.h"

// Includes for StorageReferenceInternal if needed for full type.
// #include "storage/src/desktop/storage_reference_desktop.h"

namespace firebase {
namespace storage {
namespace internal {

ListResultInternal::ListResultInternal(
    StorageReferenceInternal* platform_sri,
    const ListResultInternal* other_to_copy_from)
    : platform_sri_(platform_sri) {
  if (other_to_copy_from) {
    // This is a copy operation. For stubs, data is empty anyway.
    items_ = other_to_copy_from->items_;
    prefixes_ = other_to_copy_from->prefixes_;
    page_token_ = other_to_copy_from->page_token_;
  } else {
    // Default construction: items_, prefixes_ are default-constructed (empty).
    // page_token_ is default-constructed (empty).
  }
}

// Note: No destructor implementation needed here if it does nothing,
// as members like vectors and strings clean themselves up.
// Cleanup of this object itself is handled by ListResultInternalCommon::DeleteInternalPimpl.

// items(), prefixes(), page_token() are inline in header for stubs.

}  // namespace internal
}  // namespace storage
}  // namespace firebase
