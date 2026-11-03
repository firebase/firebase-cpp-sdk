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

#include "database/src/desktop/disconnection_desktop.h"

#include <utility>

#include "app/src/future_manager.h"
#include "database/src/common/database_reference.h"
#include "database/src/desktop/core/repo.h"
#include "database/src/desktop/database_desktop.h"
#include "database/src/desktop/database_reference_desktop.h"
#include "database/src/desktop/util_desktop.h"
#include "database/src/include/firebase/database/disconnection.h"

namespace firebase {
namespace database {
namespace internal {

namespace {

enum DisconnectionHandlerFn {
  kDisconnectionHandlerFnCancel = 0,
  kDisconnectionHandlerFnRemoveValue,
  kDisconnectionHandlerFnSetValue,
  kDisconnectionHandlerFnSetValueAndPriority,
  kDisconnectionHandlerFnUpdateChildren,
  kDisconnectionHandlerFnCount,
};

const char kVirtualChildKeyValue[] = ".value";
const char kVirtualChildKeyPriority[] = ".priority";

SafeFutureHandle<void> AllocSlotOrTemp(ReferenceCountedFutureImpl* api,
                                       int fn_idx) {
  if (api->LastResult(fn_idx).status() == kFutureStatusPending) {
    return api->SafeAlloc<void>();
  }
  return api->SafeAlloc<void>(fn_idx);
}

}  // namespace

DisconnectionHandlerInternal::DisconnectionHandlerInternal(
    DatabaseInternal* database, const Path& path)
    : database_(database), path_(path) {
  if (database_) {
    database_->future_manager().AllocFutureApi(this,
                                               kDisconnectionHandlerFnCount);
    database_->cleanup().RegisterObject(this, [](void* obj) {
      auto* self = static_cast<DisconnectionHandlerInternal*>(obj);
      if (self->database_) {
        self->database_->future_manager().ReleaseFutureApi(self);
        self->database_ = nullptr;
      }
    });
  }
}

DisconnectionHandlerInternal::~DisconnectionHandlerInternal() {
  if (database_) {
    database_->cleanup().UnregisterObject(this);
    database_->future_manager().ReleaseFutureApi(this);
    database_ = nullptr;
  }
}

Future<void> DisconnectionHandlerInternal::Cancel() {
  ReferenceCountedFutureImpl* api = future();
  if (!database_ || !api) return Future<void>();
  SafeFutureHandle<void> handle =
      api->SafeAlloc<void>(kDisconnectionHandlerFnCancel);
  database_->repo()->OnDisconnectCancel(handle, api, path_);
  return MakeFuture(api, handle);
}

Future<void> DisconnectionHandlerInternal::CancelLastResult() {
  ReferenceCountedFutureImpl* api = future();
  return api ? static_cast<const Future<void>&>(
                   api->LastResult(kDisconnectionHandlerFnCancel))
             : Future<void>();
}

Future<void> DisconnectionHandlerInternal::RemoveValue() {
  ReferenceCountedFutureImpl* api = future();
  if (!database_ || !api) return Future<void>();
  if (SetValueLastResult().status() == kFutureStatusPending ||
      SetValueAndPriorityLastResult().status() == kFutureStatusPending) {
    SafeFutureHandle<void> handle =
        AllocSlotOrTemp(api, kDisconnectionHandlerFnRemoveValue);
    api->Complete(handle, kErrorConflictingOperationInProgress,
                  kErrorMsgConflictSetValue);
    return MakeFuture(api, handle);
  }
  SafeFutureHandle<void> handle =
      api->SafeAlloc<void>(kDisconnectionHandlerFnRemoveValue);
  database_->repo()->OnDisconnectSetValue(handle, api, path_, Variant::Null());
  return MakeFuture(api, handle);
}

Future<void> DisconnectionHandlerInternal::RemoveValueLastResult() {
  ReferenceCountedFutureImpl* api = future();
  return api ? static_cast<const Future<void>&>(
                   api->LastResult(kDisconnectionHandlerFnRemoveValue))
             : Future<void>();
}

Future<void> DisconnectionHandlerInternal::SetValue(Variant value) {
  ReferenceCountedFutureImpl* api = future();
  if (!database_ || !api) return Future<void>();
  if (SetValueAndPriorityLastResult().status() == kFutureStatusPending ||
      RemoveValueLastResult().status() == kFutureStatusPending) {
    SafeFutureHandle<void> handle =
        AllocSlotOrTemp(api, kDisconnectionHandlerFnSetValue);
    api->Complete(handle, kErrorConflictingOperationInProgress,
                  kErrorMsgConflictSetValue);
    return MakeFuture(api, handle);
  }
  SafeFutureHandle<void> handle =
      api->SafeAlloc<void>(kDisconnectionHandlerFnSetValue);
  ConvertVectorToMap(&value);
  database_->repo()->OnDisconnectSetValue(handle, api, path_, std::move(value));
  return MakeFuture(api, handle);
}

Future<void> DisconnectionHandlerInternal::SetValueLastResult() {
  ReferenceCountedFutureImpl* api = future();
  return api ? static_cast<const Future<void>&>(
                   api->LastResult(kDisconnectionHandlerFnSetValue))
             : Future<void>();
}

Future<void> DisconnectionHandlerInternal::SetValueAndPriority(
    Variant value, Variant priority) {
  ReferenceCountedFutureImpl* api = future();
  if (!database_ || !api) return Future<void>();
  if (SetValueLastResult().status() == kFutureStatusPending ||
      RemoveValueLastResult().status() == kFutureStatusPending) {
    SafeFutureHandle<void> handle =
        AllocSlotOrTemp(api, kDisconnectionHandlerFnSetValueAndPriority);
    api->Complete(handle, kErrorConflictingOperationInProgress,
                  kErrorMsgConflictSetValue);
    return MakeFuture(api, handle);
  }
  if (!priority.is_fundamental_type()) {
    SafeFutureHandle<void> handle =
        AllocSlotOrTemp(api, kDisconnectionHandlerFnSetValueAndPriority);
    api->Complete(handle, kErrorInvalidVariantType,
                  kErrorMsgInvalidVariantForPriority);
    return MakeFuture(api, handle);
  }
  SafeFutureHandle<void> handle =
      api->SafeAlloc<void>(kDisconnectionHandlerFnSetValueAndPriority);
  ConvertVectorToMap(&value);
  Variant data;
  if (value.is_container_type()) {
    data = std::move(value);
    data.map()[kVirtualChildKeyPriority] = std::move(priority);
  } else {
    data = Variant::EmptyMap();
    data.map()[kVirtualChildKeyValue] = std::move(value);
    data.map()[kVirtualChildKeyPriority] = std::move(priority);
  }
  database_->repo()->OnDisconnectSetValue(handle, api, path_, std::move(data));
  return MakeFuture(api, handle);
}

Future<void> DisconnectionHandlerInternal::SetValueAndPriorityLastResult() {
  ReferenceCountedFutureImpl* api = future();
  return api ? static_cast<const Future<void>&>(
                   api->LastResult(kDisconnectionHandlerFnSetValueAndPriority))
             : Future<void>();
}

Future<void> DisconnectionHandlerInternal::UpdateChildren(Variant values) {
  ReferenceCountedFutureImpl* api = future();
  if (!database_ || !api) return Future<void>();
  if (!values.is_map()) {
    SafeFutureHandle<void> handle =
        AllocSlotOrTemp(api, kDisconnectionHandlerFnUpdateChildren);
    api->Complete(handle, kErrorInvalidVariantType,
                  kErrorMsgInvalidVariantForUpdateChildren);
    return MakeFuture(api, handle);
  }
  SafeFutureHandle<void> handle =
      api->SafeAlloc<void>(kDisconnectionHandlerFnUpdateChildren);
  ConvertVectorToMap(&values);
  database_->repo()->OnDisconnectUpdate(handle, api, path_, std::move(values));
  return MakeFuture(api, handle);
}

Future<void> DisconnectionHandlerInternal::UpdateChildrenLastResult() {
  ReferenceCountedFutureImpl* api = future();
  return api ? static_cast<const Future<void>&>(
                   api->LastResult(kDisconnectionHandlerFnUpdateChildren))
             : Future<void>();
}

ReferenceCountedFutureImpl* DisconnectionHandlerInternal::future() {
  return database_ ? database_->future_manager().GetFutureApi(this) : nullptr;
}

DisconnectionHandler
DisconnectionHandlerInternal::GetInvalidDisconnectionHandler() {
  return DisconnectionHandler(nullptr);
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
