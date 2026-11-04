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

#ifndef FIREBASE_DATABASE_SRC_IOS_DATABASE_REFERENCE_IOS_H_
#define FIREBASE_DATABASE_SRC_IOS_DATABASE_REFERENCE_IOS_H_

#include <memory>
#include <string>

#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/include/firebase/variant.h"
#include "app/src/reference_counted_future_impl.h"
#include "database/src/common/database_reference.h"
#include "database/src/include/firebase/database/disconnection.h"
#include "database/src/include/firebase/database/query.h"
#include "database/src/include/firebase/database/transaction.h"
#include "database/src/ios/query_ios.h"

#ifdef __OBJC__
#import "FIRDatabase.h"
#endif  // __OBJC__

namespace firebase {
namespace database {
namespace internal {

OBJ_C_PTR_WRAPPER(FIRDatabaseReference);

class DatabaseReferenceInternal : public QueryInternal {
 public:
  explicit DatabaseReferenceInternal(
      DatabaseInternal* database,
      std::unique_ptr<FIRDatabaseReferencePointer> impl);
  ~DatabaseReferenceInternal() override;

  DatabaseReferenceInternal(const DatabaseReferenceInternal& reference);
  DatabaseReferenceInternal& operator=(
      const DatabaseReferenceInternal& reference);
  DatabaseReferenceInternal(DatabaseReferenceInternal&& reference) noexcept;
  DatabaseReferenceInternal& operator=(
      DatabaseReferenceInternal&& reference) noexcept;

  Database* GetDatabase() const;
  const char* GetKey() const { return cached_key_.c_str(); }
  std::string GetKeyString() const { return cached_key_; }
  bool IsRoot() const;
  DatabaseReferenceInternal* GetParent();
  DatabaseReferenceInternal* GetRoot();
  DatabaseReferenceInternal* Child(const char* path);
  DatabaseReferenceInternal* PushChild();

  void RemoveValueImpl(SafeFutureHandle<void> handle);
  Future<DataSnapshot> RunTransaction(
      DoTransactionWithContext transaction_function, void* context,
      void (*delete_context)(void*), bool trigger_local_events = true);
  void SetPriorityImpl(const Variant& priority, SafeFutureHandle<void> handle);
  void SetValueImpl(const Variant& value, SafeFutureHandle<void> handle);
  void SetValueAndPriorityImpl(const Variant& value, const Variant& priority,
                               SafeFutureHandle<void> handle);
  void UpdateChildrenImpl(const Variant& values, SafeFutureHandle<void> handle);

  std::string GetUrl() const;
  DisconnectionHandler* OnDisconnect();
  static void GoOffline();
  static void GoOnline();
  ReferenceCountedFutureImpl* ref_future() { return ref_future_api_.get(); }

 protected:
#ifdef __OBJC__
  FIRDatabaseReference* impl() const { return impl_ ? impl_->get() : nil; }
#endif  // __OBJC__

 public:
  std::unique_ptr<FIRDatabaseReferencePointer> impl_;
  DisconnectionHandler* cached_disconnection_handler_ = nullptr;
  std::string cached_key_;

 private:
  ScopedFutureApi<DatabaseInternal, kDatabaseReferenceFnCount> ref_future_api_;
};

}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_SRC_IOS_DATABASE_REFERENCE_IOS_H_
