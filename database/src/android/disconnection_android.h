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

#ifndef FIREBASE_DATABASE_SRC_ANDROID_DISCONNECTION_ANDROID_H_
#define FIREBASE_DATABASE_SRC_ANDROID_DISCONNECTION_ANDROID_H_

#include <jni.h>

#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/variant.h"
#include "app/src/reference_counted_future_impl.h"
#include "database/src/common/database_reference.h"
#include "database/src/common/query.h"
#include "database/src/include/firebase/database/disconnection.h"

namespace firebase {
namespace database {
namespace internal {

class DatabaseReferenceInternal;
class DatabaseInternal;

class DisconnectionHandlerInternal {
 public:
  ~DisconnectionHandlerInternal();

  void CancelImpl(SafeFutureHandle<void> handle);
  void RemoveValueImpl(SafeFutureHandle<void> handle);
  void SetValueImpl(const Variant& value, SafeFutureHandle<void> handle);
  void SetValueAndPriorityImpl(const Variant& value, const Variant& priority,
                               SafeFutureHandle<void> handle);
  void UpdateChildrenImpl(const Variant& values, SafeFutureHandle<void> handle);

  DatabaseInternal* database_internal() const { return db_; }
  static DisconnectionHandler GetInvalidDisconnectionHandler() {
    return DisconnectionHandler(nullptr);
  }
  ReferenceCountedFutureImpl* future() { return future_api_.get(); }

  DisconnectionHandlerInternal(const DisconnectionHandlerInternal&) = delete;
  DisconnectionHandlerInternal& operator=(const DisconnectionHandlerInternal&) =
      delete;

 private:
  friend class DatabaseInternal;
  friend class DatabaseReferenceInternal;

  static bool Initialize(App* app);
  static void Terminate(App* app);
  explicit DisconnectionHandlerInternal(DatabaseInternal* db, jobject obj);

  DatabaseInternal* db_;
  jobject obj_;
  ScopedFutureApi<DatabaseInternal, kDisconnectionHandlerFnCount> future_api_;
};

}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_SRC_ANDROID_DISCONNECTION_ANDROID_H_
