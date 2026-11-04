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

#include "database/src/include/firebase/database/disconnection.h"

#include <utility>

#include "app/src/include/firebase/internal/platform.h"
#include "database/src/common/database_reference.h"

#if FIREBASE_PLATFORM_ANDROID
#include "database/src/android/database_android.h"
#include "database/src/android/disconnection_android.h"
#elif FIREBASE_PLATFORM_IOS || FIREBASE_PLATFORM_TVOS
#include "database/src/ios/database_ios.h"
#include "database/src/ios/disconnection_ios.h"
#elif defined(FIREBASE_TARGET_DESKTOP)
#include "database/src/desktop/database_desktop.h"
#include "database/src/desktop/disconnection_desktop.h"
#else
#include "database/src/stub/database_stub.h"
#include "database/src/stub/disconnection_stub.h"
#endif

#include "database/src/common/cleanup.h"

namespace firebase {
namespace database {

using internal::DisconnectionHandlerInternal;

using CleanupFnDisconnectionHandler =
    CleanupFn<DisconnectionHandler, DisconnectionHandlerInternal>;

template <>
CleanupFnDisconnectionHandler::CreateInvalidObjectFn
    CleanupFnDisconnectionHandler::create_invalid_object =
        DisconnectionHandlerInternal::GetInvalidDisconnectionHandler;

DisconnectionHandler::DisconnectionHandler(
    DisconnectionHandlerInternal* internal)
    : internal_(internal) {
  CleanupFnDisconnectionHandler::Register(this, internal_);
}

DisconnectionHandler::~DisconnectionHandler() {
  CleanupFnDisconnectionHandler::Unregister(this, internal_);
  delete internal_;
}

namespace {
Future<void> CompleteDisconnectError(ReferenceCountedFutureImpl* api,
                                     internal::DisconnectionHandlerFn fn,
                                     Error err, const char* msg) {
  SafeFutureHandle<void> handle = api->SafeAlloc<void>(fn);
  api->Complete(handle, err, msg);
  return MakeFuture(api, handle);
}
}  // namespace

#define DISCONNECT_LAST_RESULT(Name, FnEnum)                              \
  Future<void> DisconnectionHandler::Name() {                             \
    return (internal_ && internal_->future())                             \
               ? static_cast<const Future<void>&>(                        \
                     internal_->future()->LastResult(internal::FnEnum))   \
               : Future<void>();                                          \
  }

DISCONNECT_LAST_RESULT(CancelLastResult, kDisconnectionHandlerFnCancel)
DISCONNECT_LAST_RESULT(RemoveValueLastResult, kDisconnectionHandlerFnRemoveValue)
DISCONNECT_LAST_RESULT(SetValueLastResult, kDisconnectionHandlerFnSetValue)
DISCONNECT_LAST_RESULT(SetValueAndPriorityLastResult,
                       kDisconnectionHandlerFnSetValueAndPriority)
DISCONNECT_LAST_RESULT(UpdateChildrenLastResult,
                       kDisconnectionHandlerFnUpdateChildren)

#undef DISCONNECT_LAST_RESULT

Future<void> DisconnectionHandler::Cancel() {
  if (!internal_ || !internal_->future()) return Future<void>();
  ReferenceCountedFutureImpl* api = internal_->future();
  SafeFutureHandle<void> handle =
      api->SafeAlloc<void>(internal::kDisconnectionHandlerFnCancel);
  internal_->CancelImpl(handle);
  return MakeFuture(api, handle);
}

Future<void> DisconnectionHandler::RemoveValue() {
  if (!internal_ || !internal_->future()) return Future<void>();
  ReferenceCountedFutureImpl* api = internal_->future();
  SafeFutureHandle<void> handle =
      api->SafeAlloc<void>(internal::kDisconnectionHandlerFnRemoveValue);
  internal_->RemoveValueImpl(handle);
  return MakeFuture(api, handle);
}

Future<void> DisconnectionHandler::SetValue(Variant value) {
  if (!internal_ || !internal_->future()) return Future<void>();
  ReferenceCountedFutureImpl* api = internal_->future();
  if (api->LastResult(internal::kDisconnectionHandlerFnSetValueAndPriority)
          .status() == kFutureStatusPending) {
    return CompleteDisconnectError(api, internal::kDisconnectionHandlerFnSetValue,
                                   kErrorConflictingOperationInProgress,
                                   internal::kErrorMsgConflictSetValue);
  }
  SafeFutureHandle<void> handle =
      api->SafeAlloc<void>(internal::kDisconnectionHandlerFnSetValue);
  internal_->SetValueImpl(std::move(value), handle);
  return MakeFuture(api, handle);
}

Future<void> DisconnectionHandler::SetValueAndPriority(Variant value,
                                                       Variant priority) {
  if (!internal_ || !internal_->future()) return Future<void>();
  ReferenceCountedFutureImpl* api = internal_->future();
  if (api->LastResult(internal::kDisconnectionHandlerFnSetValue).status() ==
      kFutureStatusPending) {
    return CompleteDisconnectError(
        api, internal::kDisconnectionHandlerFnSetValueAndPriority,
        kErrorConflictingOperationInProgress,
        internal::kErrorMsgConflictSetValue);
  }
  if (!internal::IsValidPriority(priority)) {
    return CompleteDisconnectError(
        api, internal::kDisconnectionHandlerFnSetValueAndPriority,
        kErrorInvalidVariantType, internal::kErrorMsgInvalidVariantForPriority);
  }
  SafeFutureHandle<void> handle = api->SafeAlloc<void>(
      internal::kDisconnectionHandlerFnSetValueAndPriority);
  internal_->SetValueAndPriorityImpl(std::move(value), std::move(priority),
                                     handle);
  return MakeFuture(api, handle);
}

Future<void> DisconnectionHandler::UpdateChildren(Variant values) {
  if (!internal_ || !internal_->future()) return Future<void>();
  ReferenceCountedFutureImpl* api = internal_->future();
  if (!values.is_map()) {
    return CompleteDisconnectError(
        api, internal::kDisconnectionHandlerFnUpdateChildren,
        kErrorInvalidVariantType,
        internal::kErrorMsgInvalidVariantForUpdateChildren);
  }
  SafeFutureHandle<void> handle =
      api->SafeAlloc<void>(internal::kDisconnectionHandlerFnUpdateChildren);
  internal_->UpdateChildrenImpl(std::move(values), handle);
  return MakeFuture(api, handle);
}

}  // namespace database
}  // namespace firebase
