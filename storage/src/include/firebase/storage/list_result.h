// Copyright 2019 Google LLC
#ifndef FIREBASE_STORAGE_SRC_INCLUDE_FIREBASE_STORAGE_LIST_RESULT_H_
#define FIREBASE_STORAGE_SRC_INCLUDE_FIREBASE_STORAGE_LIST_RESULT_H_

#include <string>
#include <vector>

#include "firebase/internal/common.h"

namespace firebase {
namespace storage {

namespace internal {
class StorageListResultInternal;
}  // namespace internal

class StorageReference;

class StorageListResult {
 public:
  StorageListResult();
  ~StorageListResult();
  StorageListResult(const StorageListResult& other);
  StorageListResult& operator=(const StorageListResult& other);

#if defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)
  StorageListResult(StorageListResult&& other);
  StorageListResult& operator=(StorageListResult&& other);
#endif

  const std::vector<StorageReference>& prefixes() const;
  const std::vector<StorageReference>& items() const;
  const std::string& next_page_token() const;
  bool is_valid() const;

#ifndef SWIG
  std::string ToString() const;
  bool operator==(const StorageListResult& other) const;
  bool operator!=(const StorageListResult& other) const {
    return !(*this == other);
  }
#endif  // SWIG

 private:
  friend class internal::StorageListResultInternal;
  internal::StorageListResultInternal* internal_;
};

}  // namespace storage
}  // namespace firebase

#endif  // FIREBASE_STORAGE_SRC_INCLUDE_FIREBASE_STORAGE_LIST_RESULT_H_
