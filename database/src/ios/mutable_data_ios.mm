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

MutableDataInternal::MutableDataInternal(DatabaseInternal* database,
                                         UniquePtr<FIRMutableDataPointer> impl)
    : db_(database), impl_(std::move(impl)) {}

MutableDataInternal::~MutableDataInternal() {}

MutableDataInternal* MutableDataInternal::Clone() {
  return new MutableDataInternal(db_, MakeUnique<FIRMutableDataPointer>(impl()));
}

MutableDataInternal* MutableDataInternal::Child(const char* path) {
  return new MutableDataInternal(
      db_, MakeUnique<FIRMutableDataPointer>([impl() childDataByAppendingPath:@(path)]));
}

std::vector<MutableData> MutableDataInternal::GetChildren() {
  std::vector<MutableData> result;
  result.reserve(GetChildrenCount());
  for (FIRMutableData* child in impl().children) {
    result.push_back(
        MutableData(new MutableDataInternal(db_, MakeUnique<FIRMutableDataPointer>(child))));
  }
  return result;
}

size_t MutableDataInternal::GetChildrenCount() { return static_cast<size_t>(impl().childrenCount); }

const char* MutableDataInternal::GetKey() const { return [impl().key UTF8String]; }

std::string MutableDataInternal::GetKeyString() const {
  const char* key = [impl().key UTF8String];
  return std::string(key ? key : "");
}

Variant MutableDataInternal::GetValue() const { return IdToVariant(impl().value); }

Variant MutableDataInternal::GetPriority() { return IdToVariant(impl().priority); }

bool MutableDataInternal::HasChild(const char* path) const {
  return [impl() hasChildAtPath:@(path)] != NO ? true : false;
}

void MutableDataInternal::SetValue(Variant value) { impl().value = VariantToId(value); }

void MutableDataInternal::SetPriority(Variant priority) {
  if (!IsValidPriority(priority)) {
    db_->logger()->LogError(kErrorMsgInvalidVariantForPriority);
  } else {
    impl().priority = VariantToId(priority);
  }
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
