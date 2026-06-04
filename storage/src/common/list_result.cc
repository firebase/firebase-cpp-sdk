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

#include "firebase/storage/list_result.h"

#include "list_result_internal.h"

namespace firebase {
namespace storage {

StorageListResult::StorageListResult() : internal_(nullptr) {}

StorageListResult::~StorageListResult() {
  delete internal_;
  internal_ = nullptr;
}

StorageListResult::StorageListResult(const StorageListResult& other)
    : internal_(other.internal_
                    ? new internal::StorageListResultInternal(*other.internal_)
                    : nullptr) {}

StorageListResult& StorageListResult::operator=(
    const StorageListResult& other) {
  if (this != &other) {
    delete internal_;
    internal_ = other.internal_
                    ? new internal::StorageListResultInternal(*other.internal_)
                    : nullptr;
  }
  return *this;
}

#if defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)
StorageListResult::StorageListResult(StorageListResult&& other)
    : internal_(other.internal_) {
  other.internal_ = nullptr;
}

StorageListResult& StorageListResult::operator=(StorageListResult&& other) {
  if (this != &other) {
    delete internal_;
    internal_ = other.internal_;
    other.internal_ = nullptr;
  }
  return *this;
}
#endif  // defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)

const std::vector<StorageReference>& StorageListResult::prefixes() const {
  static const std::vector<StorageReference> empty_vector;
  return internal_ ? internal_->prefixes_ : empty_vector;
}

const std::vector<StorageReference>& StorageListResult::items() const {
  static const std::vector<StorageReference> empty_vector;
  return internal_ ? internal_->items_ : empty_vector;
}

const std::string& StorageListResult::next_page_token() const {
  static const std::string empty_string;
  return internal_ ? internal_->next_page_token_ : empty_string;
}

bool StorageListResult::is_valid() const { return internal_ != nullptr; }

}  // namespace storage
}  // namespace firebase
