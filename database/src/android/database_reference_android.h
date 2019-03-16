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

#ifndef FIREBASE_DATABASE_CLIENT_CPP_SRC_ANDROID_DATABASE_REFERENCE_ANDROID_H_
#define FIREBASE_DATABASE_CLIENT_CPP_SRC_ANDROID_DATABASE_REFERENCE_ANDROID_H_

#include <jni.h>
#include <map>
#include <string>
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/include/firebase/variant.h"
#include "database/src/android/query_android.h"
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

  virtual ~DatabaseReferenceInternal();

  DatabaseReferenceInternal(const DatabaseReferenceInternal& reference);

  DatabaseReferenceInternal& operator=(
      const DatabaseReferenceInternal& reference);

#if defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)
  DatabaseReferenceInternal(DatabaseReferenceInternal&& reference);

  DatabaseReferenceInternal& operator=(DatabaseReferenceInternal&& reference);
#endif  // defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)

  Database* GetDatabase() const;

  // Returns the key. Cached by this instance.
  const char* GetKey();

  // Returns the key, cached by this instance.
  const std::string& GetKeyString();

  // Return true if this is the root node, false otherwise.
  bool IsRoot() const;

  // Get the parent node. Or if we are the parent, then ourselves again.
  //
  // This should be passed to a DatabaseReference for lifetime management.
  DatabaseReferenceInternal* GetParent() const;

  // This should be passed to a DatabaseReference for lifetime management.
  DatabaseReferenceInternal* GetRoot() const;

  // This should be passed to a DatabaseReference for lifetime management.
  DatabaseReferenceInternal* Child(const char* path) const;

  // This should be passed to a DatabaseReference for lifetime management.
  DatabaseReferenceInternal* PushChild() const;

  Future<void> RemoveValue();

  Future<void> RemoveValueLastResult();

  Future<DataSnapshot> RunTransaction(
      DoTransactionWithContext transaction_function, void* context,
      void (*delete_context)(void*), bool trigger_local_events = true);

  Future<DataSnapshot> RunTransactionLastResult();

  Future<void> SetPriority(Variant priority);

  Future<void> SetPriorityLastResult();

  Future<void> SetValue(Variant value);

  Future<void> SetValueLastResult();

  Future<void> SetValueAndPriority(Variant value, Variant priority);

  Future<void> SetValueAndPriorityLastResult();

  Future<void> UpdateChildren(Variant values);

  Future<void> UpdateChildrenLastResult();

  std::string GetUrl() const;

  DisconnectionHandler* OnDisconnect();

  void GoOffline() const;

  void GoOnline() const;

 private:
  friend class DatabaseInternal;

  static bool Initialize(App* app);
  static void Terminate(App* app);

  // Get the Future for the DatabaseReferenceInternal.
  ReferenceCountedFutureImpl* ref_future();
  // The memory location of this member variable is used to look up our
  // ReferenceCountedFutureImpl. We can't use "this" because QueryInternal and
  // DatabaseReferenceInternal require two separate ReferenceCountedFutureImpl
  // instances, but have the same "this" pointer as one is a subclass of the
  // other.
  int future_api_id_;
  Variant cached_key_;  // A Variant for the memory management convenience.
  DisconnectionHandler* cached_disconnection_handler_;
};

// Used by RunTransaction to keep track of the important things.
struct TransactionData {
  TransactionData(DoTransactionWithContext transaction_,
                  ReferenceCountedFutureImpl* future_,
                  SafeFutureHandle<DataSnapshot> handle_)
      : transaction(transaction_),
        future(future_),
        handle(handle_),
        context(nullptr),
        delete_context(nullptr),
        java_handler(nullptr) {}
  ~TransactionData() {
    if (delete_context) delete_context(context);
    delete_context = nullptr;
    java_handler = nullptr;  // This is freed in DeleteJavaTransactionHandler
  }
  DoTransactionWithContext transaction;
  ReferenceCountedFutureImpl* future;
  SafeFutureHandle<DataSnapshot> handle;
  void* context;
  void (*delete_context)(void*);
  jobject java_handler;
};

}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_CLIENT_CPP_SRC_ANDROID_DATABASE_REFERENCE_ANDROID_H_
