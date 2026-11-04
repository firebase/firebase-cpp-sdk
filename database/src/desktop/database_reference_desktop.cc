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
  AsyncOpState(Repo::ThisRef ref_in, ReferenceCountedFutureImpl* api_in,
               SafeFutureHandle<T> handle_in,
               void* context_in = nullptr,
               void (*delete_context_in)(void*) = nullptr)
      : ref(std::move(ref_in)),
        api(api_in),
        handle(handle_in),
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
    Repo::ThisRefLock lock(&ref);
    if (lock.GetReference() != nullptr) {
      if (api != nullptr && api->GetFutureStatus(handle.get()) == kFutureStatusPending) {
        api->Complete(handle, kErrorUnknownError,
                      "The database is being destroyed.");
      }
    }
  }

  Repo::ThisRef ref;
  ReferenceCountedFutureImpl* api;
  SafeFutureHandle<T> handle;
  void* context;
  void (*delete_context)(void*);
  bool disarmed;
};


}  // namespace

const char kVirtualChildKeyValue[] = ".value";
const char kVirtualChildKeyPriority[] = ".priority";

DatabaseReferenceInternal::DatabaseReferenceInternal(DatabaseInternal* database,
                                                     const Path& path)
    : QueryInternal(database, QuerySpec(path)), ref_future_impl_(kDatabaseReferenceFnCount) {
  
}

DatabaseReferenceInternal::~DatabaseReferenceInternal() {
  
}

DatabaseReferenceInternal::DatabaseReferenceInternal(
    const DatabaseReferenceInternal& internal)
    : QueryInternal(internal), ref_future_impl_(kDatabaseReferenceFnCount) {
  
}

DatabaseReferenceInternal& DatabaseReferenceInternal::operator=(
    const DatabaseReferenceInternal& internal) {
  if (&internal == this) return *this;
  if (database_ != internal.database_) {
    
    
  }
  QueryInternal::operator=(internal);
  return *this;
}

DatabaseReferenceInternal::DatabaseReferenceInternal(
    DatabaseReferenceInternal&& internal) noexcept
    : QueryInternal(std::move(internal)), ref_future_impl_(kDatabaseReferenceFnCount) {
  internal.database_ = nullptr;
  
}

DatabaseReferenceInternal& DatabaseReferenceInternal::operator=(
    DatabaseReferenceInternal&& internal) noexcept {
  if (&internal == this) return *this;
  
  QueryInternal::operator=(std::move(internal));
  internal.database_ = nullptr;
  
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

void DatabaseReferenceInternal::RemoveValueImpl(SafeFutureHandle<void> handle) {
  ReferenceCountedFutureImpl* api = ref_future();
  if (!database_ || !api) return;
  auto state = std::make_shared<AsyncOpState<void>>(database_->repo()->this_ref(), api, handle);
  Repo::scheduler().Schedule(NewCallback(
      [](Repo::ThisRef ref, Path path, std::shared_ptr<AsyncOpState<void>> op_state) {
        Repo::ThisRefLock lock(&ref);
        if (lock.GetReference() != nullptr) {
          op_state->Disarm();
          lock.GetReference()->SetValue(path, Variant::Null(), op_state->api, op_state->handle);
        }
      },
      database_->repo()->this_ref(), query_spec_.path, state));
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
      database_->repo()->this_ref(), api, handle, context, delete_context);

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
      database_->repo()->this_ref(), query_spec_.path, transaction_function,
      trigger_local_events, state));

  return result;
}


void DatabaseReferenceInternal::SetPriorityImpl(const Variant& priority, SafeFutureHandle<void> handle) {
  ReferenceCountedFutureImpl* api = ref_future();
  if (!database_ || !api) return;
  auto state = std::make_shared<AsyncOpState<void>>(database_->repo()->this_ref(), api, handle);
  Repo::scheduler().Schedule(NewCallback(
      [](Repo::ThisRef ref, Path path, Variant prio, std::shared_ptr<AsyncOpState<void>> op_state) {
        Repo::ThisRefLock lock(&ref);
        if (lock.GetReference() != nullptr) {
          op_state->Disarm();
          ConvertVectorToMap(&prio);
          lock.GetReference()->SetValue(path.GetChild(kPriorityKey), prio, op_state->api, op_state->handle);
        }
      },
      database_->repo()->this_ref(), query_spec_.path, priority, state));
}


void DatabaseReferenceInternal::SetValueImpl(const Variant& value, SafeFutureHandle<void> handle) {
  ReferenceCountedFutureImpl* api = ref_future();
  if (!database_ || !api) return;
  auto state = std::make_shared<AsyncOpState<void>>(database_->repo()->this_ref(), api, handle);
  Repo::scheduler().Schedule(NewCallback(
      [](Repo::ThisRef ref, Path path, Variant val, std::shared_ptr<AsyncOpState<void>> op_state) {
        Repo::ThisRefLock lock(&ref);
        if (lock.GetReference() != nullptr) {
          op_state->Disarm();
          ConvertVectorToMap(&val);
          lock.GetReference()->SetValue(path, val, op_state->api, op_state->handle);
        }
      },
      database_->repo()->this_ref(), query_spec_.path, value, state));
}


void DatabaseReferenceInternal::SetValueAndPriorityImpl(
    const Variant& value, const Variant& priority, SafeFutureHandle<void> handle) {
  ReferenceCountedFutureImpl* api = ref_future();
  if (!database_ || !api) return;
  auto state = std::make_shared<AsyncOpState<void>>(database_->repo()->this_ref(), api, handle);
  Repo::scheduler().Schedule(NewCallback(
      [](Repo::ThisRef ref, Path path, Variant val, Variant prio, std::shared_ptr<AsyncOpState<void>> op_state) {
        Repo::ThisRefLock lock(&ref);
        if (lock.GetReference() != nullptr) {
          op_state->Disarm();
          ConvertVectorToMap(&val);
          Variant data;
          if (val.is_container_type()) {
            data = std::move(val);
            data.map()[kVirtualChildKeyPriority] = std::move(prio);
          } else {
            data = Variant::EmptyMap();
            data.map()[kVirtualChildKeyValue] = std::move(val);
            data.map()[kVirtualChildKeyPriority] = std::move(prio);
          }
          lock.GetReference()->SetValue(path, data, op_state->api, op_state->handle);
        }
      },
      database_->repo()->this_ref(), query_spec_.path, value, priority, state));
}


void DatabaseReferenceInternal::UpdateChildrenImpl(const Variant& values, SafeFutureHandle<void> handle) {
  ReferenceCountedFutureImpl* api = ref_future();
  if (!database_ || !api) return;
  auto state = std::make_shared<AsyncOpState<void>>(database_->repo()->this_ref(), api, handle);
  Repo::scheduler().Schedule(NewCallback(
      [](Repo::ThisRef ref, Path path, Variant vals, std::shared_ptr<AsyncOpState<void>> op_state) {
        Repo::ThisRefLock lock(&ref);
        if (lock.GetReference() != nullptr) {
          op_state->Disarm();
          ConvertVectorToMap(&vals);
          lock.GetReference()->UpdateChildren(path, vals, op_state->api, op_state->handle);
        }
      },
      database_->repo()->this_ref(), query_spec_.path, values, state));
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
  return database_ ? &ref_future_impl_ : nullptr;
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
