// File: storage/src/android/list_result_android.cc
#include "storage/src/android/list_result_android.h"

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
}

}  // namespace internal
}  // namespace storage
}  // namespace firebase
