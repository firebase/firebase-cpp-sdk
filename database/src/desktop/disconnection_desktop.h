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

#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/variant.h"
#include "app/src/path.h"
#include "app/src/reference_counted_future_impl.h"
#include "database/src/include/firebase/database/disconnection.h"

namespace firebase {
namespace database {
namespace internal {

class DatabaseInternal;

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
  ~DisconnectionHandlerInternal() = default;

  void CancelImpl(SafeFutureHandle<void> handle);
  void RemoveValueImpl(SafeFutureHandle<void> handle);
  void SetValueImpl(Variant value, SafeFutureHandle<void> handle);
  void SetValueAndPriorityImpl(Variant value, Variant priority,
                               SafeFutureHandle<void> handle);
  void UpdateChildrenImpl(Variant values, SafeFutureHandle<void> handle);

  DatabaseInternal* database_internal() const { return database_; }
  static DisconnectionHandler GetInvalidDisconnectionHandler() {
    return DisconnectionHandler(nullptr);
  }
  ReferenceCountedFutureImpl* future() { return future_impl_.get(); }

 private:
  DatabaseInternal* database_ = nullptr;
  Path path_;
  std::shared_ptr<ReferenceCountedFutureImpl> future_impl_;
};

}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_SRC_DESKTOP_DISCONNECTION_DESKTOP_H_
