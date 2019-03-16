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
#include "app/src/future_manager.h"
#include "database/src/common/database_reference.h"
#include "database/src/desktop/core/repo.h"
#include "database/src/desktop/database_desktop.h"
#include "database/src/desktop/database_reference_desktop.h"
#include "database/src/include/firebase/database/disconnection.h"

namespace firebase {
namespace database {
namespace internal {

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

DisconnectionHandlerInternal::DisconnectionHandlerInternal(
    DatabaseInternal* database, const Path& path)
    : database_(database), path_(path) {
  database_->future_manager().AllocFutureApi(this,
                                             kDisconnectionHandlerFnCount);
}

DisconnectionHandlerInternal::~DisconnectionHandlerInternal() {
  database_->future_manager().ReleaseFutureApi(this);
}

Future<void> DisconnectionHandlerInternal::Cancel() {
  SafeFutureHandle<void> handle =
      future()->SafeAlloc<void>(kDisconnectionHandlerFnCancel);
  database_->repo()->OnDisconnectCancel(handle, future(), path_);
  return MakeFuture(future(), handle);
}

Future<void> DisconnectionHandlerInternal::CancelLastResult() {
  return static_cast<const Future<void>&>(
      future()->LastResult(kDisconnectionHandlerFnCancel));
}

Future<void> DisconnectionHandlerInternal::RemoveValue() {
  SafeFutureHandle<void> handle =
      future()->SafeAlloc<void>(kDisconnectionHandlerFnRemoveValue);
  database_->repo()->OnDisconnectSetValue(handle, future(), path_,
                                          Variant::Null());
  return MakeFuture(future(), handle);
}

Future<void> DisconnectionHandlerInternal::RemoveValueLastResult() {
  return static_cast<const Future<void>&>(
      future()->LastResult(kDisconnectionHandlerFnRemoveValue));
}

Future<void> DisconnectionHandlerInternal::SetValue(Variant value) {
  SafeFutureHandle<void> handle =
      future()->SafeAlloc<void>(kDisconnectionHandlerFnSetValue);
  if (SetValueAndPriorityLastResult().status() == kFutureStatusPending) {
    future()->Complete(handle, kErrorConflictingOperationInProgress,
                       kErrorMsgConflictSetValue);
  } else {
    database_->repo()->OnDisconnectSetValue(handle, future(), path_, value);
  }
  return MakeFuture(future(), handle);
}

Future<void> DisconnectionHandlerInternal::SetValueLastResult() {
  return static_cast<const Future<void>&>(
      future()->LastResult(kDisconnectionHandlerFnSetValue));
}

Future<void> DisconnectionHandlerInternal::SetValueAndPriority(
    Variant value, Variant priority) {
  SafeFutureHandle<void> handle =
      future()->SafeAlloc<void>(kDisconnectionHandlerFnSetValueAndPriority);
  (void)handle;
  if (SetValueLastResult().status() == kFutureStatusPending) {
    future()->Complete(handle, kErrorConflictingOperationInProgress,
                       kErrorMsgConflictSetValue);
  } else if (!priority.is_fundamental_type()) {
    future()->Complete(handle, kErrorInvalidVariantType,
                       kErrorMsgInvalidVariantForPriority);
  } else {
    Variant data = Variant::EmptyMap();
    data.map()[kVirtualChildKeyValue] = value;
    data.map()[kVirtualChildKeyPriority] = priority;
    database_->repo()->OnDisconnectSetValue(handle, future(), path_, data);
  }
  return MakeFuture(future(), handle);
}

Future<void> DisconnectionHandlerInternal::SetValueAndPriorityLastResult() {
  return static_cast<const Future<void>&>(
      future()->LastResult(kDisconnectionHandlerFnSetValueAndPriority));
}

Future<void> DisconnectionHandlerInternal::UpdateChildren(Variant values) {
  SafeFutureHandle<void> handle =
      future()->SafeAlloc<void>(kDisconnectionHandlerFnUpdateChildren);
  if (!values.is_map()) {
    future()->Complete(handle, kErrorInvalidVariantType,
                       kErrorMsgInvalidVariantForUpdateChildren);
  } else {
    database_->repo()->OnDisconnectUpdate(handle, future(), path_, values);
  }
  return MakeFuture(future(), handle);
}

Future<void> DisconnectionHandlerInternal::UpdateChildrenLastResult() {
  return static_cast<const Future<void>&>(
      future()->LastResult(kDisconnectionHandlerFnUpdateChildren));
}

ReferenceCountedFutureImpl* DisconnectionHandlerInternal::future() {
  return database_->future_manager().GetFutureApi(this);
}

DisconnectionHandler
DisconnectionHandlerInternal::GetInvalidDisconnectionHandler() {
  return DisconnectionHandler(nullptr);
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
