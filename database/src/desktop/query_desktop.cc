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
        if (logger) {
          logger->LogWarning(message);
        }
        return false;
      }
    }
    if (HasEnd(params)) {
      const Variant& end_node = GetEndValue(params);
      std::string end_name = GetEndName(params);
      if ((end_name != QueryParamsComparator::kMaxKey) ||
          !(end_node.is_string())) {
        if (logger) {
          logger->LogWarning(message);
        }
        return false;
      }
    }
  } else {
    if (params.order_by == QueryParams::kOrderByPriority) {
      if ((HasStart(params) && !IsValidPriority(GetStartValue(params))) ||
          (HasEnd(params) && !IsValidPriority(GetEndValue(params)))) {
        if (logger) {
          logger->LogWarning(
              "When using orderByPriority(), values provided to "
              "StartAt(), EndAt(), or EqualTo() must be valid "
              "priorities.");
        }
        return false;
      }
    }
  }
  return true;
}

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
  if (&internal == this) return *this;
  database_ = internal.database_;
  query_spec_ = internal.query_spec_;
  future_impl_ = std::make_shared<ReferenceCountedFutureImpl>(kQueryFnCount);
  return *this;
}

QueryInternal::QueryInternal(QueryInternal&& internal) noexcept
    : database_(internal.database_),
      query_spec_(std::move(internal.query_spec_)),
      future_impl_(std::move(internal.future_impl_)) {
  internal.database_ = nullptr;
}

QueryInternal& QueryInternal::operator=(QueryInternal&& internal) noexcept {
  if (&internal == this) return *this;
  database_ = internal.database_;
  internal.database_ = nullptr;
  query_spec_ = std::move(internal.query_spec_);
  future_impl_ = std::move(internal.future_impl_);
  return *this;
}

QueryInternal::~QueryInternal() = default;

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
    // Remove the listener associated with this event registration.
    // This will immediately mark this registration as removed, and schedule the
    // registration to be removed and deleted from the sync tree. Because the
    // registration owns the Listener, the listener will be cleaned up safely as
    // well.
    DatabaseInternal::ThisRefLock lock(&database_ref_);
    DatabaseInternal* db = lock.GetReference();
    if (db != nullptr) {
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
  ValueListenerCleanupData cleanup_data(query_spec_);
  AddEventRegistration(std::make_unique<ValueEventRegistration>(
                           database_, listener, query_spec_),
                       static_cast<void*>(listener));
  database_->RegisterValueListener(query_spec_, listener,
                                   std::move(cleanup_data));
}

void QueryInternal::RemoveValueListener(ValueListener* listener) {
  if (!database_) return;
  RemoveEventRegistration(listener, query_spec_);
  database_->UnregisterValueListener(query_spec_, listener);
}

void QueryInternal::RemoveAllValueListeners() {
  if (!database_) return;
  RemoveEventRegistration(static_cast<void*>(nullptr), query_spec_);
  database_->UnregisterAllValueListeners(query_spec_);
}

void QueryInternal::AddEventRegistration(
    std::unique_ptr<EventRegistration> registration, void* listener_ptr) {
  if (!database_) return;
  database_->AddEventRegistration(query_spec_, listener_ptr,
                                  registration.get());
  // NewCallback() does not allow you to pass in a move-only object
  // such as the unique_ptr above. To avoid the need for that, we can
  // wrap the unique_ptr into a shared_ptr. This will allow us to
  // safely pass it to the callback, which will then release the pointer
  // from the unique_ptr and create its own unique_ptr.
  //
  // This guarantees that the pointer is always owned by something: either the
  // shared_ptr created here, or (once it's removed from the inner unique_ptr)
  // the newly-created unique_ptr in the callback.
  //
  // TODO(b/289284829): Refactor NewCallback to support move semantics for
  // callback arguments.
  std::shared_ptr<std::unique_ptr<EventRegistration>> reg_wrapped =
      std::make_shared<std::unique_ptr<EventRegistration>>(
          std::move(registration));
  Repo::scheduler().Schedule(NewCallback(
      [](Repo::ThisRef ref, DatabaseInternal::ThisRef db_ref,
          QuerySpec query_spec, void* listener_ptr,
          std::shared_ptr<std::unique_ptr<EventRegistration>> reg_ptr_shared) {
        Repo::ThisRefLock lock(&ref);
        // Transfer ownership of the internal pointer to a new unique_ptr.
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
  // When a listener is removed, immediately invalidate its event registration
  // before scheduling it for removal from the database. From the point of view
  // of the main thread, it has now been removed and no more callbacks should be
  // called on it. This prevents race conditions that would make it impossible
  // to know when it is safe to delete a listener pointer.
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

void QueryInternal::RemoveEventRegistration(ValueListener* listener,
                                            const QuerySpec& query_spec) {
  RemoveEventRegistration(static_cast<void*>(listener), query_spec);
}

void QueryInternal::RemoveEventRegistration(ChildListener* listener,
                                            const QuerySpec& query_spec) {
  RemoveEventRegistration(static_cast<void*>(listener), query_spec);
}

void QueryInternal::AddChildListener(ChildListener* listener) {
  if (!database_ || !listener) return;
  ChildListenerCleanupData cleanup_data(query_spec_);
  AddEventRegistration(std::make_unique<ChildEventRegistration>(
                           database_, listener, query_spec_),
                       static_cast<void*>(listener));
  database_->RegisterChildListener(query_spec_, listener,
                                   std::move(cleanup_data));
}

void QueryInternal::RemoveChildListener(ChildListener* listener) {
  if (!database_) return;
  RemoveEventRegistration(listener, query_spec_);
  database_->UnregisterChildListener(query_spec_, listener);
}

void QueryInternal::RemoveAllChildListeners() {
  if (!database_) return;
  RemoveEventRegistration(static_cast<void*>(nullptr), query_spec_);
  database_->UnregisterAllChildListeners(query_spec_);
}

DatabaseReferenceInternal* QueryInternal::GetReference() {
  if (!database_) return nullptr;
  return new DatabaseReferenceInternal(database_, query_spec_.path);
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

QueryInternal* QueryInternal::OrderByChild(const char* path) {
  Logger* logger = database_ ? database_->logger() : nullptr;
  if (path == nullptr) {
    if (logger) {
      logger->LogWarning("Query::OrderByChild(): Path cannot be null.");
    }
    return nullptr;
  }
  QuerySpec spec = query_spec_;
  spec.params.order_by = QueryParams::kOrderByChild;
  spec.params.order_by_child = path;
  if (!ValidateQueryEndpoints(spec.params, logger)) {
    return nullptr;
  }
  return new QueryInternal(database_, spec);
}

QueryInternal* QueryInternal::OrderByKey() {
  Logger* logger = database_ ? database_->logger() : nullptr;
  QuerySpec spec = query_spec_;
  spec.params.order_by = QueryParams::kOrderByKey;
  if (!ValidateQueryEndpoints(spec.params, logger)) {
    return nullptr;
  }
  return new QueryInternal(database_, spec);
}

QueryInternal* QueryInternal::OrderByPriority() {
  Logger* logger = database_ ? database_->logger() : nullptr;
  QuerySpec spec = query_spec_;
  spec.params.order_by = QueryParams::kOrderByPriority;
  if (!ValidateQueryEndpoints(spec.params, logger)) {
    return nullptr;
  }
  return new QueryInternal(database_, spec);
}

QueryInternal* QueryInternal::OrderByValue() {
  Logger* logger = database_ ? database_->logger() : nullptr;
  QuerySpec spec = query_spec_;
  spec.params.order_by = QueryParams::kOrderByValue;
  if (!ValidateQueryEndpoints(spec.params, logger)) {
    return nullptr;
  }
  return new QueryInternal(database_, spec);
}

QueryInternal* QueryInternal::StartAt(const Variant& value) {
  Logger* logger = database_ ? database_->logger() : nullptr;
  if (!value.is_null() && !value.is_numeric() && !value.is_string() &&
      !value.is_bool()) {
    if (logger) {
      logger->LogWarning(
          "Query::StartAt(): Only strings, numbers, and boolean values are "
          "allowed. (URL = %s)",
          query_spec_.path.c_str());
    }
    return nullptr;
  }
  if (HasStart(query_spec_.params)) {
    if (logger) {
      logger->LogWarning("Can't Call StartAt() or EqualTo() multiple times");
    }
    return nullptr;
  }
  QuerySpec spec = query_spec_;
  spec.params.start_at_value = value;
  if (!ValidateQueryEndpoints(spec.params, logger)) {
    return nullptr;
  }
  return new QueryInternal(database_, spec);
}

QueryInternal* QueryInternal::StartAt(const Variant& value,
                                      const char* child_key) {
  Logger* logger = database_ ? database_->logger() : nullptr;
  if (child_key == nullptr) {
    if (logger) {
      logger->LogWarning("Query::StartAt(): Child key cannot be null.");
    }
    return nullptr;
  }
  if (!value.is_null() && !value.is_numeric() && !value.is_string() &&
      !value.is_bool()) {
    if (logger) {
      logger->LogWarning(
          "Query::StartAt: Only strings, numbers, and boolean values are "
          "allowed. (URL = %s)",
          query_spec_.path.c_str());
    }
    return nullptr;
  }
  if (HasStart(query_spec_.params)) {
    if (logger) {
      logger->LogWarning("Can't Call StartAt() or EqualTo() multiple times");
    }
    return nullptr;
  }
  QuerySpec spec = query_spec_;
  spec.params.start_at_value = value;
  spec.params.start_at_child_key = Optional<std::string>(child_key);
  if (!ValidateQueryEndpoints(spec.params, logger)) {
    return nullptr;
  }
  return new QueryInternal(database_, spec);
}

QueryInternal* QueryInternal::EndAt(const Variant& value) {
  Logger* logger = database_ ? database_->logger() : nullptr;
  if (!value.is_null() && !value.is_numeric() && !value.is_string() &&
      !value.is_bool()) {
    if (logger) {
      logger->LogWarning(
          "Query::EndAt: Only strings, numbers, and boolean values are "
          "allowed. (URL = %s)",
          query_spec_.path.c_str());
    }
    return nullptr;
  }
  if (HasEnd(query_spec_.params)) {
    if (logger) {
      logger->LogWarning("Can't Call EndAt() or EqualTo() multiple times");
    }
    return nullptr;
  }
  QuerySpec spec = query_spec_;
  spec.params.end_at_value = value;
  if (!ValidateQueryEndpoints(spec.params, logger)) {
    return nullptr;
  }
  return new QueryInternal(database_, spec);
}

QueryInternal* QueryInternal::EndAt(const Variant& value,
                                    const char* child_key) {
  Logger* logger = database_ ? database_->logger() : nullptr;
  if (child_key == nullptr) {
    if (logger) {
      logger->LogWarning("Query::EndAt(): Child key cannot be null.");
    }
    return nullptr;
  }
  if (!value.is_null() && !value.is_numeric() && !value.is_string() &&
      !value.is_bool()) {
    if (logger) {
      logger->LogWarning(
          "Query::EndAt: Only strings, numbers, and boolean values are "
          "allowed. (URL = %s)",
          query_spec_.path.c_str());
    }
    return nullptr;
  }
  if (HasEnd(query_spec_.params)) {
    if (logger) {
      logger->LogWarning("Can't Call EndAt() or EqualTo() multiple times");
    }
    return nullptr;
  }
  QuerySpec spec = query_spec_;
  spec.params.end_at_value = value;
  spec.params.end_at_child_key = Optional<std::string>(child_key);
  if (!ValidateQueryEndpoints(spec.params, logger)) {
    return nullptr;
  }
  return new QueryInternal(database_, spec);
}

QueryInternal* QueryInternal::EqualTo(const Variant& value) {
  Logger* logger = database_ ? database_->logger() : nullptr;
  if (!value.is_null() && !value.is_numeric() && !value.is_string() &&
      !value.is_bool()) {
    if (logger) {
      logger->LogWarning(
          "Query::EqualTo: Only strings, numbers, and boolean values are "
          "allowed. (URL = %s)",
          query_spec_.path.c_str());
    }
    return nullptr;
  }
  if (HasStart(query_spec_.params) || HasEnd(query_spec_.params)) {
    if (logger) {
      logger->LogWarning(
          "Can't Call EqualTo() and StartAt() or EndAt() multiple times");
    }
    return nullptr;
  }
  QuerySpec spec = query_spec_;
  spec.params.equal_to_value = value;
  if (!ValidateQueryEndpoints(spec.params, logger)) {
    return nullptr;
  }
  return new QueryInternal(database_, spec);
}

QueryInternal* QueryInternal::EqualTo(const Variant& value,
                                      const char* child_key) {
  Logger* logger = database_ ? database_->logger() : nullptr;
  if (child_key == nullptr) {
    if (logger) {
      logger->LogWarning("Query::EqualTo(): Child key cannot be null.");
    }
    return nullptr;
  }
  if (!value.is_null() && !value.is_numeric() && !value.is_string() &&
      !value.is_bool()) {
    if (logger) {
      logger->LogWarning(
          "Query::EqualTo: Only strings, numbers, and boolean values are "
          "allowed. (URL = %s)",
          query_spec_.path.c_str());
    }
    return nullptr;
  }
  if (HasStart(query_spec_.params) || HasEnd(query_spec_.params)) {
    if (logger) {
      logger->LogWarning(
          "Can't Call EqualTo() and StartAt() or EndAt() multiple times");
    }
    return nullptr;
  }
  QuerySpec spec = query_spec_;
  spec.params.equal_to_value = value;
  spec.params.equal_to_child_key = Optional<std::string>(child_key);
  if (!ValidateQueryEndpoints(spec.params, logger)) {
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
  return database_ ? future_impl_.get() : nullptr;
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
