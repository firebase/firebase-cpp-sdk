// Copyright 2016 Google LLC
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

#include "database/src/ios/disconnection_ios.h"

#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/variant.h"
#include "app/src/util_ios.h"
#include "database/src/common/database_reference.h"
#include "database/src/ios/completion_block_ios.h"
#include "database/src/ios/database_ios.h"
#include "database/src/ios/database_reference_ios.h"

namespace firebase {
namespace database {
namespace internal {

using util::VariantToId;

enum DisconnectionHandlerFn {
  kDisconnectionHandlerFnCancel = 0,
  kDisconnectionHandlerFnRemoveValue,
  kDisconnectionHandlerFnSetValue,
  kDisconnectionHandlerFnSetValueAndPriority,
  kDisconnectionHandlerFnUpdateChildren,
  kDisconnectionHandlerFnCount,
};

DisconnectionHandlerInternal::DisconnectionHandlerInternal(
    DatabaseInternal* database, UniquePtr<FIRDatabaseReferencePointer> impl)
    : database_(database), impl_(std::move(impl)) {
  database_->future_manager().AllocFutureApi(this, kDisconnectionHandlerFnCount);
}

DisconnectionHandlerInternal::~DisconnectionHandlerInternal() {
  database_->future_manager().ReleaseFutureApi(this);
}

Future<void> DisconnectionHandlerInternal::Cancel() {
  SafeFutureHandle<void> handle = future()->SafeAlloc<void>(kDisconnectionHandlerFnCancel);
  [impl() cancelDisconnectOperationsWithCompletionBlock:CreateCompletionBlock(handle, future())];
  return MakeFuture(future(), handle);
}

Future<void> DisconnectionHandlerInternal::CancelLastResult() {
  return static_cast<const Future<void>&>(future()->LastResult(kDisconnectionHandlerFnCancel));
}

Future<void> DisconnectionHandlerInternal::RemoveValue() {
  SafeFutureHandle<void> handle = future()->SafeAlloc<void>(kDisconnectionHandlerFnRemoveValue);
  [impl() onDisconnectRemoveValueWithCompletionBlock:CreateCompletionBlock(handle, future())];
  return MakeFuture(future(), handle);
}

Future<void> DisconnectionHandlerInternal::RemoveValueLastResult() {
  return static_cast<const Future<void>&>(future()->LastResult(kDisconnectionHandlerFnRemoveValue));
}

Future<void> DisconnectionHandlerInternal::SetValue(Variant value) {
  SafeFutureHandle<void> handle = future()->SafeAlloc<void>(kDisconnectionHandlerFnSetValue);
  if (SetValueAndPriorityLastResult().status() == kFutureStatusPending) {
    future()->Complete(handle, kErrorConflictingOperationInProgress, kErrorMsgConflictSetValue);
  } else {
    [impl() onDisconnectSetValue:VariantToId(value)
             withCompletionBlock:CreateCompletionBlock(handle, future())];
  }
  return MakeFuture(future(), handle);
}

Future<void> DisconnectionHandlerInternal::SetValueLastResult() {
  return static_cast<const Future<void>&>(future()->LastResult(kDisconnectionHandlerFnSetValue));
}

Future<void> DisconnectionHandlerInternal::SetValueAndPriority(Variant value, Variant priority) {
  SafeFutureHandle<void> handle =
      future()->SafeAlloc<void>(kDisconnectionHandlerFnSetValueAndPriority);
  if (SetValueLastResult().status() == kFutureStatusPending) {
    future()->Complete(handle, kErrorConflictingOperationInProgress, kErrorMsgConflictSetValue);
  } else if (!IsValidPriority(priority)) {
    future()->Complete(handle, kErrorInvalidVariantType, kErrorMsgInvalidVariantForPriority);
  } else {
    [impl() onDisconnectSetValue:VariantToId(value)
                     andPriority:VariantToId(priority)
             withCompletionBlock:CreateCompletionBlock(handle, future())];
  }
  return MakeFuture(future(), handle);
}

Future<void> DisconnectionHandlerInternal::SetValueAndPriorityLastResult() {
  return static_cast<const Future<void>&>(
      future()->LastResult(kDisconnectionHandlerFnSetValueAndPriority));
}

Future<void> DisconnectionHandlerInternal::UpdateChildren(Variant values) {
  SafeFutureHandle<void> handle = future()->SafeAlloc<void>(kDisconnectionHandlerFnUpdateChildren);
  if (!values.is_map()) {
    future()->Complete(handle, kErrorInvalidVariantType, kErrorMsgInvalidVariantForUpdateChildren);
  } else {
    id values_id = VariantToId(values);
    NSDictionary *values_dict = (NSDictionary *)values_id;
    [impl() onDisconnectUpdateChildValues:values_dict
                      withCompletionBlock:CreateCompletionBlock(handle, future())];
  }
  return MakeFuture(future(), handle);
}

Future<void> DisconnectionHandlerInternal::UpdateChildrenLastResult() {
  return
      static_cast<const Future<void>&>(future()->LastResult(kDisconnectionHandlerFnUpdateChildren));
}

FIRDatabaseReference* _Nonnull DisconnectionHandlerInternal::impl() const { return impl_->get(); }

ReferenceCountedFutureImpl* DisconnectionHandlerInternal::future() {
  return database_->future_manager().GetFutureApi(this);
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
