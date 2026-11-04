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

#ifndef FIREBASE_DATABASE_SRC_DESKTOP_QUERY_DESKTOP_H_
#define FIREBASE_DATABASE_SRC_DESKTOP_QUERY_DESKTOP_H_

#include <cstddef>
#include <memory>
#include <utility>

#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/reference_counted_future_impl.h"
#include "database/src/common/query_spec.h"
#include "database/src/desktop/core/event_registration.h"
#include "database/src/include/firebase/database.h"
#include "database/src/include/firebase/database/listener.h"

namespace firebase {
namespace database {
namespace internal {

class DatabaseInternal;

class QueryInternal {
 public:
  QueryInternal() = default;
  QueryInternal(DatabaseInternal* database, const QuerySpec& query_spec);
  QueryInternal(const QueryInternal& query);
  QueryInternal& operator=(const QueryInternal& query);
  QueryInternal(QueryInternal&& query) noexcept;
  QueryInternal& operator=(QueryInternal&& query) noexcept;
  virtual ~QueryInternal() = default;

  void GetValueImpl(SafeFutureHandle<DataSnapshot> handle);
  void AddValueListener(ValueListener* listener);
  void RemoveValueListener(ValueListener* listener);
  void RemoveAllValueListeners();
  void AddChildListener(ChildListener* listener);
  void RemoveChildListener(ChildListener* listener);
  void RemoveAllChildListeners();
  DatabaseReferenceInternal* GetReference();
  void SetKeepSynchronized(bool keep_sync);

  QueryInternal* OrderByChild(const char* path);
  QueryInternal* OrderByKey();
  QueryInternal* OrderByPriority();
  QueryInternal* OrderByValue();
  QueryInternal* StartAt(const Variant& value);
  QueryInternal* StartAt(const Variant& value, const char* child_key);
  QueryInternal* EndAt(const Variant& value);
  QueryInternal* EndAt(const Variant& value, const char* child_key);
  QueryInternal* EqualTo(const Variant& value);
  QueryInternal* EqualTo(const Variant& value, const char* child_key);
  QueryInternal* LimitToFirst(size_t limit);
  QueryInternal* LimitToLast(size_t limit);

  const QuerySpec& query_spec() const { return query_spec_; }
  DatabaseInternal* database_internal() const { return database_; }
  ReferenceCountedFutureImpl* query_future() {
    return database_ ? future_impl_.get() : nullptr;
  }

 protected:
  DatabaseInternal* database_ = nullptr;
  QuerySpec query_spec_;

 private:
  QueryInternal* OrderBy(QueryParams::OrderBy order_by);
  void AddEventRegistration(std::unique_ptr<EventRegistration> registration,
                            void* listener_ptr);
  void RemoveEventRegistration(void* listener_ptr, const QuerySpec& query_spec);

  std::shared_ptr<ReferenceCountedFutureImpl> future_impl_;
};

struct ValueListenerCleanupData {
  explicit ValueListenerCleanupData(QuerySpec query_spec_in)
      : query_spec(std::move(query_spec_in)) {}
  QuerySpec query_spec;
};

struct ChildListenerCleanupData {
  explicit ChildListenerCleanupData(QuerySpec query_spec_in)
      : query_spec(std::move(query_spec_in)) {}
  QuerySpec query_spec;
};

}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_SRC_DESKTOP_QUERY_DESKTOP_H_
