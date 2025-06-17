// File: storage/src/include/firebase/storage/list_result.h
#ifndef FIREBASE_STORAGE_CLIENT_CPP_SRC_INCLUDE_FIREBASE_STORAGE_LIST_RESULT_H_
#define FIREBASE_STORAGE_CLIENT_CPP_SRC_INCLUDE_FIREBASE_STORAGE_LIST_RESULT_H_

#include <string>
#include <vector>

#include "firebase/internal/common.h"
#include "firebase/storage/common.h"

namespace firebase {
namespace storage {

// Forward declarations for internal classes.
namespace internal {
class ListResultInternal;
class ListResultInternalCommon;
class StorageReferenceInternal;
}  // namespace internal

class StorageReference; // Forward declaration

/// @brief Holds the results of a list operation from StorageReference::List()
/// or StorageReference::ListAll().
///
/// This class provides access to the items (files) and prefixes (directories)
/// found under a given StorageReference, as well as a page token for pagination
/// if the results are not complete.
class ListResult {
 public:
  /// @brief Default constructor. Creates an empty and invalid ListResult.
  ///
  /// A valid ListResult is typically obtained from the Future returned by
  /// StorageReference::List() or StorageReference::ListAll().
  ListResult();

  /// @brief Copy constructor.
  /// @param[in] other ListResult to copy the contents from.
  ListResult(const ListResult& other);

  /// @brief Copy assignment operator.
  /// @param[in] other ListResult to copy the contents from.
  /// @return Reference to this ListResult.
  ListResult& operator=(const ListResult& other);

#if defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)
  /// @brief Move constructor.
  /// @param[in] other ListResult to move the contents from. `other` is left in
  /// a valid but unspecified state.
  ListResult(ListResult&& other);

  /// @brief Move assignment operator.
  /// @param[in] other ListResult to move the contents from. `other` is left in
  /// a valid but unspecified state.
  /// @return Reference to this ListResult.
  ListResult& operator=(ListResult&& other);
#endif  // defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)

  /// @brief Destructor.
  ~ListResult();

  /// @brief Gets the individual items (files) found in this result.
  ///
  /// @return Vector of StorageReferences to the items. Will be an empty
  /// vector if no items are found or if the ListResult is invalid.
  const std::vector<StorageReference>& items() const;

  /// @brief Gets the prefixes (directories) found in this result.
  /// These can be used to further "navigate" the storage hierarchy by calling
  /// List or ListAll on them.
  ///
  /// @return Vector of StorageReferences to the prefixes. Will be an empty
  /// vector if no prefixes are found or if the ListResult is invalid.
  const std::vector<StorageReference>& prefixes() const;

  /// @brief Gets the page token for the next page of results.
  ///
  /// If the string is empty, it indicates that there are no more results
  /// for the current list operation. This token can be passed to
  /// StorageReference::List() to retrieve the next page.
  ///
  /// @return Page token string. Empty if no more results or if the
  /// ListResult is invalid.
  const std::string& page_token() const;

  /// @brief Returns true if this ListResult object is valid, false otherwise.
  ///
  /// An invalid ListResult is typically one that was default-constructed
  /// and not subsequently assigned a valid result from a list operation,
  /// or one that has been moved from. Operations on an invalid ListResult
  /// will typically return default values (e.g., empty vectors or strings).
  ///
  /// @return true if this ListResult is valid and represents a result from
  /// a list operation (even if that result is empty); false otherwise.
  bool is_valid() const;

 private:
  friend class StorageReference;
  friend class internal::StorageReferenceInternal;
  friend class internal::ListResultInternalCommon; // Manages lifecycle of internal_

  // Private constructor for creating a ListResult with an existing PIMPL object.
  // Takes ownership of the provided internal_pimpl.
  explicit ListResult(internal::ListResultInternal* internal_pimpl);

  internal::ListResultInternal* internal_; // Pointer to the internal implementation.

  // Static empty results to return for invalid ListResult objects.
  static const std::vector<StorageReference> s_empty_items_;
  static const std::vector<StorageReference> s_empty_prefixes_;
  static const std::string s_empty_page_token_;
};

}  // namespace storage
}  // namespace firebase

#endif  // FIREBASE_STORAGE_CLIENT_CPP_SRC_INCLUDE_FIREBASE_STORAGE_LIST_RESULT_H_
