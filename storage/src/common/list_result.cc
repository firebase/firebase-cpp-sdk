// Copyright 2024 Google LLC
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

#include "storage/src/include/firebase/storage/list_result.h"
#include "storage/src/common/list_result_internal.h"

namespace firebase {
namespace storage {

StorageListResult::StorageListResult() : internal_(nullptr) {}

StorageListResult::~StorageListResult() {
  delete internal_;
  internal_ = nullptr;
}

StorageListResult::StorageListResult(const StorageListResult& other)
    : internal_(other.internal_ ? new internal::StorageListResultInternal(*other.internal_) : nullptr) {}

StorageListResult& StorageListResult::operator=(const StorageListResult& other) {
  if (this == &other) {
    return *this;
  }
  delete internal_;
  internal_ = other.internal_ ? new internal::StorageListResultInternal(*other.internal_) : nullptr;
  return *this;
}

#if defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)
StorageListResult::StorageListResult(StorageListResult&& other) {
  internal_ = other.internal_;
  other.internal_ = nullptr;
}

StorageListResult& StorageListResult::operator=(StorageListResult&& other) {
  if (this == &other) {
    return *this;
  }
  delete internal_;
  internal_ = other.internal_;
  other.internal_ = nullptr;
  return *this;
}
#endif  // defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)

const std::vector<StorageReference>& StorageListResult::prefixes() const {
  static const std::vector<StorageReference> empty_prefixes;
  return internal_ ? internal_->prefixes() : empty_prefixes;
}

const std::vector<StorageReference>& StorageListResult::items() const {
  static const std::vector<StorageReference> empty_items;
  return internal_ ? internal_->items() : empty_items;
}

const char* StorageListResult::next_page_token() const {
  return internal_ ? internal_->next_page_token() : nullptr;
}

StorageListResult::StorageListResult(internal::StorageListResultInternal* internal)
    : internal_(internal) {}

}  // namespace storage
}  // namespace firebase
