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

#ifndef FIREBASE_DATABASE_SRC_ANDROID_DATA_SNAPSHOT_ANDROID_H_
#define FIREBASE_DATABASE_SRC_ANDROID_DATA_SNAPSHOT_ANDROID_H_

#include <jni.h>
#include <stddef.h>

#include <string>

#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/include/firebase/internal/mutex.h"
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

class DataSnapshotInternal {
 public:
  DataSnapshotInternal(const DataSnapshotInternal& snapshot);
  DataSnapshotInternal& operator=(const DataSnapshotInternal& snapshot);
  DataSnapshotInternal(DataSnapshotInternal&& snapshot) noexcept;
  DataSnapshotInternal& operator=(DataSnapshotInternal&& snapshot) noexcept;
  ~DataSnapshotInternal();

  bool Exists() const;
  DataSnapshotInternal* Child(const char* path) const;
  std::vector<DataSnapshot> GetChildren() const;
  size_t GetChildrenCount() const;
  bool HasChildren() const;
  const char* GetKey();
  std::string GetKeyString();
  Variant GetValue() const;
  Variant GetPriority() const;
  DatabaseReferenceInternal* GetReference() const;
  bool HasChild(const char* path) const;

  DatabaseInternal* database_internal() const { return db_; }
  static DataSnapshot GetInvalidDataSnapshot() { return DataSnapshot(nullptr); }

 private:
  friend class DatabaseInternal;
  friend class DatabaseReferenceInternal;
  friend class internal::Callbacks;

  static DataSnapshot MakeDataSnapshot(DataSnapshotInternal* ptr) {
    return DataSnapshot(ptr);
  }

  DataSnapshotInternal(DatabaseInternal* database, jobject data_snapshot_obj);

  static bool Initialize(App* app);
  static void Terminate(App* app);

  DatabaseInternal* db_;
  jobject obj_;
  mutable Mutex key_mutex_;
  Variant cached_key_;
};

}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_SRC_ANDROID_DATA_SNAPSHOT_ANDROID_H_
