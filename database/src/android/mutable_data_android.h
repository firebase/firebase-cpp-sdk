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

#ifndef FIREBASE_DATABASE_CLIENT_CPP_SRC_ANDROID_MUTABLE_DATA_ANDROID_H_
#define FIREBASE_DATABASE_CLIENT_CPP_SRC_ANDROID_MUTABLE_DATA_ANDROID_H_

#include <jni.h>
#include <string>
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/include/firebase/variant.h"
#include "database/src/include/firebase/database/common.h"
#include "database/src/include/firebase/database/mutable_data.h"

namespace firebase {
namespace database {

class DatabaseInternal;

namespace internal {
class Callbacks;

// The iOS implementation of MutableData, which encapsulates the data and
// priority at a location.
class MutableDataInternal {
 public:
  ~MutableDataInternal();

  // Create a shallow copy of the MutableData.
  // The caller owns the returned pointer and is responsible for deleting it.
  MutableDataInternal* Clone();

  // Used to obtain a MutableData instance that encapsulates the data and
  // priority at the given relative path.
  MutableDataInternal* Child(const char* path);

  // Get all the immediate children of this location.
  std::vector<MutableData> GetChildren();

  // Get the number of children of this location.
  size_t GetChildrenCount();

  // Get the key name of the source location of this data.
  const char* GetKey();

  // Get the key name of the source location of this data.
  std::string GetKeyString();

  // Get the value of the data contained at this location.
  Variant GetValue() const;

  // Get the priority of the data contained at this snapshot.
  Variant GetPriority() const;

  // Does this MutableData have data at a particular location?
  bool HasChild(const char* path) const;

  // Sets the data at this location to the given value.
  void SetValue(Variant value);

  // Sets the priority of this field, which controls its sort order relative to
  // its siblings.
  void SetPriority(Variant priority);

  // Returns a pointer to the database this MutableData is from.
  DatabaseInternal* database_internal() const { return db_; }

 private:
  friend class DatabaseInternal;
  friend class internal::Callbacks;

  static MutableData MakeMutableData(MutableDataInternal* ptr) {
    return MutableData(ptr);
  }

  MutableDataInternal(DatabaseInternal* database, jobject mutable_data_obj);

  MutableDataInternal& operator=(const MutableDataInternal&);
  MutableDataInternal(MutableDataInternal&);

  static bool Initialize(App* app);
  static void Terminate(App* app);

  DatabaseInternal* db_;
  jobject obj_;
  Variant cached_key_;
};

}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_CLIENT_CPP_SRC_ANDROID_MUTABLE_DATA_ANDROID_H_
