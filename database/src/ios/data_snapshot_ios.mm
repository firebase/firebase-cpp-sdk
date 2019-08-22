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

#include "database/src/ios/data_snapshot_ios.h"

#include <stddef.h>
#include <string>

#include "app/src/include/firebase/internal/common.h"
#include "app/src/include/firebase/variant.h"
#include "app/src/util_ios.h"
#include "database/src/ios/database_reference_ios.h"

#import "FIRDatabase.h"

namespace firebase {
namespace database {
namespace internal {

using util::IdToVariant;

DataSnapshotInternal::DataSnapshotInternal(DatabaseInternal* database,
                                           UniquePtr<FIRDataSnapshotPointer> impl)
    : database_(database), impl_(std::move(impl)) {}

DataSnapshotInternal::DataSnapshotInternal(const DataSnapshotInternal& other)
    : database_(other.database_) {
  impl_.reset(new FIRDataSnapshotPointer(*other.impl_));
}

DataSnapshotInternal& DataSnapshotInternal::operator=(const DataSnapshotInternal& other) {
  database_ = other.database_;
  impl_.reset(new FIRDataSnapshotPointer(*other.impl_));
  return *this;
}

#if defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)
DataSnapshotInternal::DataSnapshotInternal(DataSnapshotInternal&& other) {
  database_ = std::move(other.database_);
  impl_ = std::move(other.impl_);
  other.impl_ = MakeUnique<FIRDataSnapshotPointer>(nil);
}

DataSnapshotInternal& DataSnapshotInternal::operator=(DataSnapshotInternal&& other) {
  database_ = std::move(other.database_);
  impl_ = std::move(other.impl_);
  other.impl_ = MakeUnique<FIRDataSnapshotPointer>(nil);
  return *this;
}
#endif  // defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)

DataSnapshotInternal::~DataSnapshotInternal() {}

bool DataSnapshotInternal::Exists() const { return [impl() exists] != NO ? true : false; }

DataSnapshotInternal* DataSnapshotInternal::Child(const char* path) const {
  return new DataSnapshotInternal(
      database_, MakeUnique<FIRDataSnapshotPointer>([impl() childSnapshotForPath:@(path)]));
}

std::vector<DataSnapshot> DataSnapshotInternal::GetChildren() {
  std::vector<DataSnapshot> result;
  for (FIRDataSnapshot* child in impl().children) {
    result.push_back(DataSnapshot(
        new DataSnapshotInternal(database_, MakeUnique<FIRDataSnapshotPointer>(child))));
  }
  return result;
}

size_t DataSnapshotInternal::GetChildrenCount() { return impl().childrenCount; }

bool DataSnapshotInternal::HasChildren() { return [impl() hasChildren] != NO ? true : false; }

const char* DataSnapshotInternal::GetKey() const { return [impl().key UTF8String]; }

std::string DataSnapshotInternal::GetKeyString() const {
  return std::string([impl().key UTF8String]);
}

Variant DataSnapshotInternal::GetValue() const { return IdToVariant(impl().value); }

Variant DataSnapshotInternal::GetPriority() { return IdToVariant(impl().priority); }

DatabaseReferenceInternal* DataSnapshotInternal::GetReference() const {
  return new DatabaseReferenceInternal(database_,
                                       MakeUnique<FIRDatabaseReferencePointer>(impl().ref));
}

bool DataSnapshotInternal::HasChild(const char* path) {
  return [impl() hasChild:@(path)] != NO ? true : false;
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
