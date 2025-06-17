// File: storage/src/desktop/list_result_desktop.h
#ifndef FIREBASE_STORAGE_CLIENT_CPP_SRC_DESKTOP_LIST_RESULT_DESKTOP_H_
#define FIREBASE_STORAGE_CLIENT_CPP_SRC_DESKTOP_LIST_RESULT_DESKTOP_H_

#include <string>
#include <vector>

#include "firebase/storage/storage_reference.h"
#include "storage/src/desktop/storage_reference_desktop.h"
#include "storage/src/desktop/storage_internal_desktop.h"

namespace firebase {
namespace storage {
namespace internal {

/// Desktop platform's internal implementation for ListResult.
/// This class holds the data for a list operation specific to the desktop platform.
/// Its lifecycle is managed by the public ListResult class via static helpers.
class ListResultInternal {
 public:
  /// Constructor.
  /// @param[in] platform_sri The desktop StorageReferenceInternal this list result
  ///                         is associated with; used for context.
  /// @param[in] other_to_copy_from If not null, initializes this instance by
  ///                               copying data from other_to_copy_from.
  explicit ListResultInternal(
      StorageReferenceInternal* platform_sri,
      const ListResultInternal* other_to_copy_from = nullptr);

  // Destructor is default as members clean themselves up and lifecycle is external.

  const std::vector<StorageReference>& items() const { return items_; }
  const std::vector<StorageReference>& prefixes() const { return prefixes_; }
  const std::string& page_token() const { return page_token_; }

  /// Provides access to the StorageReferenceInternal this object is associated with.
  StorageReferenceInternal* storage_reference_internal() const {
    return platform_sri_;
  }

  /// Provides access to the StorageInternal context, typically for cleanup registration.
  StorageInternal* associated_storage_internal() const {
    return platform_sri_ ? platform_sri_->storage_internal() : nullptr;
  }

 private:
  // Disallow copy assignment; copy construction is handled via the constructor.
  ListResultInternal& operator=(const ListResultInternal&);

  StorageReferenceInternal* platform_sri_; // Associated StorageReference, not owned.

  // Data for list results (stubs are empty).
  std::vector<StorageReference> items_;
  std::vector<StorageReference> prefixes_;
  std::string page_token_;
};

}  // namespace internal
}  // namespace storage
}  // namespace firebase

#endif  // FIREBASE_STORAGE_CLIENT_CPP_SRC_DESKTOP_LIST_RESULT_DESKTOP_H_
