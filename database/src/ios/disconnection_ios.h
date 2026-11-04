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

#ifndef FIREBASE_DATABASE_SRC_IOS_DISCONNECTION_IOS_H_
#define FIREBASE_DATABASE_SRC_IOS_DISCONNECTION_IOS_H_

#include <memory>

#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/variant.h"
#include "app/src/reference_counted_future_impl.h"
#include "database/src/common/database_reference.h"
#include "database/src/common/query.h"
#include "database/src/include/firebase/database/disconnection.h"

#ifdef __OBJC__
#import "FIRDatabase.h"
#endif  // __OBJC__

namespace firebase {
namespace database {
namespace internal {

#pragma clang assume_nonnull begin

class FIRDatabaseReferencePointer;
class DatabaseInternal;
class DatabaseReferenceInternal;

class DisconnectionHandlerInternal {
 public:
  ~DisconnectionHandlerInternal();

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
  ReferenceCountedFutureImpl* _Nullable future() { return future_api_.get(); }

 private:
  friend class DatabaseReferenceInternal;
  explicit DisconnectionHandlerInternal(
      DatabaseInternal* database,
      std::unique_ptr<FIRDatabaseReferencePointer> impl);

#ifdef __OBJC__
  FIRDatabaseReference* impl() const;
#endif  // __OBJC__

  DatabaseInternal* database_;
  std::unique_ptr<FIRDatabaseReferencePointer> impl_;
  ScopedFutureApi<DatabaseInternal, kDisconnectionHandlerFnCount> future_api_;
};

#pragma clang assume_nonnull end

}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_SRC_IOS_DISCONNECTION_IOS_H_
