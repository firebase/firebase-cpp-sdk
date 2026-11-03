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

#include "database/src/desktop/database_reference_desktop.h"

#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <utility>

#include "app/src/variant_util.h"
#include "database/src/common/database_reference.h"
#include "database/src/desktop/database_desktop.h"
#include "database/src/desktop/disconnection_desktop.h"
#include "database/src/desktop/util_desktop.h"

namespace firebase {

using callback::NewCallback;

namespace database {
namespace internal {

namespace {

// Keeps the FutureBackingData and ReferenceCountedFutureImpl alive while a
// scheduled Repo callback is in flight, and completes the handle with an error
// (plus frees transaction context if any) if the Repo or Scheduler shuts down
// before the operation is handed off to Repo.
template <typename T>
struct AsyncOpState {
  AsyncOpState(ReferenceCountedFutureImpl* api_in,
               SafeFutureHandle<T> handle_in, Future<T> backing_future_in,
               void* context_in = nullptr,
               void (*delete_context_in)(void*) = nullptr)
      : api(api_in),
        handle(handle_in),
        backing_future(std::move(backing_future_in)),
        context(context_in),
        delete_context(delete_context_in),
        disarmed(false) {}

  ~AsyncOpState() { FailIfNotDisarmed(); }

  void Disarm() { disarmed = true; }

  void FailIfNotDisarmed() {
    if (disarmed) return;
    disarmed = true;
    if (delete_context != nullptr) {
      delete_context(context);
      delete_context = nullptr;
    }
    if (api != nullptr && backing_future.status() == kFutureStatusPending) {
      api->Complete(handle, kErrorUnknownError,
                    "The database is being destroyed.");
    }
  }

  ReferenceCountedFutureImpl* api;
  SafeFutureHandle<T> handle;
  Future<T> backing_future;
  void* context;
  void (*delete_context)(void*);
  bool disarmed;
};

SafeFutureHandle<void> AllocSlotOrTemp(ReferenceCountedFutureImpl* api,
                                       int fn_idx) {
  if (api->LastResult(fn_idx).status() == kFutureStatusPending) {
    return api->SafeAlloc<void>();
  }
  return api->SafeAlloc<void>(fn_idx);
}

}  // namespace

const char kVirtualChildKeyValue[] = ".value";
const char kVirtualChildKeyPriority[] = ".priority";

DatabaseReferenceInternal::DatabaseReferenceInternal(DatabaseInternal* database,
                                                     const Path& path)
    : QueryInternal(database, QuerySpec(path)) {
  if (database_) {
    database_->future_manager().AllocFutureApi(&future_api_id_,
                                               kDatabaseReferenceFnCount);
  }
}

DatabaseReferenceInternal::~DatabaseReferenceInternal() {
  if (database_) {
    database_->future_manager().ReleaseFutureApi(&future_api_id_);
  }
}

DatabaseReferenceInternal::DatabaseReferenceInternal(
    const DatabaseReferenceInternal& internal)
    : QueryInternal(internal) {
  if (database_) {
    database_->future_manager().AllocFutureApi(&future_api_id_,
                                               kDatabaseReferenceFnCount);
  }
}

DatabaseReferenceInternal& DatabaseReferenceInternal::operator=(
    const DatabaseReferenceInternal& internal) {
  if (&internal == this) return *this;
  if (database_ != internal.database_) {
    if (database_) {
      database_->future_manager().ReleaseFutureApi(&future_api_id_);
    }
    if (internal.database_) {
      internal.database_->future_manager().AllocFutureApi(
          &future_api_id_, kDatabaseReferenceFnCount);
    }
  }
  QueryInternal::operator=(internal);
  return *this;
}

DatabaseReferenceInternal::DatabaseReferenceInternal(
    DatabaseReferenceInternal&& internal) noexcept
    : QueryInternal(std::move(internal)) {
  internal.database_ = nullptr;
  if (database_) {
    database_->future_manager().MoveFutureApi(&internal.future_api_id_,
                                              &future_api_id_);
  }
}

DatabaseReferenceInternal& DatabaseReferenceInternal::operator=(
    DatabaseReferenceInternal&& internal) noexcept {
  if (&internal == this) return *this;
  if (database_) {
    database_->future_manager().ReleaseFutureApi(&future_api_id_);
  }
  QueryInternal::operator=(std::move(internal));
  internal.database_ = nullptr;
  if (database_) {
    database_->future_manager().MoveFutureApi(&internal.future_api_id_,
                                              &future_api_id_);
  }
  return *this;
}

Database* DatabaseReferenceInternal::GetDatabase() const {
  if (!database_) return nullptr;
  return database_->constructor_url().empty()
             ? Database::GetInstance(database_->GetApp())
             : Database::GetInstance(database_->GetApp(),
                                     database_->constructor_url().c_str());
}

std::string DatabaseReferenceInternal::GetUrl() const {
  if (!database_) return std::string();
  std::stringstream url;
  url << database_->database_url();
  if (!query_spec_.path.str().empty()) {
    url << '/';
    url << query_spec_.path.str();
  }
  return url.str();
}

bool DatabaseReferenceInternal::IsRoot() const {
  return query_spec_.path.empty();
}

DatabaseReferenceInternal* DatabaseReferenceInternal::GetParent() {
  if (!database_) return nullptr;
  return new DatabaseReferenceInternal(database_, query_spec_.path.GetParent());
}

DatabaseReferenceInternal* DatabaseReferenceInternal::GetRoot() {
  if (!database_) return nullptr;
  return new DatabaseReferenceInternal(database_, Path());
}

DatabaseReferenceInternal* DatabaseReferenceInternal::Child(const char* path) {
  if (!database_ || path == nullptr) return nullptr;
  return new DatabaseReferenceInternal(database_,
                                       query_spec_.path.GetChild(path));
}

DatabaseReferenceInternal* DatabaseReferenceInternal::PushChild() {
  if (!database_) return nullptr;
  auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                 std::chrono::system_clock::now().time_since_epoch())
                 .count();
  std::string child = database_->name_generator().GeneratePushChildName(now);
  return new DatabaseReferenceInternal(database_,
                                       query_spec_.path.GetChild(child));
}

Future<void> DatabaseReferenceInternal::RemoveValue() {
  ReferenceCountedFutureImpl* api = ref_future();
  if (!database_ || !api) return Future<void>();
  SafeFutureHandle<void> handle =
      api->SafeAlloc<void>(kDatabaseReferenceFnRemoveValue);
  Future<void> result = MakeFuture(api, handle);
  auto state = std::make_shared<AsyncOpState<void>>(api, handle, result);

  Repo::scheduler().Schedule(NewCallback(
      [](Repo::ThisRef ref, Path path,
         std::shared_ptr<AsyncOpState<void>> op_state) {
        Repo::ThisRefLock lock(&ref);
        if (lock.GetReference() != nullptr) {
          op_state->Disarm();
          lock.GetReference()->SetValue(path, Variant::Null(), op_state->api,
                                        op_state->handle);
        } else {
          op_state->FailIfNotDisarmed();
        }
      },
      database_->EnsureRepoRef(), query_spec_.path, state));
  return result;
}

Future<void> DatabaseReferenceInternal::RemoveValueLastResult() {
  ReferenceCountedFutureImpl* api = ref_future();
  return api ? static_cast<const Future<void>&>(
                   api->LastResult(kDatabaseReferenceFnRemoveValue))
             : Future<void>();
}

Future<DataSnapshot> DatabaseReferenceInternal::RunTransaction(
    DoTransactionWithContext transaction_function, void* context,
    void (*delete_context)(void*), bool trigger_local_events) {
  ReferenceCountedFutureImpl* api = ref_future();
  if (!database_ || !api) {
    if (delete_context != nullptr) {
      delete_context(context);
    }
    return Future<DataSnapshot>();
  }
  SafeFutureHandle<DataSnapshot> handle = api->SafeAlloc<DataSnapshot>(
      kDatabaseReferenceFnRunTransaction, DataSnapshot(nullptr));
  Future<DataSnapshot> result = MakeFuture(api, handle);
  auto state = std::make_shared<AsyncOpState<DataSnapshot>>(
      api, handle, result, context, delete_context);

  Repo::scheduler().Schedule(NewCallback(
      [](Repo::ThisRef ref, Path path,
         DoTransactionWithContext transaction_function,
         bool trigger_local_events,
         std::shared_ptr<AsyncOpState<DataSnapshot>> op_state) {
        Repo::ThisRefLock lock(&ref);
        if (lock.GetReference() != nullptr) {
          op_state->Disarm();
          lock.GetReference()->StartTransaction(
              path, transaction_function, op_state->context,
              op_state->delete_context, trigger_local_events, op_state->api,
              op_state->handle);
        } else {
          op_state->FailIfNotDisarmed();
        }
      },
      database_->EnsureRepoRef(), query_spec_.path, transaction_function,
      trigger_local_events, state));

  return result;
}

Future<DataSnapshot> DatabaseReferenceInternal::RunTransactionLastResult() {
  ReferenceCountedFutureImpl* api = ref_future();
  return api ? static_cast<const Future<DataSnapshot>&>(
                   api->LastResult(kDatabaseReferenceFnRunTransaction))
             : Future<DataSnapshot>();
}

Future<void> DatabaseReferenceInternal::SetPriority(const Variant& priority) {
  ReferenceCountedFutureImpl* api = ref_future();
  if (!database_ || !api) return Future<void>();
  if (SetValueAndPriorityLastResult().status() == kFutureStatusPending) {
    SafeFutureHandle<void> handle =
        AllocSlotOrTemp(api, kDatabaseReferenceFnSetPriority);
    api->Complete(handle, kErrorConflictingOperationInProgress,
                  kErrorMsgConflictSetPriority);
    return MakeFuture(api, handle);
  }
  if (!priority.is_fundamental_type()) {
    SafeFutureHandle<void> handle =
        AllocSlotOrTemp(api, kDatabaseReferenceFnSetPriority);
    api->Complete(handle, kErrorInvalidVariantType,
                  kErrorMsgInvalidVariantForPriority);
    return MakeFuture(api, handle);
  }
  SafeFutureHandle<void> handle =
      api->SafeAlloc<void>(kDatabaseReferenceFnSetPriority);
  Future<void> result = MakeFuture(api, handle);
  auto state = std::make_shared<AsyncOpState<void>>(api, handle, result);

  Repo::scheduler().Schedule(NewCallback(
      [](Repo::ThisRef ref, Path path, Variant priority,
         std::shared_ptr<AsyncOpState<void>> op_state) {
        Repo::ThisRefLock lock(&ref);
        if (lock.GetReference() != nullptr) {
          op_state->Disarm();
          ConvertVectorToMap(&priority);
          lock.GetReference()->SetValue(path, priority, op_state->api,
                                        op_state->handle);
        } else {
          op_state->FailIfNotDisarmed();
        }
      },
      database_->EnsureRepoRef(), query_spec_.path.GetChild(kPriorityKey),
      priority, state));
  return result;
}

Future<void> DatabaseReferenceInternal::SetPriorityLastResult() {
  ReferenceCountedFutureImpl* api = ref_future();
  return api ? static_cast<const Future<void>&>(
                   api->LastResult(kDatabaseReferenceFnSetPriority))
             : Future<void>();
}

Future<void> DatabaseReferenceInternal::SetValue(const Variant& value) {
  ReferenceCountedFutureImpl* api = ref_future();
  if (!database_ || !api) return Future<void>();
  if (SetValueAndPriorityLastResult().status() == kFutureStatusPending) {
    SafeFutureHandle<void> handle =
        AllocSlotOrTemp(api, kDatabaseReferenceFnSetValue);
    api->Complete(handle, kErrorConflictingOperationInProgress,
                  kErrorMsgConflictSetValue);
    return MakeFuture(api, handle);
  }
  SafeFutureHandle<void> handle =
      api->SafeAlloc<void>(kDatabaseReferenceFnSetValue);
  Future<void> result = MakeFuture(api, handle);
  auto state = std::make_shared<AsyncOpState<void>>(api, handle, result);

  Repo::scheduler().Schedule(NewCallback(
      [](Repo::ThisRef ref, Path path, Variant value,
         std::shared_ptr<AsyncOpState<void>> op_state) {
        Repo::ThisRefLock lock(&ref);
        if (lock.GetReference() != nullptr) {
          op_state->Disarm();
          ConvertVectorToMap(&value);
          lock.GetReference()->SetValue(path, value, op_state->api,
                                        op_state->handle);
        } else {
          op_state->FailIfNotDisarmed();
        }
      },
      database_->EnsureRepoRef(), query_spec_.path, value, state));
  return result;
}

Future<void> DatabaseReferenceInternal::SetValueLastResult() {
  ReferenceCountedFutureImpl* api = ref_future();
  return api ? static_cast<const Future<void>&>(
                   api->LastResult(kDatabaseReferenceFnSetValue))
             : Future<void>();
}

Future<void> DatabaseReferenceInternal::SetValueAndPriority(
    const Variant& value, const Variant& priority) {
  ReferenceCountedFutureImpl* api = ref_future();
  if (!database_ || !api) return Future<void>();
  if (SetValueLastResult().status() == kFutureStatusPending) {
    SafeFutureHandle<void> handle =
        AllocSlotOrTemp(api, kDatabaseReferenceFnSetValueAndPriority);
    api->Complete(handle, kErrorConflictingOperationInProgress,
                  kErrorMsgConflictSetValue);
    return MakeFuture(api, handle);
  }
  if (SetPriorityLastResult().status() == kFutureStatusPending) {
    SafeFutureHandle<void> handle =
        AllocSlotOrTemp(api, kDatabaseReferenceFnSetValueAndPriority);
    api->Complete(handle, kErrorConflictingOperationInProgress,
                  kErrorMsgConflictSetPriority);
    return MakeFuture(api, handle);
  }
  if (!priority.is_fundamental_type()) {
    SafeFutureHandle<void> handle =
        AllocSlotOrTemp(api, kDatabaseReferenceFnSetValueAndPriority);
    api->Complete(handle, kErrorInvalidVariantType,
                  kErrorMsgInvalidVariantForPriority);
    return MakeFuture(api, handle);
  }
  SafeFutureHandle<void> handle =
      api->SafeAlloc<void>(kDatabaseReferenceFnSetValueAndPriority);
  // https://firebase.google.com/docs/reference/rest/database/#section-priorities
  //
  // To write priority and data at the same time, you can add a .priority
  // child to the JSON payload.
  //
  // To write priority and a primitive value (e.g. a string) at the same time,
  // you can add a .priority child and put the primitive value in a .value
  // child
  Variant value_priority = value;
  ConvertVectorToMap(&value_priority);
  if (value_priority.is_container_type()) {
    value_priority.map()[kVirtualChildKeyPriority] = priority;
  } else {
    value_priority = std::map<Variant, Variant>{
        std::make_pair(kVirtualChildKeyValue, std::move(value_priority)),
        std::make_pair(kVirtualChildKeyPriority, priority)};
  }
  Future<void> result = MakeFuture(api, handle);
  auto state = std::make_shared<AsyncOpState<void>>(api, handle, result);

  Repo::scheduler().Schedule(NewCallback(
      [](Repo::ThisRef ref, Path path, Variant value_priority,
         std::shared_ptr<AsyncOpState<void>> op_state) {
        Repo::ThisRefLock lock(&ref);
        if (lock.GetReference() != nullptr) {
          op_state->Disarm();
          ConvertVectorToMap(&value_priority);
          lock.GetReference()->SetValue(path, value_priority, op_state->api,
                                        op_state->handle);
        } else {
          op_state->FailIfNotDisarmed();
        }
      },
      database_->EnsureRepoRef(), query_spec_.path, std::move(value_priority),
      state));
  return result;
}

Future<void> DatabaseReferenceInternal::SetValueAndPriorityLastResult() {
  ReferenceCountedFutureImpl* api = ref_future();
  return api ? static_cast<const Future<void>&>(
                   api->LastResult(kDatabaseReferenceFnSetValueAndPriority))
             : Future<void>();
}

Future<void> DatabaseReferenceInternal::UpdateChildren(const Variant& values) {
  ReferenceCountedFutureImpl* api = ref_future();
  if (!database_ || !api) return Future<void>();
  if (!values.is_map()) {
    SafeFutureHandle<void> handle =
        AllocSlotOrTemp(api, kDatabaseReferenceFnUpdateChildren);
    api->Complete(handle, kErrorInvalidVariantType,
                  kErrorMsgInvalidVariantForUpdateChildren);
    return MakeFuture(api, handle);
  }
  SafeFutureHandle<void> handle =
      api->SafeAlloc<void>(kDatabaseReferenceFnUpdateChildren);
  Future<void> result = MakeFuture(api, handle);
  auto state = std::make_shared<AsyncOpState<void>>(api, handle, result);

  Repo::scheduler().Schedule(NewCallback(
      [](Repo::ThisRef ref, Path path, Variant values,
         std::shared_ptr<AsyncOpState<void>> op_state) {
        Repo::ThisRefLock lock(&ref);
        if (lock.GetReference() != nullptr) {
          op_state->Disarm();
          ConvertVectorToMap(&values);
          lock.GetReference()->UpdateChildren(path, values, op_state->api,
                                              op_state->handle);
        } else {
          op_state->FailIfNotDisarmed();
        }
      },
      database_->EnsureRepoRef(), query_spec_.path, values, state));
  return result;
}

Future<void> DatabaseReferenceInternal::UpdateChildrenLastResult() {
  ReferenceCountedFutureImpl* api = ref_future();
  return api ? static_cast<const Future<void>&>(
                   api->LastResult(kDatabaseReferenceFnUpdateChildren))
             : Future<void>();
}

DisconnectionHandler* DatabaseReferenceInternal::OnDisconnect() {
  if (!database_) return nullptr;
  return new DisconnectionHandler(
      new DisconnectionHandlerInternal(database_, query_spec_.path));
}

void DatabaseReferenceInternal::GoOffline() {
  if (database_) database_->GoOffline();
}

void DatabaseReferenceInternal::GoOnline() {
  if (database_) database_->GoOnline();
}

ReferenceCountedFutureImpl* DatabaseReferenceInternal::ref_future() {
  return database_ ? database_->future_manager().GetFutureApi(&future_api_id_)
                   : nullptr;
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
