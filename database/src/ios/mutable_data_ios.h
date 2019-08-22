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

#ifndef FIREBASE_DATABASE_CLIENT_CPP_SRC_IOS_MUTABLE_DATA_IOS_H_
#define FIREBASE_DATABASE_CLIENT_CPP_SRC_IOS_MUTABLE_DATA_IOS_H_

#include <string>
#include "app/memory/unique_ptr.h"
#include "app/src/include/firebase/variant.h"
#include "app/src/util_ios.h"
#include "database/src/include/firebase/database/mutable_data.h"

#ifdef __OBJC__
#import "FIRDatabase.h"
#endif  // __OBJC__

namespace firebase {
namespace database {
namespace internal {

// This defines the class FIRMutableDataPointer, which is a C++-compatible
// wrapper around the FIRMutableData Obj-C class.
OBJ_C_PTR_WRAPPER(FIRMutableData);

#pragma clang assume_nonnull begin

class DatabaseReferenceInternal;

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
  const char* GetKey() const;

  // Get the key name of the source location of this data.
  std::string GetKeyString() const;

  // Get the value of the data contained at this location.
  Variant GetValue() const;

  // Get the priority of the data contained at this snapshot.
  Variant GetPriority();

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
  friend class DatabaseReferenceInternal;

  MutableDataInternal(DatabaseInternal* database,
                      UniquePtr<FIRMutableDataPointer> impl);

#ifdef __OBJC__
  FIRMutableData* impl() const { return impl_->get(); }
#endif  // __OBJC__

  DatabaseInternal* db_;
  UniquePtr<FIRMutableDataPointer> impl_;
};

#pragma clang assume_nonnull end

}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_CLIENT_CPP_SRC_IOS_MUTABLE_DATA_IOS_H_
