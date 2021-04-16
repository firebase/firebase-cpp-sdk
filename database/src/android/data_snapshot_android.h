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

#ifndef FIREBASE_DATABASE_CLIENT_CPP_SRC_ANDROID_DATA_SNAPSHOT_ANDROID_H_
#define FIREBASE_DATABASE_CLIENT_CPP_SRC_ANDROID_DATA_SNAPSHOT_ANDROID_H_

#include <jni.h>
#include <stddef.h>
#include <string>
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/include/firebase/variant.h"
#include "database/src/include/firebase/database/common.h"
#include "database/src/include/firebase/database/data_snapshot.h"

namespace firebase {
namespace database {

class Database;
class DatabaseInternal;
class DatabaseReferenceInternal;

namespace internal {
class Callbacks;

// The Android implementation of the DataSnapshot, which contains data from a
// Firebase Database location.
class DataSnapshotInternal {
 public:
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
  std::vector<DataSnapshot> GetChildren() const;

  // Get the number of children of this location.
  size_t GetChildrenCount() const;

  // Does this DataSnapshot have any children at all?
  bool HasChildren() const;

  // Get the key name of the source location of this snapshot.
  const char* GetKey();

  // Get the key name of the source location of this snapshot.
  std::string GetKeyString();

  // Get the value of the data contained in this snapshot.
  Variant GetValue() const;

  // Get the priority of the data contained in this snapshot.
  Variant GetPriority() const;

  // Obtain a DatabaseReference to the source location for this snapshot.
  //
  // The returned pointer should be passed to a DatabaseReference for lifetime
  // management.
  DatabaseReferenceInternal* GetReference() const;

  // Does this DataSnapshot have data at a particular location?
  bool HasChild(const char* path) const;

  DatabaseInternal* database_internal() const { return db_; }

  // Special method to create an invalid DataSnapshot, because
  // DataSnapshot's constructor is private.
  static DataSnapshot GetInvalidDataSnapshot() { return DataSnapshot(nullptr); }

 private:
  friend class DatabaseInternal;
  friend class DatabaseReferenceInternal;
  friend class internal::Callbacks;

  static DataSnapshot MakeDataSnapshot(DataSnapshotInternal* ptr) {
    return DataSnapshot(ptr);
  }

  // Creates a new global reference on the jobject, so you can delete your
  // reference after calling this.
  DataSnapshotInternal(DatabaseInternal* database, jobject data_snapshot_obj);

  static bool Initialize(App* app);
  static void Terminate(App* app);

  DatabaseInternal* db_;
  jobject obj_;
  Variant cached_key_;
};

}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_CLIENT_CPP_SRC_ANDROID_DATA_SNAPSHOT_ANDROID_H_
