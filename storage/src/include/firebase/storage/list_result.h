#ifndef FIREBASE_STORAGE_CLIENT_CPP_SRC_INCLUDE_FIREBASE_STORAGE_LIST_RESULT_H_
#define FIREBASE_STORAGE_CLIENT_CPP_SRC_INCLUDE_FIREBASE_STORAGE_LIST_RESULT_H_

#include <string>
#include <vector>

#include "firebase/future.h"
#include "firebase/storage/storage_reference.h"

namespace firebase {
namespace storage {

namespace internal {
class ListResultInternal;
}  // namespace internal

class ListResult {
 public:
  // Constructor that takes items and page_token.
  ListResult(std::vector<StorageReference> items, std::string page_token);

  // Constructor that takes an internal ListResultInternal object.
  ListResult(internal::ListResultInternal* internal);

  // Copy constructor.
  ListResult(const ListResult& other);

  // Copy assignment operator.
  ListResult& operator=(const ListResult& other);

  // Move constructor.
  ListResult(ListResult&& other);

  // Move assignment operator.
  ListResult& operator=(ListResult&& other);

  // Destructor.
  ~ListResult();

  // Gets the items in this list page.
  const std::vector<StorageReference>& items() const;

  // Gets the page token for the next page of results.
  const std::string& page_token() const;

  // Returns true if this ListResult is valid.
  bool is_valid() const;

 private:
  friend class internal::ListResultInternal;

  std::vector<StorageReference> items_;
  std::string page_token_;
  internal::ListResultInternal* internal_;
};

}  // namespace storage
}  // namespace firebase

#endif  // FIREBASE_STORAGE_CLIENT_CPP_SRC_INCLUDE_FIREBASE_STORAGE_LIST_RESULT_H_
