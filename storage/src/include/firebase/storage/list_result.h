#ifndef FIREBASE_STORAGE_CLIENT_CPP_SRC_INCLUDE_FIREBASE_STORAGE_LIST_RESULT_H_
#define FIREBASE_STORAGE_CLIENT_CPP_SRC_INCLUDE_FIREBASE_STORAGE_LIST_RESULT_H_

#include <string>
#include <vector>

#include "firebase/internal/common.h" // For FIREBASE_DEPRECATED_MSG
#include "firebase/storage/common.h" // For SWIG_STORAGE_EXPORT
#include "firebase/storage/storage_reference.h"
// No longer include cleanup_notifier directly here, it's an internal detail.
// No longer forward declare StorageReference if ListResult is now a class with an internal ptr.

namespace firebase {
namespace storage {

// Forward declaration for internal class
namespace internal {
class ListResultInternal;
}  // namespace internal

/// @brief Results from a list operation.
class SWIG_STORAGE_EXPORT ListResult {
 public:
  /// @brief Default constructor. Creates an invalid ListResult.
  ListResult();

  /// @brief Copy constructor.
  ///
  /// @param[in] other ListResult to copy from.
  ListResult(const ListResult& other);

  /// @brief Move constructor.
  ///
  /// @param[in] other ListResult to move from.
  ListResult(ListResult&& other);

  ~ListResult();

  /// @brief Copy assignment operator.
  ///
  /// @param[in] other ListResult to copy from.
  ///
  /// @return Reference to this ListResult.
  ListResult& operator=(const ListResult& other);

  /// @brief Move assignment operator.
  ///
  /// @param[in] other ListResult to move from.
  ///
  /// @return Reference to this ListResult.
  ListResult& operator=(ListResult&& other);

  /// @brief Gets the individual items (files) found in this result.
  ///
  /// @return Vector of StorageReferences to the items. Will be empty if
  /// no items are found or if the ListResult is invalid.
  const std::vector<StorageReference>& items() const;

  /// @brief Gets the prefixes (directories) found in this result.
  /// These can be used to further "navigate" the storage hierarchy.
  ///
  /// @return Vector of StorageReferences to the prefixes. Will be empty if
  /// no prefixes are found or if the ListResult is invalid.
  const std::vector<StorageReference>& prefixes() const;

  /// @brief Gets the page token for the next page of results.
  ///
  /// If empty, there are no more results.
  ///
  /// @return Page token string.
  const std::string& page_token() const;

  /// @brief Returns true if this ListResult is valid, false otherwise.
  /// An invalid ListResult is one that has not been initialized or has
  /// been moved from.
  bool is_valid() const;

 private:
  friend class StorageReference; // Allows StorageReference to construct ListResult
  friend class internal::StorageReferenceInternal; // Allow internal classes to construct

  // Constructor for internal use, takes ownership of the internal object.
  explicit ListResult(internal::ListResultInternal* internal_list_result);

  // Using firebase::internal::InternalUniquePtr for managing the lifecycle
  // of the internal object, ensuring it's cleaned up properly.
  ::firebase::internal::InternalUniquePtr<internal::ListResultInternal> internal_;

  // Static empty results to return for invalid ListResult objects
  static const std::vector<StorageReference> s_empty_items_;
  static const std::vector<StorageReference> s_empty_prefixes_;
  static const std::string s_empty_page_token_;
};

}  // namespace storage
}  // namespace firebase

#endif  // FIREBASE_STORAGE_CLIENT_CPP_SRC_INCLUDE_FIREBASE_STORAGE_LIST_RESULT_H_
