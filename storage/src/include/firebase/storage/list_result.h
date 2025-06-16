// File: storage/src/include/firebase/storage/list_result.h
#ifndef FIREBASE_STORAGE_CLIENT_CPP_SRC_INCLUDE_FIREBASE_STORAGE_LIST_RESULT_H_
#define FIREBASE_STORAGE_CLIENT_CPP_SRC_INCLUDE_FIREBASE_STORAGE_LIST_RESULT_H_

#include <string>
#include <vector>

#include "firebase/internal/common.h" // For FIREBASE_DEPRECATED_MSG, SWIG_STORAGE_EXPORT etc.
#include "firebase/storage/common.h"  // For SWIG_STORAGE_EXPORT (if not from internal/common.h)

// Forward declaration for the PIMPL class.
// The actual definition comes from platform-specific headers.
namespace firebase {
namespace storage {
namespace internal {
class ListResultInternal;
class ListResultInternalCommon; // For friend declaration
class StorageReferenceInternal; // For the private constructor from StorageReference
}  // namespace internal

class StorageReference; // Forward declaration

/// @brief Results from a list operation.
class SWIG_STORAGE_EXPORT ListResult {
 public:
  /// @brief Default constructor. Creates an empty, invalid ListResult.
  /// To get a valid ListResult, call methods like StorageReference::ListAll().
  ListResult();

  /// @brief Copy constructor.
  /// @param[in] other ListResult to copy from.
  ListResult(const ListResult& other);

  /// @brief Copy assignment operator.
  /// @param[in] other ListResult to copy from.
  /// @return Reference to this ListResult.
  ListResult& operator=(const ListResult& other);

#if defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)
  /// @brief Move constructor.
  /// @param[in] other ListResult to move from.
  ListResult(ListResult&& other);

  /// @brief Move assignment operator.
  /// @param[in] other ListResult to move from.
  /// @return Reference to this ListResult.
  ListResult& operator=(ListResult&& other);
#endif  // defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)

  ~ListResult();

  const std::vector<StorageReference>& items() const;
  const std::vector<StorageReference>& prefixes() const;
  const std::string& page_token() const;
  bool is_valid() const;

 private:
  // Allow StorageReference (and its internal part) to construct ListResults
  // with an actual PIMPL object.
  friend class StorageReference;
  friend class internal::StorageReferenceInternal;

  // Allow ListResultInternalCommon to access internal_ for lifecycle management.
  friend class internal::ListResultInternalCommon;

  // Private constructor for creating a ListResult with an existing PIMPL object.
  // Takes ownership of the provided internal_pimpl.
  explicit ListResult(internal::ListResultInternal* internal_pimpl);

  internal::ListResultInternal* internal_; // Raw pointer to PIMPL

  // Static empty results to return for invalid ListResult objects
  static const std::vector<StorageReference> s_empty_items_;
  static const std::vector<StorageReference> s_empty_prefixes_;
  static const std::string s_empty_page_token_;
};

}  // namespace storage
}  // namespace firebase

#endif  // FIREBASE_STORAGE_CLIENT_CPP_SRC_INCLUDE_FIREBASE_STORAGE_LIST_RESULT_H_
