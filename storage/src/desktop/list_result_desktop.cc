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
    // Copy data from the other instance.
    items_ = other_to_copy_from->items_;
    prefixes_ = other_to_copy_from->prefixes_;
    page_token_ = other_to_copy_from->page_token_;
  }
  // If other_to_copy_from is null, members are default-initialized (empty for
  // stub).
}

// Destructor is default. Members are cleaned up automatically.
// Lifecycle of this PIMPL object is managed by the public ListResult class
// via ListResultInternalCommon static helpers.

// Accessor methods (items(), prefixes(), page_token()) are inline in the
// header.

}  // namespace internal
}  // namespace storage
}  // namespace firebase
