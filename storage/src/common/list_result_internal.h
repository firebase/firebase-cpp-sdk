// Copyright 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef FIREBASE_STORAGE_SRC_COMMON_LIST_RESULT_INTERNAL_H_
#define FIREBASE_STORAGE_SRC_COMMON_LIST_RESULT_INTERNAL_H_

#include <string>
#include <vector>

#include "firebase/storage/storage_reference.h"

namespace firebase {
namespace storage {
namespace internal {

class StorageListResultInternal {
 public:
  StorageListResultInternal() {}

  StorageListResultInternal(const std::vector<StorageReference>& prefixes,
                            const std::vector<StorageReference>& items,
                            const std::string& next_page_token)
      : prefixes_(prefixes),
        items_(items),
        next_page_token_(next_page_token) {}

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

}  // namespace internal
}  // namespace storage
}  // namespace firebase

#endif  // FIREBASE_STORAGE_SRC_COMMON_LIST_RESULT_INTERNAL_H_
