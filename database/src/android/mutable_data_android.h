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

#ifndef FIREBASE_DATABASE_SRC_ANDROID_MUTABLE_DATA_ANDROID_H_
#define FIREBASE_DATABASE_SRC_ANDROID_MUTABLE_DATA_ANDROID_H_

#include <jni.h>

#include <string>

#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/include/firebase/variant.h"
#include "database/src/include/firebase/database/common.h"
#include "database/src/include/firebase/database/mutable_data.h"

namespace firebase {
namespace database {

class DatabaseInternal;

namespace internal {
class Callbacks;

class MutableDataInternal {
 public:
  ~MutableDataInternal();

  MutableDataInternal* Child(const char* path);
  std::vector<MutableData> GetChildren();
  size_t GetChildrenCount();
  const char* GetKey();
  std::string GetKeyString();
  Variant GetValue() const;
  Variant GetPriority() const;
  bool HasChild(const char* path) const;
  void SetValue(const Variant& value);
  void SetPriority(const Variant& priority);
  DatabaseInternal* database_internal() const { return db_; }

 private:
  friend class DatabaseInternal;
  friend class internal::Callbacks;

  static MutableData MakeMutableData(MutableDataInternal* ptr) {
    return MutableData(ptr);
  }

  MutableDataInternal(DatabaseInternal* database, jobject mutable_data_obj);
  MutableDataInternal(const MutableDataInternal&) = delete;
  MutableDataInternal& operator=(const MutableDataInternal&) = delete;

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

#endif  // FIREBASE_DATABASE_SRC_ANDROID_MUTABLE_DATA_ANDROID_H_
