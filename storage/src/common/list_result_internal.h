// Copyright 2019 Google LLC
#ifndef FIREBASE_STORAGE_SRC_COMMON_LIST_RESULT_INTERNAL_H_
#define FIREBASE_STORAGE_SRC_COMMON_LIST_RESULT_INTERNAL_H_

#include <string>
#include <vector>

#include "firebase/storage/list_result.h"
#include "firebase/storage/storage_reference.h"

namespace firebase::storage::internal {

class StorageListResultInternal {
 public:
  StorageListResultInternal() = default;
  StorageListResultInternal(const std::vector<StorageReference>& prefixes,
                            const std::vector<StorageReference>& items,
                            const std::string& next_page_token)
      : prefixes_(prefixes), items_(items), next_page_token_(next_page_token) {}

  static StorageListResult AsStorageListResult(
      StorageListResultInternal* internal) {
    StorageListResult result;
    result.internal_ = internal;
    return result;
  }

  std::vector<StorageReference> prefixes_;
  std::vector<StorageReference> items_;
  std::string next_page_token_;
};

}  // namespace firebase::storage::internal

#endif  // FIREBASE_STORAGE_SRC_COMMON_LIST_RESULT_INTERNAL_H_
