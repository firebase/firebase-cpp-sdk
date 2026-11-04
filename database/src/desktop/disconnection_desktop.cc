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

#include "database/src/common/database_reference.h"
#include "database/src/desktop/core/repo.h"
#include "database/src/desktop/database_desktop.h"
#include "database/src/desktop/database_reference_desktop.h"
#include "database/src/desktop/util_desktop.h"
#include "database/src/include/firebase/database/disconnection.h"

namespace firebase {
namespace database {
namespace internal {

DisconnectionHandlerInternal::DisconnectionHandlerInternal(
    DatabaseInternal* database, const Path& path)
    : database_(database), path_(path), future_impl_(kDisconnectionHandlerFnCount) {
}

DisconnectionHandlerInternal::~DisconnectionHandlerInternal() {
}

void DisconnectionHandlerInternal::CancelImpl(SafeFutureHandle<void> handle) {
  if (!database_) return;
  database_->repo()->OnDisconnectCancel(handle, future(), path_);
}

void DisconnectionHandlerInternal::RemoveValueImpl(SafeFutureHandle<void> handle) {
  if (!database_) return;
  database_->repo()->OnDisconnectSetValue(handle, future(), path_, Variant::Null());
}

void DisconnectionHandlerInternal::SetValueImpl(Variant value, SafeFutureHandle<void> handle) {
  if (!database_) return;
  database_->repo()->OnDisconnectSetValue(handle, future(), path_, std::move(value));
}

void DisconnectionHandlerInternal::SetValueAndPriorityImpl(
    Variant value, Variant priority, SafeFutureHandle<void> handle) {
  if (!database_) return;
  Variant data;
  if (value.is_container_type()) {
    data = std::move(value);
    data.map()[".priority"] = std::move(priority);
  } else {
    data = Variant::EmptyMap();
    data.map()[".value"] = std::move(value);
    data.map()[".priority"] = std::move(priority);
  }
  database_->repo()->OnDisconnectSetValue(handle, future(), path_, std::move(data));
}

void DisconnectionHandlerInternal::UpdateChildrenImpl(Variant values, SafeFutureHandle<void> handle) {
  if (!database_) return;
  database_->repo()->OnDisconnectUpdate(handle, future(), path_, std::move(values));
}

ReferenceCountedFutureImpl* DisconnectionHandlerInternal::future() {
  return &future_impl_;
}

DisconnectionHandler
DisconnectionHandlerInternal::GetInvalidDisconnectionHandler() {
  return DisconnectionHandler(nullptr);
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
