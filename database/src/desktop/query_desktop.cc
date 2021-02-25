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

#include "database/src/desktop/query_desktop.h"

#include <sstream>

#include "app/memory/unique_ptr.h"
#include "app/rest/transport_builder.h"
#include "app/rest/transport_curl.h"
#include "app/rest/util.h"
#include "app/src/callback.h"
#include "app/src/function_registry.h"
#include "app/src/semaphore.h"
#include "app/src/thread.h"
#include "app/src/variant_util.h"
#include "database/src/common/query.h"
#include "database/src/desktop/connection/host_info.h"
#include "database/src/desktop/core/child_event_registration.h"
#include "database/src/desktop/core/event_registration.h"
#include "database/src/desktop/core/value_event_registration.h"
#include "database/src/desktop/data_snapshot_desktop.h"
#include "database/src/desktop/database_desktop.h"
#include "database/src/desktop/database_reference_desktop.h"
#include "database/src/desktop/util_desktop.h"

namespace firebase {

using callback::NewCallback;

namespace database {
namespace internal {

// This method validates that key index has been called with the correct
// combination of parameters
static bool ValidateQueryEndpoints(const QueryParams& params, Logger* logger) {
  if (params.order_by == QueryParams::kOrderByKey) {
    const char message[] =
        "You must use StartAt(String value), EndAt(String value) or "
        "EqualTo(String value) in combination with orderByKey(). Other type of "
        "values or using the version with 2 parameters is not supported";
    if (HasStart(params)) {
      const Variant& start_node = GetStartValue(params);
      std::string start_name = GetStartName(params);
      if ((start_name != QueryParamsComparator::kMinKey) ||
          !(start_node.is_string())) {
        logger->LogWarning(message);
        return false;
      }
    }
    if (HasEnd(params)) {
      const Variant& end_node = GetEndValue(params);
      std::string end_name = GetEndName(params);
      if ((end_name != QueryParamsComparator::kMaxKey) ||
          !(end_node.is_string())) {
        logger->LogWarning(message);
        return false;
      }
    }
  } else {
    if (params.order_by == QueryParams::kOrderByPriority) {
      if ((HasStart(params) && !IsValidPriority(GetStartValue(params))) ||
          (HasEnd(params) && !IsValidPriority(GetEndValue(params)))) {
        logger->LogWarning(
            "When using orderByPriority(), values provided to "
            "StartAt(), EndAt(), or EqualTo() must be valid "
            "priorities.");
        return false;
      }
    }
  }
  return true;
}

QueryInternal::QueryInternal(DatabaseInternal* database,
                             const QuerySpec& query_spec)
    : database_(database), query_spec_(query_spec) {
  database_->future_manager().AllocFutureApi(&future_api_id_, kQueryFnCount);
}

QueryInternal::QueryInternal(const QueryInternal& internal)
    : database_(internal.database_), query_spec_(internal.query_spec_) {
  database_->future_manager().AllocFutureApi(&future_api_id_, kQueryFnCount);
}

QueryInternal& QueryInternal::operator=(const QueryInternal& internal) {
  database_ = internal.database_;
  query_spec_ = internal.query_spec_;
  return *this;
}

#if defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)
QueryInternal::QueryInternal(QueryInternal&& internal)
    : database_(internal.database_),
      query_spec_(std::move(internal.query_spec_)) {
  database_->future_manager().MoveFutureApi(&internal.future_api_id_,
                                            &future_api_id_);
}

QueryInternal& QueryInternal::operator=(QueryInternal&& internal) {
  database_ = internal.database_;
  query_spec_ = std::move(internal.query_spec_);
  database_->future_manager().MoveFutureApi(&internal.future_api_id_,
                                            &future_api_id_);
  return *this;
}
#endif  // defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)

QueryInternal::~QueryInternal() {}

class SingleValueEventRegistration : public ValueEventRegistration {
 public:
  SingleValueEventRegistration(DatabaseInternal* database,
                               SingleValueListener** single_listener_holder,
                               const QuerySpec& query_spec)
      : ValueEventRegistration(database, *single_listener_holder, query_spec),
        listener_mutex_(database->listener_mutex()),
        single_listener_holder_(single_listener_holder) {}

  void FireEvent(const Event& event) override {
    MutexLock lock(*listener_mutex_);
    if (single_listener_holder_ && *single_listener_holder_) {
      single_listener_holder_ = nullptr;
      ValueEventRegistration::FireEvent(event);
    }
  }

  void FireCancelEvent(Error error) override {
    MutexLock lock(*listener_mutex_);
    if (single_listener_holder_ && *single_listener_holder_) {
      single_listener_holder_ = nullptr;
      ValueEventRegistration::FireCancelEvent(error);
    }
  }

 private:
  Mutex* listener_mutex_;
  SingleValueListener** single_listener_holder_;
};

Future<DataSnapshot> QueryInternal::GetValue() {
  SafeFutureHandle<DataSnapshot> handle =
      query_future()->SafeAlloc<DataSnapshot>(kQueryFnGetValue,
                                              DataSnapshot(nullptr));
  // TODO(b/68878322): Refactor this code to be less brittle, possibly using the
  // CleanupNotifier or something like it.
  SingleValueListener* single_listener =
      new SingleValueListener(database_, query_spec_, query_future(), handle);

  // If the database goes away, we need to be able to reach into these blocks
  // and clear their single_listener pointer. We can't do that directly, but we
  // can cache a pointer to the pointer, and clear that instead.
  SingleValueListener** single_listener_holder =
      database_->AddSingleValueListener(single_listener);

  AddEventRegistration(MakeUnique<SingleValueEventRegistration>(
      database_, single_listener_holder, query_spec_));
  return MakeFuture(query_future(), handle);
}

Future<DataSnapshot> QueryInternal::GetValueLastResult() {
  return static_cast<const Future<DataSnapshot>&>(
      query_future()->LastResult(kQueryFnGetValue));
}

void QueryInternal::AddValueListener(ValueListener* listener) {
  ValueListenerCleanupData cleanup_data(query_spec_);
  AddEventRegistration(
      MakeUnique<ValueEventRegistration>(database_, listener, query_spec_));

  database_->RegisterValueListener(query_spec_, listener,
                                   std::move(cleanup_data));
}

void QueryInternal::RemoveValueListener(ValueListener* listener) {
  RemoveEventRegistration(listener, query_spec_);
  database_->UnregisterValueListener(query_spec_, listener);
}

void QueryInternal::RemoveAllValueListeners() {
  RemoveEventRegistration(static_cast<void*>(nullptr), query_spec_);
  database_->UnregisterAllValueListeners(query_spec_);
}

void QueryInternal::AddEventRegistration(
    UniquePtr<EventRegistration> registration) {
  Repo::scheduler().Schedule(NewCallback(
      [](Repo::ThisRef ref, UniquePtr<EventRegistration> registration) {
        Repo::ThisRefLock lock(&ref);
        if (lock.GetReference() != nullptr) {
          lock.GetReference()->AddEventCallback(Move(registration));
        }
      },
      database_->repo()->this_ref(), Move(registration)));
}

void QueryInternal::RemoveEventRegistration(void* listener_ptr,
                                            const QuerySpec& query_spec) {
  Repo::scheduler().Schedule(NewCallback(
      [](Repo::ThisRef ref, void* listener_ptr, QuerySpec query_spec) {
        Repo::ThisRefLock lock(&ref);
        if (lock.GetReference() != nullptr) {
          lock.GetReference()->RemoveEventCallback(listener_ptr, query_spec);
        }
      },
      database_->repo()->this_ref(), listener_ptr, query_spec));
}

void QueryInternal::RemoveEventRegistration(ValueListener* listener,
                                            const QuerySpec& query_spec) {
  RemoveEventRegistration(static_cast<void*>(listener), query_spec);
}

void QueryInternal::RemoveEventRegistration(ChildListener* listener,
                                            const QuerySpec& query_spec) {
  RemoveEventRegistration(static_cast<void*>(listener), query_spec);
}

void QueryInternal::AddChildListener(ChildListener* listener) {
  ChildListenerCleanupData cleanup_data(query_spec_);
  AddEventRegistration(
      MakeUnique<ChildEventRegistration>(database_, listener, query_spec_));
  database_->RegisterChildListener(query_spec_, listener,
                                   std::move(cleanup_data));
}

void QueryInternal::RemoveChildListener(ChildListener* listener) {
  RemoveEventRegistration(listener, query_spec_);
  database_->UnregisterChildListener(query_spec_, listener);
}

void QueryInternal::RemoveAllChildListeners() {
  RemoveEventRegistration(static_cast<void*>(nullptr), query_spec_);
  database_->UnregisterAllChildListeners(query_spec_);
}

DatabaseReferenceInternal* QueryInternal::GetReference() {
  return new DatabaseReferenceInternal(database_, query_spec_.path);
}

void QueryInternal::SetKeepSynchronized(bool keep_synchronized) {
  Repo::scheduler().Schedule(NewCallback(
      [](Repo::ThisRef ref, QuerySpec query_spec, bool keep_synchronized) {
        Repo::ThisRefLock lock(&ref);
        if (lock.GetReference() != nullptr) {
          lock.GetReference()->SetKeepSynchronized(query_spec,
                                                   keep_synchronized);
        }
      },
      database_->repo()->this_ref(), query_spec_, keep_synchronized));
}

QueryInternal* QueryInternal::OrderByChild(const char* path) {
  QuerySpec spec = query_spec_;
  spec.params.order_by = QueryParams::kOrderByChild;
  spec.params.order_by_child = path;
  return new QueryInternal(database_, spec);
}

QueryInternal* QueryInternal::OrderByKey() {
  QuerySpec spec = query_spec_;
  spec.params.order_by = QueryParams::kOrderByKey;
  return new QueryInternal(database_, spec);
}

QueryInternal* QueryInternal::OrderByPriority() {
  QuerySpec spec = query_spec_;
  spec.params.order_by = QueryParams::kOrderByPriority;
  return new QueryInternal(database_, spec);
}

QueryInternal* QueryInternal::OrderByValue() {
  QuerySpec spec = query_spec_;
  spec.params.order_by = QueryParams::kOrderByValue;
  return new QueryInternal(database_, spec);
}

QueryInternal* QueryInternal::StartAt(const Variant& value) {
  Logger* logger = database_->logger();
  if (!value.is_numeric() && !value.is_string() && !value.is_bool()) {
    logger->LogWarning(
        "Query::StartAt(): Only strings, numbers, and boolean values are "
        "allowed. (URL = %s)",
        query_spec_.path.c_str());
    return nullptr;
  }
  if (HasStart(query_spec_.params)) {
    logger->LogWarning("Can't Call StartAt() or EqualTo() multiple times");
    return nullptr;
  }
  QuerySpec spec = query_spec_;
  spec.params.start_at_value = value;
  if (!ValidateQueryEndpoints(spec.params, database_->logger())) {
    return nullptr;
  }
  return new QueryInternal(database_, spec);
}

QueryInternal* QueryInternal::StartAt(const Variant& value,
                                      const char* child_key) {
  Logger* logger = database_->logger();
  if (!value.is_numeric() && !value.is_string() && !value.is_bool()) {
    logger->LogWarning(
        "Query::StartAt: Only strings, numbers, and boolean values are "
        "allowed. (URL = %s)",
        query_spec_.path.c_str());
    return nullptr;
  }
  FIREBASE_ASSERT_RETURN(nullptr, child_key != nullptr);
  QuerySpec spec = query_spec_;
  spec.params.start_at_value = value;
  spec.params.start_at_child_key = child_key;
  if (!ValidateQueryEndpoints(spec.params, database_->logger())) {
    return nullptr;
  }
  return new QueryInternal(database_, spec);
}

QueryInternal* QueryInternal::EndAt(const Variant& value) {
  Logger* logger = database_->logger();
  if (!value.is_numeric() && !value.is_string() && !value.is_bool()) {
    logger->LogWarning(
        "Query::EndAt: Only strings, numbers, and boolean values are "
        "allowed. (URL = %s)",
        query_spec_.path.c_str());
    return nullptr;
  }
  if (HasEnd(query_spec_.params)) {
    logger->LogWarning("Can't Call EndAt() or EqualTo() multiple times");
    return nullptr;
  }
  QuerySpec spec = query_spec_;
  spec.params.end_at_value = value;
  if (!ValidateQueryEndpoints(spec.params, database_->logger())) {
    return nullptr;
  }
  return new QueryInternal(database_, spec);
}

QueryInternal* QueryInternal::EndAt(const Variant& value,
                                    const char* child_key) {
  Logger* logger = database_->logger();
  if (!value.is_numeric() && !value.is_string() && !value.is_bool()) {
    logger->LogWarning(
        "Query::EndAt: Only strings, numbers, and boolean values are "
        "allowed. (URL = %s)",
        query_spec_.path.c_str());
    return nullptr;
  }
  FIREBASE_ASSERT_RETURN(nullptr, child_key != nullptr);
  QuerySpec spec = query_spec_;
  spec.params.end_at_value = value;
  spec.params.end_at_child_key = child_key;
  if (!ValidateQueryEndpoints(spec.params, database_->logger())) {
    return nullptr;
  }
  return new QueryInternal(database_, spec);
}

QueryInternal* QueryInternal::EqualTo(const Variant& value) {
  Logger* logger = database_->logger();
  if (!value.is_numeric() && !value.is_string() && !value.is_bool()) {
    logger->LogWarning(
        "Query::EqualTo: Only strings, numbers, and boolean values are "
        "allowed. (URL = %s)",
        query_spec_.path.c_str());
    return nullptr;
  }
  QuerySpec spec = query_spec_;
  spec.params.equal_to_value = value;
  if (!ValidateQueryEndpoints(spec.params, database_->logger())) {
    return nullptr;
  }
  return new QueryInternal(database_, spec);
}

QueryInternal* QueryInternal::EqualTo(const Variant& value,
                                      const char* child_key) {
  Logger* logger = database_->logger();
  if (!value.is_numeric() && !value.is_string() && !value.is_bool()) {
    logger->LogWarning(
        "Query::EqualTo: Only strings, numbers, and boolean values are "
        "allowed. (URL = %s)",
        query_spec_.path.c_str());
    return nullptr;
  }
  QuerySpec spec = query_spec_;
  spec.params.equal_to_value = value;
  spec.params.equal_to_child_key = child_key;
  if (!ValidateQueryEndpoints(spec.params, database_->logger())) {
    return nullptr;
  }
  return new QueryInternal(database_, spec);
}

QueryInternal* QueryInternal::LimitToFirst(size_t limit) {
  QuerySpec spec = query_spec_;
  spec.params.limit_first = limit;
  return new QueryInternal(database_, spec);
}

QueryInternal* QueryInternal::LimitToLast(size_t limit) {
  QuerySpec spec = query_spec_;
  spec.params.limit_last = limit;
  return new QueryInternal(database_, spec);
}

ReferenceCountedFutureImpl* QueryInternal::query_future() {
  return database_->future_manager().GetFutureApi(&future_api_id_);
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
