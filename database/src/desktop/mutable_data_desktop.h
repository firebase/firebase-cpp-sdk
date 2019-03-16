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

#ifndef FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_MUTABLE_DATA_DESKTOP_H_
#define FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_MUTABLE_DATA_DESKTOP_H_

#include <string>
#include "app/memory/shared_ptr.h"
#include "app/src/include/firebase/variant.h"
#include "app/src/path.h"
#include "database/src/include/firebase/database/mutable_data.h"

namespace firebase {
namespace database {
namespace internal {

// The desktop implementation of MutableData, which encapsulates the data and
// priority at a location.
class MutableDataInternal {
 public:
  // This constructor is used when creating the original copy of MutableData
  explicit MutableDataInternal(
      DatabaseInternal* database, const Variant& data);

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
  const char* GetKey() const;

  // Get the key name of the source location of this data.
  std::string GetKeyString() const;

  // Get the value of the data contained at this location.
  Variant GetValue();

  // Get the priority of the data contained at this snapshot.
  Variant GetPriority();

  // Does this MutableData have data at a particular location?
  bool HasChild(const char* path) const;

  // Format the contents of this mutable data into a string, which can be used
  // for debugging or other purposes.
  void ToString(std::string* string_output);

  // Sets the data at this location to the given value.
  void SetValue(const Variant& value);

  // Sets the priority of this field, which controls its sort order relative to
  // its siblings.
  void SetPriority(const Variant& priority);

  // Get the Variant based on path_ and holder_
  Variant* GetNode();

  // Get stored path_.  Mostly for debug purpose
  const Path& GetPath() const { return path_; }

  // Get stored holder_.  Mostly for debug purpose
  const Variant& GetHolder() const { return *holder_; }

  // Returns a pointer to the database this MutableData is from.
  DatabaseInternal* database_internal() const { return db_; }

 private:
  explicit MutableDataInternal(const MutableDataInternal& other,
                               const Path& path);

  DatabaseInternal* db_;
  // Path relative to the root of holder_
  Path path_;

  // A shared Variant to be modified
  SharedPtr<Variant> holder_;
};

}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_MUTABLE_DATA_DESKTOP_H_
