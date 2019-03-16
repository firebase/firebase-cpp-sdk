// Copyright 2017 Google LLC
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

#include "database/src/desktop/data_snapshot_desktop.h"

#include <stddef.h>
#include <string>
#include "app/src/include/firebase/internal/common.h"
#include "app/src/include/firebase/variant.h"
#include "database/src/desktop/database_reference_desktop.h"
#include "database/src/desktop/util_desktop.h"

namespace firebase {
namespace database {
namespace internal {

DataSnapshotInternal::DataSnapshotInternal(DatabaseInternal* database,
                                           const Path& path,
                                           const Variant& data)
    : database_(database), path_(path), data_(data) {
  if (HasVector(data_)) {
    ConvertVectorToMap(&data_);
  }
}

DataSnapshotInternal::DataSnapshotInternal(const DataSnapshotInternal& internal)
    : database_(internal.database_),
      path_(internal.path_),
      data_(internal.data_) {}

DataSnapshotInternal& DataSnapshotInternal::operator=(
    const DataSnapshotInternal& internal) {
  database_ = internal.database_;
  path_ = internal.path_;
  data_ = internal.data_;
  return *this;
}

#if defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)
DataSnapshotInternal::DataSnapshotInternal(DataSnapshotInternal&& internal) {
  database_ = internal.database_;
  data_ = std::move(internal.data_);
  path_ = std::move(internal.path_);
}

DataSnapshotInternal& DataSnapshotInternal::operator=(
    DataSnapshotInternal&& internal) {
  database_ = internal.database_;
  data_ = std::move(internal.data_);
  path_ = std::move(internal.path_);
  return *this;
}
#endif  // defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)

DataSnapshotInternal::~DataSnapshotInternal() {}

bool DataSnapshotInternal::Exists() const { return data_ != Variant::Null(); }

DataSnapshotInternal* DataSnapshotInternal::Child(const char* path) const {
  const Variant* child = GetInternalVariant(&data_, Path(path));
  if (child != nullptr) {
    return new DataSnapshotInternal(database_, path_.GetChild(path), *child);
  }
  return nullptr;
}

std::vector<DataSnapshot> DataSnapshotInternal::GetChildren() {
  std::vector<DataSnapshot> result;
  std::map<Variant, const Variant*> children;
  GetEffectiveChildren(data_, &children);

  for (auto& child : children) {
    assert(child.first.is_string());
    result.push_back(DataSnapshot(new DataSnapshotInternal(
        database_, path_.GetChild(child.first.string_value()), *child.second)));
  }

  return result;
}

size_t DataSnapshotInternal::GetChildrenCount() {
  return CountEffectiveChildren(data_);
}

bool DataSnapshotInternal::HasChildren() {
  return CountEffectiveChildren(data_) != 0;
}

const char* DataSnapshotInternal::GetKey() const { return path_.GetBaseName(); }

std::string DataSnapshotInternal::GetKeyString() const {
  return path_.GetBaseName();
}

Variant DataSnapshotInternal::GetValue() const {
  Variant result = data_;
  PrunePrioritiesAndConvertVector(&result);
  return result;
}

Variant DataSnapshotInternal::GetPriority() {
  auto* priority = GetVariantPriority(data_);
  if (priority) {
    return *priority;
  }
  return Variant::Null();
}

DatabaseReferenceInternal* DataSnapshotInternal::GetReference() const {
  return new DatabaseReferenceInternal(database_, path_);
}

bool DataSnapshotInternal::HasChild(const char* path) const {
  return GetInternalVariant(&data_, Path(path)) != nullptr;
}

bool DataSnapshotInternal::operator==(const DataSnapshotInternal& other) const {
  return database_ == other.database_ && path_ == other.path_ &&
         data_ == other.data_;
}

bool DataSnapshotInternal::operator!=(const DataSnapshotInternal& other) const {
  return !(*this == other);
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
