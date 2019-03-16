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

#ifndef FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_DISCONNECTION_DESKTOP_H_
#define FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_DISCONNECTION_DESKTOP_H_

#include "app/src/future_manager.h"
#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/variant.h"
#include "app/src/path.h"
#include "database/src/include/firebase/database/disconnection.h"

namespace firebase {
namespace database {
namespace internal {

class DatabaseInternal;

// The iOS implementation of the Disconnection handler, which allows you to
// register server-side actions to occur when the client disconnects.
class DisconnectionHandlerInternal {
 public:
  explicit DisconnectionHandlerInternal(DatabaseInternal* database,
                                        const Path& path);

  ~DisconnectionHandlerInternal();

  // Cancel any Disconnection operations that are queued up by this handler.
  // When the Future returns, if its Error is kErrorNone, the queue has been
  // cleared on the server.
  Future<void> Cancel();

  // Get the result of the most recent call to Cancel().
  Future<void> CancelLastResult();

  // Remove the value at the current location when the client disconnects. When
  // the Future returns, if its Error is kErrorNone, the RemoveValue operation
  // has been successfully queued up on the server.
  Future<void> RemoveValue();

  // Get the result of the most recent call to RemoveValue().
  Future<void> RemoveValueLastResult();

  // Set the value of the data at the current location when the client
  // disconnects. When the Future returns, if its Error is kErrorNone, the
  // SetValue operation has been successfully queued up on the server.
  Future<void> SetValue(Variant value);

  // Get the result of the most recent call to SetValue().
  Future<void> SetValueLastResult();

  // Set the value and priority of the data at the current location when the
  // client disconnects. When the Future returns, if its Error is kErrorNone,
  // the SetValue operation has been successfully queued up on the server.
  Future<void> SetValueAndPriority(Variant value, Variant priority);

  // Get the result of the most recent call to SetValueAndPriority().
  Future<void> SetValueAndPriorityLastResult();

  // Updates the specified child keys to the given values when the client
  // disconnects. When the Future returns, if its Error is kErrorNone, the
  // UpdateChildren operation has been successfully queued up by the server.
  Future<void> UpdateChildren(Variant values);

  // Gets the result of the most recent call to either version of
  // UpdateChildren().
  Future<void> UpdateChildrenLastResult();

  DatabaseInternal* database_internal() const { return database_; }

  // Special method to create an invalid DisconnectionHandlerInternal, because
  // DisconnectionHandler's constructor is private.
  static DisconnectionHandler GetInvalidDisconnectionHandler();

 private:
  ReferenceCountedFutureImpl* future();

  DatabaseInternal* database_;

  Path path_;
};

}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_DISCONNECTION_DESKTOP_H_
