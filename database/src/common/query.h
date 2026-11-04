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

#ifndef FIREBASE_DATABASE_SRC_COMMON_QUERY_H_
#define FIREBASE_DATABASE_SRC_COMMON_QUERY_H_

#include "app/src/reference_counted_future_impl.h"

namespace firebase {
namespace database {
namespace internal {

enum kQueryFn {
  kQueryFnGetValue = 0,
  kQueryFnCount,
};

template <typename DbInternal, int FnCount>
class ScopedFutureApi {
 public:
  explicit ScopedFutureApi(DbInternal* db) : db_(db) {
    if (db_) db_->future_manager().AllocFutureApi(&id_, FnCount);
  }
  ScopedFutureApi(const ScopedFutureApi& other) : db_(other.db_) {
    if (db_) db_->future_manager().AllocFutureApi(&id_, FnCount);
  }
  ScopedFutureApi& operator=(const ScopedFutureApi& other) {
    if (this != &other && db_ != other.db_) {
      if (db_) db_->future_manager().ReleaseFutureApi(&id_);
      db_ = other.db_;
      if (db_) db_->future_manager().AllocFutureApi(&id_, FnCount);
    }
    return *this;
  }
  ScopedFutureApi(ScopedFutureApi&& other) noexcept : db_(other.db_) {
    other.db_ = nullptr;
    if (db_) db_->future_manager().MoveFutureApi(&other.id_, &id_);
  }
  ScopedFutureApi& operator=(ScopedFutureApi&& other) noexcept {
    if (this != &other) {
      if (db_) db_->future_manager().ReleaseFutureApi(&id_);
      db_ = other.db_;
      other.db_ = nullptr;
      if (db_) db_->future_manager().MoveFutureApi(&other.id_, &id_);
    }
    return *this;
  }
  ~ScopedFutureApi() {
    if (db_) db_->future_manager().ReleaseFutureApi(&id_);
  }
  ReferenceCountedFutureImpl* get() const {
    return db_ ? db_->future_manager().GetFutureApi(const_cast<int*>(&id_))
               : nullptr;
  }

 private:
  DbInternal* db_ = nullptr;
  int id_ = 0;
};

}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_SRC_COMMON_QUERY_H_
