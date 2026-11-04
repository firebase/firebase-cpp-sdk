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

#include <utility>

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

DisconnectionHandlerInternal::DisconnectionHandlerInternal(
    DatabaseInternal* database,
    std::unique_ptr<FIRDatabaseReferencePointer> impl)
    : database_(database), impl_(std::move(impl)), future_api_(database) {}

DisconnectionHandlerInternal::~DisconnectionHandlerInternal() = default;

void DisconnectionHandlerInternal::CancelImpl(SafeFutureHandle<void> handle) {
  [impl() cancelDisconnectOperationsWithCompletionBlock:CreateCompletionBlock(
                                                            handle, future())];
}

void DisconnectionHandlerInternal::RemoveValueImpl(
    SafeFutureHandle<void> handle) {
  [impl() onDisconnectRemoveValueWithCompletionBlock:CreateCompletionBlock(
                                                         handle, future())];
}

void DisconnectionHandlerInternal::SetValueImpl(Variant value,
                                                SafeFutureHandle<void> handle) {
  [impl() onDisconnectSetValue:VariantToId(value)
           withCompletionBlock:CreateCompletionBlock(handle, future())];
}

void DisconnectionHandlerInternal::SetValueAndPriorityImpl(
    Variant value, Variant priority, SafeFutureHandle<void> handle) {
  [impl() onDisconnectSetValue:VariantToId(value)
                   andPriority:VariantToId(priority)
           withCompletionBlock:CreateCompletionBlock(handle, future())];
}

void DisconnectionHandlerInternal::UpdateChildrenImpl(
    Variant values, SafeFutureHandle<void> handle) {
  [impl() onDisconnectUpdateChildValues:(NSDictionary*)VariantToId(values)
                    withCompletionBlock:CreateCompletionBlock(handle,
                                                              future())];
}

FIRDatabaseReference* _Nonnull DisconnectionHandlerInternal::impl() const {
  return impl_->get();
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
