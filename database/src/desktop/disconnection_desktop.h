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

#ifndef FIREBASE_DATABASE_SRC_DESKTOP_DISCONNECTION_DESKTOP_H_
#define FIREBASE_DATABASE_SRC_DESKTOP_DISCONNECTION_DESKTOP_H_

#include <memory>

#include "app/src/reference_counted_future_impl.h"
#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/variant.h"
#include "app/src/path.h"
#include "database/src/include/firebase/database/disconnection.h"

namespace firebase {
namespace database {
namespace internal {

class DatabaseInternal;

// The desktop implementation of the Disconnection handler, which allows you to
// register server-side actions to occur when the client disconnects.
class DisconnectionHandlerInternal {
 public:
  explicit DisconnectionHandlerInternal(DatabaseInternal* database,
                                        const Path& path);

  DisconnectionHandlerInternal(const DisconnectionHandlerInternal&) = delete;
  DisconnectionHandlerInternal& operator=(const DisconnectionHandlerInternal&) =
      delete;
  DisconnectionHandlerInternal(DisconnectionHandlerInternal&&) = delete;
  DisconnectionHandlerInternal& operator=(DisconnectionHandlerInternal&&) =
      delete;

  ~DisconnectionHandlerInternal();

  // Cancel any Disconnection operations that are queued up by this handler.
  // When the Future returns, if its Error is kErrorNone, the queue has been
  // cleared on the server.
  void CancelImpl(SafeFutureHandle<void> handle);

  // Get the result of the most recent call to Cancel().
  
  // Remove the value at the current location when the client disconnects. When
  // the Future returns, if its Error is kErrorNone, the RemoveValue operation
  // has been successfully queued up on the server.
  void RemoveValueImpl(SafeFutureHandle<void> handle);

  // Get the result of the most recent call to RemoveValue().
  
  // Set the value of the data at the current location when the client
  // disconnects. When the Future returns, if its Error is kErrorNone, the
  // SetValue operation has been successfully queued up on the server.
  void SetValueImpl(Variant value, SafeFutureHandle<void> handle);

  // Get the result of the most recent call to SetValue().
  
  // Set the value and priority of the data at the current location when the
  // client disconnects. When the Future returns, if its Error is kErrorNone,
  // the SetValue operation has been successfully queued up on the server.
  void SetValueAndPriorityImpl(Variant value, Variant priority, SafeFutureHandle<void> handle);

  // Get the result of the most recent call to SetValueAndPriority().
  
  // Updates the specified child keys to the given values when the client
  // disconnects. When the Future returns, if its Error is kErrorNone, the
  // UpdateChildren operation has been successfully queued up by the server.
  void UpdateChildrenImpl(Variant values, SafeFutureHandle<void> handle);

  // Gets the result of the most recent call to either version of
  // UpdateChildren().
  
  DatabaseInternal* database_internal() const { return database_; }

  // Special method to create an invalid DisconnectionHandlerInternal, because
  // DisconnectionHandler's constructor is private.
  static DisconnectionHandler GetInvalidDisconnectionHandler();

  ReferenceCountedFutureImpl* future();

 private:
  DatabaseInternal* database_ = nullptr;

  Path path_;
  std::shared_ptr<ReferenceCountedFutureImpl> future_impl_;
};

}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_SRC_DESKTOP_DISCONNECTION_DESKTOP_H_
