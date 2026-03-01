// File: storage/src/desktop/list_result_desktop.cc
#include "storage/src/desktop/list_result_desktop.h"

namespace firebase {
namespace storage {
namespace internal {

ListResultInternal::ListResultInternal(
    StorageReferenceInternal* platform_sri,
    const ListResultInternal* other_to_copy_from)
    : platform_sri_(platform_sri) {
  if (other_to_copy_from) {
    items_ = other_to_copy_from->items_;
    prefixes_ = other_to_copy_from->prefixes_;
    page_token_ = other_to_copy_from->page_token_;
  }
  // If other_to_copy_from is null, members are default-initialized.
}

// Note: Lifecycle of this PIMPL object is managed by the public ListResult
// class.

}  // namespace internal
}  // namespace storage
}  // namespace firebase
