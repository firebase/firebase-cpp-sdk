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

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "app/src/callback.h"
#include "database/src/common/query.h"
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

namespace {

bool ValidateQueryEndpoints(const QueryParams& params, Logger* logger) {
  if (params.order_by == QueryParams::kOrderByKey) {
    const char message[] =
        "You must use StartAt(String value), EndAt(String value) or "
        "EqualTo(String value) in combination with orderByKey(). Other type of "
        "values or using the version with 2 parameters is not supported";
    if ((HasStart(params) &&
         (GetStartName(params) != QueryParamsComparator::kMinKey ||
          !GetStartValue(params).is_string())) ||
        (HasEnd(params) &&
         (GetEndName(params) != QueryParamsComparator::kMaxKey ||
          !GetEndValue(params).is_string()))) {
      if (logger) logger->LogWarning(message);
      return false;
    }
  } else if (params.order_by == QueryParams::kOrderByPriority) {
    if ((HasStart(params) && !IsValidPriority(GetStartValue(params))) ||
        (HasEnd(params) && !IsValidPriority(GetEndValue(params)))) {
      if (logger) {
        logger->LogWarning(
            "When using orderByPriority(), values provided to StartAt(), "
            "EndAt(), or EqualTo() must be valid priorities.");
      }
      return false;
    }
  }
  return true;
}

bool ValidateEndpointValue(const Variant& value, const char* fn_name,
                           const Path& path, Logger* logger) {
  if (!value.is_null() && !value.is_numeric() && !value.is_string() &&
      !value.is_bool()) {
    if (logger) {
      logger->LogWarning(
          "Query::%s: Only strings, numbers, and boolean values are allowed. "
          "(URL = %s)",
          fn_name, path.c_str());
    }
    return false;
  }
  return true;
}

class SingleValueEventRegistration : public ValueEventRegistration {
 public:
  SingleValueEventRegistration(
      DatabaseInternal* database,
      std::shared_ptr<ReferenceCountedFutureImpl> future_impl,
      std::unique_ptr<SingleValueListener> listener,
      const QuerySpec& query_spec)
      : ValueEventRegistration(database, listener.get(), query_spec),
        active_(true),
        database_ref_(database ? database->this_ref()
                               : DatabaseInternal::ThisRef(nullptr)),
        future_impl_(std::move(future_impl)),
        listener_(std::move(listener)) {}

  void FireEvent(const Event& event) override {
    if (active_) {
      active_ = false;
      ValueEventRegistration::FireEvent(event);
      CleanUp();
    }
  }

  void FireCancelEvent(Error error) override {
    if (active_) {
      active_ = false;
      ValueEventRegistration::FireCancelEvent(error);
      CleanUp();
    }
  }

 private:
  void CleanUp() {
    DatabaseInternal::ThisRefLock lock(&database_ref_);
    if (DatabaseInternal* db = lock.GetReference()) {
      void* listener_ptr = static_cast<void*>(listener_.get());
      db->RemoveActiveEventRegistrations(query_spec(), listener_ptr);
      Repo::scheduler().Schedule(NewCallback(
          [](Repo::ThisRef ref, void* listener_ptr, QuerySpec spec) {
            Repo::ThisRefLock repo_lock(&ref);
            if (repo_lock.GetReference() != nullptr) {
              repo_lock.GetReference()->RemoveEventCallback(listener_ptr, spec);
            }
          },
          db->EnsureRepoRef(), listener_ptr, query_spec()));
    }
  }

  bool active_;
  DatabaseInternal::ThisRef database_ref_;
  std::shared_ptr<ReferenceCountedFutureImpl> future_impl_;
  std::unique_ptr<SingleValueListener> listener_;
};

}  // namespace

QueryInternal::QueryInternal(DatabaseInternal* database,
                             const QuerySpec& query_spec)
    : database_(database),
      query_spec_(query_spec),
      future_impl_(
          std::make_shared<ReferenceCountedFutureImpl>(kQueryFnCount)) {}

QueryInternal::QueryInternal(const QueryInternal& internal)
    : database_(internal.database_),
      query_spec_(internal.query_spec_),
      future_impl_(
          std::make_shared<ReferenceCountedFutureImpl>(kQueryFnCount)) {}

QueryInternal& QueryInternal::operator=(const QueryInternal& internal) {
  if (&internal != this) {
    database_ = internal.database_;
    query_spec_ = internal.query_spec_;
    future_impl_ = std::make_shared<ReferenceCountedFutureImpl>(kQueryFnCount);
  }
  return *this;
}

QueryInternal::QueryInternal(QueryInternal&& internal) noexcept
    : database_(internal.database_),
      query_spec_(std::move(internal.query_spec_)),
      future_impl_(std::move(internal.future_impl_)) {
  internal.database_ = nullptr;
}

QueryInternal& QueryInternal::operator=(QueryInternal&& internal) noexcept {
  if (&internal != this) {
    database_ = internal.database_;
    internal.database_ = nullptr;
    query_spec_ = std::move(internal.query_spec_);
    future_impl_ = std::move(internal.future_impl_);
  }
  return *this;
}

void QueryInternal::GetValueImpl(SafeFutureHandle<DataSnapshot> handle) {
  ReferenceCountedFutureImpl* api = query_future();
  if (!database_ || !api) {
    if (future_impl_) {
      future_impl_->Complete(handle, kErrorUnknownError,
                             "The database is being destroyed.");
    }
    return;
  }
  auto listener =
      std::make_unique<SingleValueListener>(database_, query_spec_, api, handle);
  void* listener_ptr = listener.get();
  AddEventRegistration(
      std::make_unique<SingleValueEventRegistration>(
          database_, future_impl_, std::move(listener), query_spec_),
      listener_ptr);
}

void QueryInternal::AddValueListener(ValueListener* listener) {
  if (!database_ || !listener) return;
  AddEventRegistration(std::make_unique<ValueEventRegistration>(
                           database_, listener, query_spec_),
                       static_cast<void*>(listener));
  database_->RegisterValueListener(query_spec_, listener,
                                   ValueListenerCleanupData(query_spec_));
}

void QueryInternal::RemoveValueListener(ValueListener* listener) {
  if (!database_) return;
  RemoveEventRegistration(static_cast<void*>(listener), query_spec_);
  database_->UnregisterValueListener(query_spec_, listener);
}

void QueryInternal::RemoveAllValueListeners() {
  if (!database_) return;
  RemoveEventRegistration(nullptr, query_spec_);
  database_->UnregisterAllValueListeners(query_spec_);
}

void QueryInternal::AddChildListener(ChildListener* listener) {
  if (!database_ || !listener) return;
  AddEventRegistration(std::make_unique<ChildEventRegistration>(
                           database_, listener, query_spec_),
                       static_cast<void*>(listener));
  database_->RegisterChildListener(query_spec_, listener,
                                   ChildListenerCleanupData(query_spec_));
}

void QueryInternal::RemoveChildListener(ChildListener* listener) {
  if (!database_) return;
  RemoveEventRegistration(static_cast<void*>(listener), query_spec_);
  database_->UnregisterChildListener(query_spec_, listener);
}

void QueryInternal::RemoveAllChildListeners() {
  if (!database_) return;
  RemoveEventRegistration(nullptr, query_spec_);
  database_->UnregisterAllChildListeners(query_spec_);
}

void QueryInternal::AddEventRegistration(
    std::unique_ptr<EventRegistration> registration, void* listener_ptr) {
  if (!database_) return;
  database_->AddEventRegistration(query_spec_, listener_ptr,
                                  registration.get());
  auto reg_wrapped = std::make_shared<std::unique_ptr<EventRegistration>>(
      std::move(registration));
  Repo::scheduler().Schedule(NewCallback(
      [](Repo::ThisRef ref, DatabaseInternal::ThisRef db_ref,
         QuerySpec query_spec, void* listener_ptr,
         std::shared_ptr<std::unique_ptr<EventRegistration>> reg_ptr_shared) {
        Repo::ThisRefLock lock(&ref);
        EventRegistration* reg_ptr = reg_ptr_shared->release();
        std::unique_ptr<EventRegistration> reg(reg_ptr);
        if (lock.GetReference() != nullptr) {
          lock.GetReference()->AddEventCallback(std::move(reg));
        } else if (reg_ptr != nullptr) {
          DatabaseInternal::ThisRefLock db_lock(&db_ref);
          if (db_lock.GetReference() != nullptr) {
            db_lock.GetReference()->RemoveEventRegistration(
                query_spec, listener_ptr, reg_ptr);
          }
        }
      },
      database_->EnsureRepoRef(), database_->this_ref(), query_spec_,
      listener_ptr, reg_wrapped));
}

void QueryInternal::RemoveEventRegistration(void* listener_ptr,
                                            const QuerySpec& query_spec) {
  if (!database_) return;
  database_->RemoveActiveEventRegistrations(query_spec, listener_ptr);
  Repo::scheduler().Schedule(NewCallback(
      [](Repo::ThisRef ref, void* listener_ptr, QuerySpec query_spec) {
        Repo::ThisRefLock lock(&ref);
        if (lock.GetReference() != nullptr) {
          lock.GetReference()->RemoveEventCallback(listener_ptr, query_spec);
        }
      },
      database_->EnsureRepoRef(), listener_ptr, query_spec));
}

DatabaseReferenceInternal* QueryInternal::GetReference() {
  return database_ ? new DatabaseReferenceInternal(database_, query_spec_.path)
                   : nullptr;
}

void QueryInternal::SetKeepSynchronized(bool keep_synchronized) {
  if (!database_) return;
  Repo::scheduler().Schedule(NewCallback(
      [](Repo::ThisRef ref, QuerySpec query_spec, bool keep_synchronized) {
        Repo::ThisRefLock lock(&ref);
        if (lock.GetReference() != nullptr) {
          lock.GetReference()->SetKeepSynchronized(query_spec,
                                                   keep_synchronized);
        }
      },
      database_->EnsureRepoRef(), query_spec_, keep_synchronized));
}

QueryInternal* QueryInternal::OrderBy(QueryParams::OrderBy order_by) {
  Logger* logger = database_ ? database_->logger() : nullptr;
  QuerySpec spec = query_spec_;
  spec.params.order_by = order_by;
  return ValidateQueryEndpoints(spec.params, logger)
             ? new QueryInternal(database_, spec)
             : nullptr;
}

QueryInternal* QueryInternal::OrderByChild(const char* path) {
  Logger* logger = database_ ? database_->logger() : nullptr;
  if (path == nullptr) {
    if (logger) logger->LogWarning("Query::OrderByChild(): Path cannot be null.");
    return nullptr;
  }
  QuerySpec spec = query_spec_;
  spec.params.order_by = QueryParams::kOrderByChild;
  spec.params.order_by_child = path;
  return ValidateQueryEndpoints(spec.params, logger)
             ? new QueryInternal(database_, spec)
             : nullptr;
}

QueryInternal* QueryInternal::OrderByKey() {
  return OrderBy(QueryParams::kOrderByKey);
}
QueryInternal* QueryInternal::OrderByPriority() {
  return OrderBy(QueryParams::kOrderByPriority);
}
QueryInternal* QueryInternal::OrderByValue() {
  return OrderBy(QueryParams::kOrderByValue);
}

QueryInternal* QueryInternal::StartAt(const Variant& value) {
  Logger* logger = database_ ? database_->logger() : nullptr;
  if (!ValidateEndpointValue(value, "StartAt()", query_spec_.path, logger))
    return nullptr;
  if (HasStart(query_spec_.params)) {
    if (logger)
      logger->LogWarning("Can't Call StartAt() or EqualTo() multiple times");
    return nullptr;
  }
  QuerySpec spec = query_spec_;
  spec.params.start_at_value = value;
  return ValidateQueryEndpoints(spec.params, logger)
             ? new QueryInternal(database_, spec)
             : nullptr;
}

QueryInternal* QueryInternal::StartAt(const Variant& value,
                                      const char* child_key) {
  Logger* logger = database_ ? database_->logger() : nullptr;
  if (child_key == nullptr) {
    if (logger) logger->LogWarning("Query::StartAt(): Child key cannot be null.");
    return nullptr;
  }
  if (!ValidateEndpointValue(value, "StartAt", query_spec_.path, logger))
    return nullptr;
  if (HasStart(query_spec_.params)) {
    if (logger)
      logger->LogWarning("Can't Call StartAt() or EqualTo() multiple times");
    return nullptr;
  }
  QuerySpec spec = query_spec_;
  spec.params.start_at_value = value;
  spec.params.start_at_child_key = std::string(child_key);
  return ValidateQueryEndpoints(spec.params, logger)
             ? new QueryInternal(database_, spec)
             : nullptr;
}

QueryInternal* QueryInternal::EndAt(const Variant& value) {
  Logger* logger = database_ ? database_->logger() : nullptr;
  if (!ValidateEndpointValue(value, "EndAt", query_spec_.path, logger))
    return nullptr;
  if (HasEnd(query_spec_.params)) {
    if (logger)
      logger->LogWarning("Can't Call EndAt() or EqualTo() multiple times");
    return nullptr;
  }
  QuerySpec spec = query_spec_;
  spec.params.end_at_value = value;
  return ValidateQueryEndpoints(spec.params, logger)
             ? new QueryInternal(database_, spec)
             : nullptr;
}

QueryInternal* QueryInternal::EndAt(const Variant& value,
                                    const char* child_key) {
  Logger* logger = database_ ? database_->logger() : nullptr;
  if (child_key == nullptr) {
    if (logger) logger->LogWarning("Query::EndAt(): Child key cannot be null.");
    return nullptr;
  }
  if (!ValidateEndpointValue(value, "EndAt", query_spec_.path, logger))
    return nullptr;
  if (HasEnd(query_spec_.params)) {
    if (logger)
      logger->LogWarning("Can't Call EndAt() or EqualTo() multiple times");
    return nullptr;
  }
  QuerySpec spec = query_spec_;
  spec.params.end_at_value = value;
  spec.params.end_at_child_key = std::string(child_key);
  return ValidateQueryEndpoints(spec.params, logger)
             ? new QueryInternal(database_, spec)
             : nullptr;
}

QueryInternal* QueryInternal::EqualTo(const Variant& value) {
  Logger* logger = database_ ? database_->logger() : nullptr;
  if (!ValidateEndpointValue(value, "EqualTo", query_spec_.path, logger))
    return nullptr;
  if (HasStart(query_spec_.params) || HasEnd(query_spec_.params)) {
    if (logger) {
      logger->LogWarning(
          "Can't Call EqualTo() and StartAt() or EndAt() multiple times");
    }
    return nullptr;
  }
  QuerySpec spec = query_spec_;
  spec.params.equal_to_value = value;
  return ValidateQueryEndpoints(spec.params, logger)
             ? new QueryInternal(database_, spec)
             : nullptr;
}

QueryInternal* QueryInternal::EqualTo(const Variant& value,
                                      const char* child_key) {
  Logger* logger = database_ ? database_->logger() : nullptr;
  if (child_key == nullptr) {
    if (logger) logger->LogWarning("Query::EqualTo(): Child key cannot be null.");
    return nullptr;
  }
  if (!ValidateEndpointValue(value, "EqualTo", query_spec_.path, logger))
    return nullptr;
  if (HasStart(query_spec_.params) || HasEnd(query_spec_.params)) {
    if (logger) {
      logger->LogWarning(
          "Can't Call EqualTo() and StartAt() or EndAt() multiple times");
    }
    return nullptr;
  }
  QuerySpec spec = query_spec_;
  spec.params.equal_to_value = value;
  spec.params.equal_to_child_key = std::string(child_key);
  return ValidateQueryEndpoints(spec.params, logger)
             ? new QueryInternal(database_, spec)
             : nullptr;
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

}  // namespace internal
}  // namespace database
}  // namespace firebase
