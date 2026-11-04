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

#ifndef FIREBASE_DATABASE_SRC_ANDROID_DATABASE_REFERENCE_ANDROID_H_
#define FIREBASE_DATABASE_SRC_ANDROID_DATABASE_REFERENCE_ANDROID_H_

#include <jni.h>

#include <map>
#include <string>

#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/include/firebase/variant.h"
#include "database/src/android/query_android.h"
#include "database/src/common/database_reference.h"
#include "database/src/include/firebase/database/disconnection.h"
#include "database/src/include/firebase/database/query.h"
#include "database/src/include/firebase/database/transaction.h"

namespace firebase {
namespace database {
namespace internal {

class DatabaseInternal;

class DatabaseReferenceInternal : public QueryInternal {
 public:
  DatabaseReferenceInternal(DatabaseInternal* database,
                            jobject database_reference_obj);
  ~DatabaseReferenceInternal() override;

  DatabaseReferenceInternal(const DatabaseReferenceInternal& reference);
  DatabaseReferenceInternal& operator=(
      const DatabaseReferenceInternal& reference);
  DatabaseReferenceInternal(DatabaseReferenceInternal&& reference) noexcept;
  DatabaseReferenceInternal& operator=(
      DatabaseReferenceInternal&& reference) noexcept;

  Database* GetDatabase() const;
  const char* GetKey();
  const std::string& GetKeyString();
  bool IsRoot() const;
  DatabaseReferenceInternal* GetParent() const;
  DatabaseReferenceInternal* GetRoot() const;
  DatabaseReferenceInternal* Child(const char* path) const;
  DatabaseReferenceInternal* PushChild() const;

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
  void GoOffline() const;
  void GoOnline() const;
  ReferenceCountedFutureImpl* ref_future() { return ref_future_api_.get(); }

 private:
  friend class DatabaseInternal;
  static bool Initialize(App* app);
  static void Terminate(App* app);

  ScopedFutureApi<DatabaseInternal, kDatabaseReferenceFnCount> ref_future_api_;
  mutable Mutex member_mutex_;
  Variant cached_key_;
  DisconnectionHandler* cached_disconnection_handler_ = nullptr;
};

struct TransactionData {
  TransactionData(DoTransactionWithContext transaction_,
                  ReferenceCountedFutureImpl* future_,
                  SafeFutureHandle<DataSnapshot> handle_)
      : transaction(transaction_),
        future(future_),
        handle(handle_),
        backing_future(MakeFuture(future_, handle_)),
        context(nullptr),
        delete_context(nullptr),
        java_handler(nullptr) {}
  ~TransactionData() {
    if (delete_context) delete_context(context);
    delete_context = nullptr;
    java_handler = nullptr;
  }
  DoTransactionWithContext transaction;
  ReferenceCountedFutureImpl* future;
  SafeFutureHandle<DataSnapshot> handle;
  Future<DataSnapshot> backing_future;
  void* context;
  void (*delete_context)(void*);
  jobject java_handler;
};

}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_SRC_ANDROID_DATABASE_REFERENCE_ANDROID_H_
