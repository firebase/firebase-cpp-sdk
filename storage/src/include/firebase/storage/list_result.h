#ifndef FIREBASE_STORAGE_CLIENT_CPP_SRC_INCLUDE_FIREBASE_STORAGE_LIST_RESULT_H_
#define FIREBASE_STORAGE_CLIENT_CPP_SRC_INCLUDE_FIREBASE_STORAGE_LIST_RESULT_H_

#include <string>
#include <vector>

#include "firebase/storage/storage_reference.h"
#include "app/src/cleanup_notifier.h" // Required for CleanupNotifier

namespace firebase {
namespace storage {

// Forward declaration for StorageReference to break circular dependency,
// if StorageReference includes ListResult.
class StorageReference;

struct ListResult {
  ListResult() : items(), prefixes(), page_token() {}

  std::vector<StorageReference> items;
  std::vector<StorageReference> prefixes;
  std::string page_token;

  // If ListResult itself needs to be managed by CleanupNotifier,
  // it would typically be part of a class that inherits from
  // firebase::internal::InternalCleanupNotifierInterface.
  // For a simple struct like this, direct cleanup management might not be needed
  // unless it holds resources that require explicit cleanup.
  // However, if it's part of a Future result, the Future's lifecycle
  // will be managed.
  // For now, we'll keep it simple as per stub requirements.
  // If CleanupNotifier is to be used directly with ListResult instances,
  // this struct might need to be refactored into a class.
  // For now, assuming it's a plain data object.
};

}  // namespace storage
}  // namespace firebase

#endif  // FIREBASE_STORAGE_CLIENT_CPP_SRC_INCLUDE_FIREBASE_STORAGE_LIST_RESULT_H_
