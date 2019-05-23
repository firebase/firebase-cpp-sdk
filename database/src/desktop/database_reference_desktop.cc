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
#include <stdio.h>
#include <sstream>
#include "app/rest/util.h"
#include "app/src/variant_util.h"
#include "database/src/common/database_reference.h"
#include "database/src/desktop/database_desktop.h"
#include "database/src/desktop/disconnection_desktop.h"
#include "database/src/desktop/util_desktop.h"

#include "flatbuffers/util.h"

namespace firebase {

using callback::NewCallback;

namespace database {
namespace internal {

const char kVirtualChildKeyValue[] = ".value";
const char kVirtualChildKeyPriority[] = ".priority";

DatabaseReferenceInternal::DatabaseReferenceInternal(DatabaseInternal* database,
                                                     const Path& path)
    : QueryInternal(database, QuerySpec(path)) {
  database_->future_manager().AllocFutureApi(&future_api_id_,
                                             kDatabaseReferenceFnCount);
}

DatabaseReferenceInternal::DatabaseReferenceInternal(
    const DatabaseReferenceInternal& internal)
    : QueryInternal(internal) {
  database_->future_manager().AllocFutureApi(&future_api_id_,
                                             kDatabaseReferenceFnCount);
}

DatabaseReferenceInternal& DatabaseReferenceInternal::operator=(
    const DatabaseReferenceInternal& internal) {
  QueryInternal::operator=(internal);
  return *this;
}

#if defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)
DatabaseReferenceInternal::DatabaseReferenceInternal(
    DatabaseReferenceInternal&& internal) {
  database_->future_manager().MoveFutureApi(&internal.future_api_id_,
                                            &future_api_id_);
  QueryInternal::operator=(std::move(internal));
}

DatabaseReferenceInternal& DatabaseReferenceInternal::operator=(
    DatabaseReferenceInternal&& internal) {
  database_->future_manager().MoveFutureApi(&internal.future_api_id_,
                                            &future_api_id_);
  QueryInternal::operator=(std::move(internal));
  return *this;
}
#endif  // defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)

Database* DatabaseReferenceInternal::GetDatabase() const {
  return Database::GetInstance(database_->GetApp());
}

std::string DatabaseReferenceInternal::GetUrl() const {
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
  return new DatabaseReferenceInternal(database_, query_spec_.path.GetParent());
}

DatabaseReferenceInternal* DatabaseReferenceInternal::GetRoot() {
  return new DatabaseReferenceInternal(database_, Path());
}

DatabaseReferenceInternal* DatabaseReferenceInternal::Child(const char* path) {
  return new DatabaseReferenceInternal(database_,
                                       query_spec_.path.GetChild(path));
}

DatabaseReferenceInternal* DatabaseReferenceInternal::PushChild() {
  auto now = static_cast<int64_t>(time(nullptr)) * 1000L;
  std::string child = database_->name_generator().GeneratePushChildName(now);
  return new DatabaseReferenceInternal(database_,
                                       query_spec_.path.GetChild(child));
}

Future<void> DatabaseReferenceInternal::RemoveValue() {
  SafeFutureHandle<void> handle =
      ref_future()->SafeAlloc<void>(kDatabaseReferenceFnRemoveValue);

  Repo::scheduler().Schedule(NewCallback(
      [](Repo* repo, Path path, ReferenceCountedFutureImpl* api,
         SafeFutureHandle<void> handle) {
        repo->SetValue(path, Variant::Null(), api, handle);
      },
      database_->repo(), query_spec_.path, ref_future(), handle));
  return MakeFuture(ref_future(), handle);
}

Future<void> DatabaseReferenceInternal::RemoveValueLastResult() {
  return static_cast<const Future<void>&>(
      ref_future()->LastResult(kDatabaseReferenceFnRemoveValue));
}

Future<DataSnapshot> DatabaseReferenceInternal::RunTransaction(
    DoTransactionWithContext transaction_function, void* context,
    void (*delete_context)(void*), bool trigger_local_events) {
  SafeFutureHandle<DataSnapshot> handle = ref_future()->SafeAlloc<DataSnapshot>(
      kDatabaseReferenceFnRunTransaction, DataSnapshot(nullptr));

  Repo::scheduler().Schedule(NewCallback(
      [](Repo* repo, Path path, DoTransactionWithContext transaction_function,
         void* context, void (*delete_context)(void*),
         bool trigger_local_events, ReferenceCountedFutureImpl* api,
         SafeFutureHandle<DataSnapshot> handle) {
        repo->StartTransaction(path, transaction_function, context,
                               delete_context, trigger_local_events, api,
                               handle);
      },
      database_->repo(), query_spec_.path, transaction_function, context,
      delete_context, trigger_local_events, ref_future(), handle));

  return MakeFuture(ref_future(), handle);
}

Future<DataSnapshot> DatabaseReferenceInternal::RunTransactionLastResult() {
  return static_cast<const Future<DataSnapshot>&>(
      ref_future()->LastResult(kDatabaseReferenceFnRunTransaction));
}

Future<void> DatabaseReferenceInternal::SetPriority(const Variant& priority) {
  SafeFutureHandle<void> handle =
      ref_future()->SafeAlloc<void>(kDatabaseReferenceFnSetPriority);
  if (SetValueAndPriorityLastResult().status() == kFutureStatusPending) {
    ref_future()->Complete(handle, kErrorConflictingOperationInProgress,
                           kErrorMsgConflictSetPriority);
  } else if (!priority.is_fundamental_type()) {
    ref_future()->Complete(handle, kErrorInvalidVariantType,
                           kErrorMsgInvalidVariantForPriority);
  } else {
    Repo::scheduler().Schedule(NewCallback(
        [](Repo* repo, Path path, Variant priority,
           ReferenceCountedFutureImpl* api, SafeFutureHandle<void> handle) {
          ConvertVectorToMap(&priority);
          repo->SetValue(path, priority, api, handle);
        },
        database_->repo(), query_spec_.path.GetChild(kPriorityKey), priority,
        ref_future(), handle));
  }
  return MakeFuture(ref_future(), handle);
}

Future<void> DatabaseReferenceInternal::SetPriorityLastResult() {
  return static_cast<const Future<void>&>(
      ref_future()->LastResult(kDatabaseReferenceFnSetPriority));
}

Future<void> DatabaseReferenceInternal::SetValue(const Variant& value) {
  SafeFutureHandle<void> handle =
      ref_future()->SafeAlloc<void>(kDatabaseReferenceFnSetValue);
  if (SetValueAndPriorityLastResult().status() == kFutureStatusPending) {
    ref_future()->Complete(handle, kErrorConflictingOperationInProgress,
                           kErrorMsgConflictSetValue);
  } else {
    Repo::scheduler().Schedule(NewCallback(
        [](Repo* repo, Path path, Variant value,
           ReferenceCountedFutureImpl* api, SafeFutureHandle<void> handle) {
          ConvertVectorToMap(&value);
          repo->SetValue(path, value, api, handle);
        },
        database_->repo(), query_spec_.path, value, ref_future(), handle));
  }
  return MakeFuture(ref_future(), handle);
}

Future<void> DatabaseReferenceInternal::SetValueLastResult() {
  return static_cast<const Future<void>&>(
      ref_future()->LastResult(kDatabaseReferenceFnSetValue));
}

Future<void> DatabaseReferenceInternal::SetValueAndPriority(
    const Variant& value, const Variant& priority) {
  SafeFutureHandle<void> handle =
      ref_future()->SafeAlloc<void>(kDatabaseReferenceFnSetValueAndPriority);
  if (SetValueLastResult().status() == kFutureStatusPending) {
    ref_future()->Complete(handle, kErrorConflictingOperationInProgress,
                           kErrorMsgConflictSetValue);
  } else if (SetPriorityLastResult().status() == kFutureStatusPending) {
    ref_future()->Complete(handle, kErrorConflictingOperationInProgress,
                           kErrorMsgConflictSetPriority);
  } else if (!priority.is_fundamental_type()) {
    ref_future()->Complete(handle, kErrorInvalidVariantType,
                           kErrorMsgInvalidVariantForPriority);
  } else {
    // https://firebase.google.com/docs/reference/rest/database/#section-priorities
    //
    // To write priority and data at the same time, you can add a .priority
    // child to the JSON payload.
    //
    // To write priority and a primitive value (e.g. a string) at the same time,
    // you can add a .priority child and put the primitive value in a .value
    // child
    Variant value_priority;
    if (value.is_container_type()) {
      value_priority = value;
      value_priority.map()[kVirtualChildKeyPriority] = priority;
    } else {
      value_priority = std::map<Variant, Variant>{
          std::make_pair(kVirtualChildKeyValue, value),
          std::make_pair(kVirtualChildKeyPriority, priority)};
    }
    Repo::scheduler().Schedule(NewCallback(
        [](Repo* repo, Path path, Variant value_priority,
           ReferenceCountedFutureImpl* api, SafeFutureHandle<void> handle) {
          ConvertVectorToMap(&value_priority);
          repo->SetValue(path, value_priority, api, handle);
        },
        database_->repo(), query_spec_.path, value_priority, ref_future(),
        handle));
  }
  return MakeFuture(ref_future(), handle);
}

Future<void> DatabaseReferenceInternal::SetValueAndPriorityLastResult() {
  return static_cast<const Future<void>&>(
      ref_future()->LastResult(kDatabaseReferenceFnSetValueAndPriority));
}

Future<void> DatabaseReferenceInternal::UpdateChildren(const Variant& values) {
  SafeFutureHandle<void> handle =
      ref_future()->SafeAlloc<void>(kDatabaseReferenceFnUpdateChildren);
  if (!values.is_map()) {
    ref_future()->Complete(handle, kErrorInvalidVariantType,
                           kErrorMsgInvalidVariantForUpdateChildren);
  } else {
    Repo::scheduler().Schedule(NewCallback(
        [](Repo* repo, Path path, Variant values,
           ReferenceCountedFutureImpl* api, SafeFutureHandle<void> handle) {
          ConvertVectorToMap(&values);
          repo->UpdateChildren(path, values, api, handle);
        },
        database_->repo(), query_spec_.path, values, ref_future(), handle));
  }
  return MakeFuture(ref_future(), handle);
}

Future<void> DatabaseReferenceInternal::UpdateChildrenLastResult() {
  return static_cast<const Future<void>&>(
      ref_future()->LastResult(kDatabaseReferenceFnUpdateChildren));
}

DisconnectionHandler* DatabaseReferenceInternal::OnDisconnect() {
  return new DisconnectionHandler(
      new DisconnectionHandlerInternal(database_, query_spec_.path));
}

void DatabaseReferenceInternal::GoOffline() { database_->GoOffline(); }

void DatabaseReferenceInternal::GoOnline() { database_->GoOnline(); }

ReferenceCountedFutureImpl* DatabaseReferenceInternal::ref_future() {
  return database_->future_manager().GetFutureApi(&future_api_id_);
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
