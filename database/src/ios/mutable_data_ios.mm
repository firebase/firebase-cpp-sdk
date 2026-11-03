// Copyright 2016 Google LLC
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

#include "database/src/ios/mutable_data_ios.h"

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "FIRDatabase.h"
#include "app/src/log.h"
#include "app/src/util_ios.h"
#include "database/src/common/database_reference.h"
#include "database/src/include/firebase/database/mutable_data.h"
#include "database/src/ios/database_ios.h"
#include "database/src/ios/database_reference_ios.h"

namespace firebase {
namespace database {
namespace internal {

using util::IdToVariant;
using util::VariantToId;

MutableDataInternal::MutableDataInternal(
    DatabaseInternal* database, std::unique_ptr<FIRMutableDataPointer> impl)
    : db_(database), impl_(std::move(impl)) {
  const char* key =
      impl_ && impl_->get() ? [impl_->get().key UTF8String] : nullptr;
  cached_key_ = key ? key : "";
}

MutableDataInternal::~MutableDataInternal() = default;

MutableDataInternal* MutableDataInternal::Child(const char* path) {
  if (path == nullptr || !impl()) return nullptr;
  @try {
    NSString* path_str = @(path);
    if (!path_str) return nullptr;
    FIRMutableData* child_data = [impl() childDataByAppendingPath:path_str];
    if (!child_data) return nullptr;
    return new MutableDataInternal(
        db_, std::make_unique<FIRMutableDataPointer>(child_data));
  } @catch (NSException* exception) {
    if (db_ && db_->logger()) {
      const char* reason = exception.reason.UTF8String
                               ? exception.reason.UTF8String
                               : "Unknown exception";
      db_->logger()->LogWarning("MutableData::Child('%s') failed: %s", path,
                                reason);
    }
    return nullptr;
  }
}

std::vector<MutableData> MutableDataInternal::GetChildren() {
  std::vector<MutableData> result;
  if (!impl()) return result;
  result.reserve(GetChildrenCount());
  for (FIRMutableData* child in impl().children) {
    result.push_back(MutableData(new MutableDataInternal(
        db_, std::make_unique<FIRMutableDataPointer>(child))));
  }
  return result;
}

size_t MutableDataInternal::GetChildrenCount() const {
  return impl() ? static_cast<size_t>(impl().childrenCount) : 0;
}

const char* MutableDataInternal::GetKey() const {
  return cached_key_.c_str();
}

std::string MutableDataInternal::GetKeyString() const {
  return cached_key_;
}

Variant MutableDataInternal::GetValue() const {
  return IdToVariant(impl().value);
}

Variant MutableDataInternal::GetPriority() const {
  return IdToVariant(impl().priority);
}

bool MutableDataInternal::HasChild(const char* path) const {
  if (path == nullptr || !impl()) return false;
  @try {
    NSString* path_str = @(path);
    if (!path_str) return false;
    return [impl() hasChildAtPath:path_str] != NO;
  } @catch (NSException* exception) {
    if (db_ && db_->logger()) {
      const char* reason = exception.reason.UTF8String
                               ? exception.reason.UTF8String
                               : "Unknown exception";
      db_->logger()->LogWarning("MutableData::HasChild('%s') failed: %s", path,
                                reason);
    }
    return false;
  }
}

void MutableDataInternal::SetValue(const Variant& value) {
  if (!impl()) return;
  @try {
    impl().value = VariantToId(value);
  } @catch (NSException* exception) {
    if (db_ && db_->logger()) {
      const char* reason = exception.reason.UTF8String
                               ? exception.reason.UTF8String
                               : "Unknown exception";
      db_->logger()->LogWarning("MutableData::SetValue() failed: %s", reason);
    }
  }
}

void MutableDataInternal::SetPriority(const Variant& priority) {
  if (!IsValidPriority(priority)) {
    if (db_ && db_->logger()) {
      db_->logger()->LogError(kErrorMsgInvalidVariantForPriority);
    }
    return;
  }
  if (!impl()) return;
  @try {
    impl().priority = VariantToId(priority);
  } @catch (NSException* exception) {
    if (db_ && db_->logger()) {
      const char* reason = exception.reason.UTF8String
                               ? exception.reason.UTF8String
                               : "Unknown exception";
      db_->logger()->LogWarning("MutableData::SetPriority() failed: %s",
                                reason);
    }
  }
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
