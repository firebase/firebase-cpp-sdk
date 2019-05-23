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
#include "database/src/common/query_spec.h"
#include "database/src/desktop/database_reference_desktop.h"
#include "database/src/desktop/query_params_comparator.h"
#include "database/src/desktop/util_desktop.h"

namespace firebase {
namespace database {
namespace internal {

DataSnapshotInternal::DataSnapshotInternal(DatabaseInternal* database,
                                           const Variant& data,
                                           const QuerySpec& query_spec)
    : database_(database), data_(data), query_spec_(query_spec) {
  if (HasVector(data_)) {
    ConvertVectorToMap(&data_);
  }
}

DataSnapshotInternal::DataSnapshotInternal(const DataSnapshotInternal& internal)
    : database_(internal.database_),
      data_(internal.data_),
      query_spec_(internal.query_spec_) {}

DataSnapshotInternal& DataSnapshotInternal::operator=(
    const DataSnapshotInternal& internal) {
  database_ = internal.database_;
  data_ = internal.data_;
  query_spec_ = internal.query_spec_;
  return *this;
}

#if defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)
DataSnapshotInternal::DataSnapshotInternal(DataSnapshotInternal&& internal) {
  database_ = internal.database_;
  data_ = std::move(internal.data_);
  query_spec_ = std::move(internal.query_spec_);
}

DataSnapshotInternal& DataSnapshotInternal::operator=(
    DataSnapshotInternal&& internal) {
  database_ = internal.database_;
  data_ = std::move(internal.data_);
  query_spec_ = std::move(internal.query_spec_);
  return *this;
}
#endif  // defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)

DataSnapshotInternal::~DataSnapshotInternal() {}

bool DataSnapshotInternal::Exists() const { return data_ != Variant::Null(); }

DataSnapshotInternal* DataSnapshotInternal::Child(const char* path) const {
  const Variant& child = VariantGetChild(&data_, Path(path));
  return new DataSnapshotInternal(database_, child,
                                  QuerySpec(query_spec_.path.GetChild(path)));
}

std::vector<DataSnapshot> DataSnapshotInternal::GetChildren() {
  std::vector<DataSnapshot> result;
  std::map<Variant, const Variant*> children;
  GetEffectiveChildren(data_, &children);

  for (auto& child : children) {
    assert(child.first.is_string());
    result.push_back(DataSnapshot(new DataSnapshotInternal(
        database_, *child.second,
        QuerySpec(query_spec_.path.GetChild(child.first.string_value())))));
  }

  QueryParamsComparator cmp(&query_spec_.params);
  std::sort(result.begin(), result.end(),
            [&cmp](const DataSnapshot& lhs, const DataSnapshot& rhs) {
              return cmp.Compare(lhs.internal_->path().c_str(),
                                 lhs.internal_->data_,
                                 rhs.internal_->path().c_str(),
                                 rhs.internal_->data_) < 0;
            });

  return result;
}

size_t DataSnapshotInternal::GetChildrenCount() {
  return CountEffectiveChildren(data_);
}

bool DataSnapshotInternal::HasChildren() {
  return CountEffectiveChildren(data_) != 0;
}

const char* DataSnapshotInternal::GetKey() const {
  return query_spec_.path.GetBaseName();
}

std::string DataSnapshotInternal::GetKeyString() const {
  return query_spec_.path.GetBaseName();
}

Variant DataSnapshotInternal::GetValue() const {
  Variant result = data_;
  PrunePrioritiesAndConvertVector(&result);
  return result;
}

Variant DataSnapshotInternal::GetPriority() {
  return GetVariantPriority(data_);
}

DatabaseReferenceInternal* DataSnapshotInternal::GetReference() const {
  return new DatabaseReferenceInternal(database_, query_spec_.path);
}

bool DataSnapshotInternal::HasChild(const char* path) const {
  return !VariantIsEmpty(VariantGetChild(&data_, Path(path)));
}

bool DataSnapshotInternal::operator==(const DataSnapshotInternal& other) const {
  return database_ == other.database_ && data_ == other.data_ &&
         query_spec_ == other.query_spec_;
}

bool DataSnapshotInternal::operator!=(const DataSnapshotInternal& other) const {
  return !(*this == other);
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
