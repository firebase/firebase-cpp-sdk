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

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "app/src/include/firebase/internal/common.h"
#include "app/src/include/firebase/variant.h"
#include "app/src/util_ios.h"
#include "database/src/ios/database_reference_ios.h"

#import "FIRDatabase.h"

namespace firebase {
namespace database {
namespace internal {

using util::IdToVariant;

DataSnapshotInternal::DataSnapshotInternal(
    DatabaseInternal* database, std::unique_ptr<FIRDataSnapshotPointer> impl)
    : database_(database), impl_(std::move(impl)) {
  const char* key = impl_ && impl_->get() ? [impl_->get().key UTF8String] : nullptr;
  cached_key_ = key ? key : "";
}

DataSnapshotInternal::DataSnapshotInternal(const DataSnapshotInternal& other)
    : database_(other.database_),
      impl_(std::make_unique<FIRDataSnapshotPointer>(other.impl())),
      cached_key_(other.cached_key_) {}

DataSnapshotInternal& DataSnapshotInternal::operator=(
    const DataSnapshotInternal& other) {
  if (&other == this) return *this;
  database_ = other.database_;
  impl_ = std::make_unique<FIRDataSnapshotPointer>(other.impl());
  cached_key_ = other.cached_key_;
  return *this;
}

DataSnapshotInternal::DataSnapshotInternal(
    DataSnapshotInternal&& other) noexcept
    : database_(other.database_),
      impl_(std::move(other.impl_)),
      cached_key_(std::move(other.cached_key_)) {
  other.database_ = nullptr;
}

DataSnapshotInternal& DataSnapshotInternal::operator=(
    DataSnapshotInternal&& other) noexcept {
  if (&other == this) return *this;
  database_ = other.database_;
  other.database_ = nullptr;
  impl_ = std::move(other.impl_);
  cached_key_ = std::move(other.cached_key_);
  return *this;
}

DataSnapshotInternal::~DataSnapshotInternal() = default;

bool DataSnapshotInternal::Exists() const { return [impl() exists] != NO; }

DataSnapshotInternal* DataSnapshotInternal::Child(const char* path) const {
  if (path == nullptr || !impl()) return nullptr;
  @try {
    NSString* path_str = @(path);
    if (!path_str) return nullptr;
    FIRDataSnapshot* child_snapshot = [impl() childSnapshotForPath:path_str];
    if (!child_snapshot) return nullptr;
    return new DataSnapshotInternal(
        database_, std::make_unique<FIRDataSnapshotPointer>(child_snapshot));
  } @catch (NSException* exception) {
    return nullptr;
  }
}

std::vector<DataSnapshot> DataSnapshotInternal::GetChildren() const {
  std::vector<DataSnapshot> result;
  if (!impl()) return result;
  result.reserve(GetChildrenCount());
  for (FIRDataSnapshot* child in impl().children) {
    result.push_back(DataSnapshot(new DataSnapshotInternal(
        database_, std::make_unique<FIRDataSnapshotPointer>(child))));
  }
  return result;
}

size_t DataSnapshotInternal::GetChildrenCount() const {
  return impl() ? static_cast<size_t>(impl().childrenCount) : 0;
}

bool DataSnapshotInternal::HasChildren() const {
  return [impl() hasChildren] != NO;
}

const char* DataSnapshotInternal::GetKey() const {
  return cached_key_.c_str();
}

std::string DataSnapshotInternal::GetKeyString() const {
  return cached_key_;
}

Variant DataSnapshotInternal::GetValue() const {
  return IdToVariant(impl().value);
}

Variant DataSnapshotInternal::GetPriority() const {
  return IdToVariant(impl().priority);
}

DatabaseReferenceInternal* DataSnapshotInternal::GetReference() const {
  if (!database_ || !impl()) return nullptr;
  return new DatabaseReferenceInternal(
      database_, std::make_unique<FIRDatabaseReferencePointer>(impl().ref));
}

bool DataSnapshotInternal::HasChild(const char* path) const {
  if (path == nullptr || !impl()) return false;
  @try {
    NSString* path_str = @(path);
    if (!path_str) return false;
    return [impl() hasChild:path_str] != NO;
  } @catch (NSException* exception) {
    return false;
  }
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
