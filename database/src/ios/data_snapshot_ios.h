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

#ifndef FIREBASE_DATABASE_CLIENT_CPP_SRC_IOS_DATA_SNAPSHOT_IOS_H_
#define FIREBASE_DATABASE_CLIENT_CPP_SRC_IOS_DATA_SNAPSHOT_IOS_H_

#include <stddef.h>
#include <string>

#include "app/memory/unique_ptr.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/include/firebase/variant.h"
#include "app/src/util_ios.h"
#include "database/src/include/firebase/database/common.h"
#include "database/src/include/firebase/database/data_snapshot.h"

#ifdef __OBJC__
#import "FIRDatabase.h"
#endif  // __OBJC__

namespace firebase {
namespace database {
class Database;
namespace internal {
class DatabaseInternal;
class DatabaseReferenceInternal;

// This defines the class FIRDataSnapshotPointer, which is a C++-compatible
// wrapper around the FIRDataSnapshot Obj-C class.
OBJ_C_PTR_WRAPPER(FIRDataSnapshot);

#pragma clang assume_nonnull begin

// The iOS implementation of the DataSnapshot, which contains data from a
// Firebase Database location.
class DataSnapshotInternal {
 public:
  DataSnapshotInternal(DatabaseInternal* database,
                       UniquePtr<FIRDataSnapshotPointer> impl);

  DataSnapshotInternal(const DataSnapshotInternal& snapshot);

  DataSnapshotInternal& operator=(const DataSnapshotInternal& snapshot);

#if defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)
  DataSnapshotInternal(DataSnapshotInternal&& snapshot);

  DataSnapshotInternal& operator=(DataSnapshotInternal&& snapshot);
#endif  // defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)

  ~DataSnapshotInternal();

  // Returns true if the data is non-empty.
  bool Exists() const;

  // Get a DataSnapshot for the location at the specified relative path.
  //
  // The returned pointer should be passed to a DataSnapshot for lifetime
  // management.
  DataSnapshotInternal* Child(const char* path) const;

  // Get all the immediate children of this location.
  std::vector<DataSnapshot> GetChildren();

  // Get the number of children of this location.
  size_t GetChildrenCount();

  // Does this DataSnapshot have any children at all?
  bool HasChildren();

  // Get the key name of the source location of this snapshot.
  const char* GetKey() const;

  // Get the key name of the source location of this snapshot.
  std::string GetKeyString() const;

  // Get the value of the data contained in this snapshot.
  Variant GetValue() const;

  // Get the priority of the data contained in this snapshot.
  Variant GetPriority();

  // Obtain a DatabaseReference to the source location for this snapshot.
  //
  // The returned pointer should be passed to a DatabaseReference for lifetime
  // management.
  DatabaseReferenceInternal* GetReference() const;

  // Does this DataSnapshot have data at a particular location?
  bool HasChild(const char* path);

  /// Get the absolute URL of this data snapshot.
  std::string GetUrl() const;

  DatabaseInternal* database_internal() const { return database_; }

  // Special method to create an invalid DataSnapshot, because
  // DataSnapshot's constructor is private.
  static DataSnapshot GetInvalidDataSnapshot() { return DataSnapshot(nullptr); }

 private:
#ifdef __OBJC__
  FIRDataSnapshot* impl() const { return impl_->get(); }
#endif  // __OBJC__

  DatabaseInternal* database_;

  // Object lifetime managed by Objective C ARC.
  UniquePtr<FIRDataSnapshotPointer> impl_;
};

#pragma clang assume_nonnull end

}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_CLIENT_CPP_SRC_IOS_DATA_SNAPSHOT_IOS_H_
