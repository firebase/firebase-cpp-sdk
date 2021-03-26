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

#include "database/src/include/firebase/database/data_snapshot.h"

#include "app/src/assert.h"
#include "app/src/include/firebase/internal/platform.h"
#include "database/src/include/firebase/database.h"
#include "database/src/include/firebase/database/database_reference.h"

// QueryInternal is defined in these 3 files, one implementation for each OS.
#if FIREBASE_PLATFORM_ANDROID
#include "database/src/android/data_snapshot_android.h"
#include "database/src/android/database_android.h"
#elif FIREBASE_PLATFORM_IOS
#include "database/src/ios/data_snapshot_ios.h"
#include "database/src/ios/database_ios.h"
#elif defined(FIREBASE_TARGET_DESKTOP)
#include "database/src/desktop/data_snapshot_desktop.h"
#include "database/src/desktop/database_desktop.h"
#else
#include "database/src/stub/data_snapshot_stub.h"
#include "database/src/stub/database_stub.h"
#endif  // FIREBASE_PLATFORM_ANDROID, FIREBASE_PLATFORM_IOS,
        // defined(FIREBASE_TARGET_DESKTOP)

#include "database/src/common/cleanup.h"

namespace firebase {
namespace database {

using internal::DataSnapshotInternal;

typedef CleanupFn<DataSnapshot, DataSnapshotInternal> CleanupFnDataSnapshot;

template <>
CleanupFnDataSnapshot::CreateInvalidObjectFn
    CleanupFnDataSnapshot::create_invalid_object =
        DataSnapshotInternal::GetInvalidDataSnapshot;

DataSnapshot::DataSnapshot(DataSnapshotInternal* internal)
    : internal_(internal) {
  CleanupFnDataSnapshot::Register(this, internal_);
}

DataSnapshot::DataSnapshot(const DataSnapshot& src)
    : internal_(src.internal_ ? new DataSnapshotInternal(*src.internal_)
                              : nullptr) {
  CleanupFnDataSnapshot::Register(this, internal_);
}

DataSnapshot& DataSnapshot::operator=(const DataSnapshot& src) {
  CleanupFnDataSnapshot::Unregister(this, internal_);
  if (internal_) delete internal_;
  internal_ =
      src.internal_ ? new DataSnapshotInternal(*src.internal_) : nullptr;
  CleanupFnDataSnapshot::Register(this, internal_);
  return *this;
}

#if defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)
DataSnapshot::DataSnapshot(DataSnapshot&& snapshot) {
  CleanupFnDataSnapshot::Unregister(&snapshot, snapshot.internal_);
  internal_ = snapshot.internal_;
  snapshot.internal_ = nullptr;
  CleanupFnDataSnapshot::Register(this, internal_);
}

DataSnapshot& DataSnapshot::operator=(DataSnapshot&& snapshot) {
  CleanupFnDataSnapshot::Unregister(this, internal_);
  CleanupFnDataSnapshot::Unregister(&snapshot, snapshot.internal_);
  if (internal_) delete internal_;
  internal_ = snapshot.internal_;
  snapshot.internal_ = nullptr;
  CleanupFnDataSnapshot::Register(this, internal_);
  return *this;
}
#endif  // defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)

DataSnapshot::~DataSnapshot() {
  CleanupFnDataSnapshot::Unregister(this, internal_);

  delete internal_;
  internal_ = nullptr;
}

bool DataSnapshot::exists() const { return internal_ && internal_->Exists(); }

DataSnapshot DataSnapshot::Child(const char* path) const {
  return internal_ && path ? DataSnapshot(internal_->Child(path))
                           : DataSnapshot(nullptr);
}

DataSnapshot DataSnapshot::Child(const std::string& path) const {
  return Child(path.c_str());
}

std::vector<DataSnapshot> DataSnapshot::children() const {
  return internal_ ? internal_->GetChildren() : std::vector<DataSnapshot>();
}

size_t DataSnapshot::children_count() const {
  return internal_ ? internal_->GetChildrenCount() : 0;
}

bool DataSnapshot::has_children() const {
  return internal_ && internal_->HasChildren();
}

const char* DataSnapshot::key() const {
  return internal_ ? internal_->GetKey() : nullptr;
}

std::string DataSnapshot::key_string() const {
  return internal_ ? internal_->GetKeyString() : std::string();
}

Variant DataSnapshot::value() const {
  return internal_ ? internal_->GetValue() : Variant::Null();
}

Variant DataSnapshot::priority() const {
  return internal_ ? internal_->GetPriority() : Variant::Null();
}

DatabaseReference DataSnapshot::GetReference() const {
  return internal_ ? DatabaseReference(internal_->GetReference())
                   : DatabaseReference(nullptr);
}

bool DataSnapshot::HasChild(const char* path) const {
  return internal_ && path && internal_->HasChild(path);
}

bool DataSnapshot::HasChild(const std::string& path) const {
  return HasChild(path.c_str());
}

bool DataSnapshot::is_valid() const { return internal_ != nullptr; }

}  // namespace database
}  // namespace firebase
