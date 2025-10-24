// Copyright 2025 Google LLC
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

#include "storage/src/desktop/list_result_desktop.h"

#include "storage/src/desktop/storage_desktop.h"

namespace firebase {
namespace storage {
namespace internal {

ListResultInternal::ListResultInternal(StorageInternal* storage_internal)
    : storage_internal_(storage_internal) {}

ListResultInternal::ListResultInternal(
    StorageInternal* storage_internal,
    const std::vector<StorageReference>& items,
    const std::vector<StorageReference>& prefixes,
    const std::string& page_token)
    : storage_internal_(storage_internal),
      items_stub_(items),
      prefixes_stub_(prefixes),
      page_token_stub_(page_token) {}

ListResultInternal::ListResultInternal(const ListResultInternal& other)
    : storage_internal_(other.storage_internal_),
      items_stub_(other.items_stub_),
      prefixes_stub_(other.prefixes_stub_),
      page_token_stub_(other.page_token_stub_) {}

ListResultInternal& ListResultInternal::operator=(
    const ListResultInternal& other) {
  if (&other == this) {
    return *this;
  }
  storage_internal_ = other.storage_internal_;
  items_stub_ = other.items_stub_;
  prefixes_stub_ = other.prefixes_stub_;
  page_token_stub_ = other.page_token_stub_;
  return *this;
}

// Methods are already stubbed inline in the header.

}  // namespace internal
}  // namespace storage
}  // namespace firebase
