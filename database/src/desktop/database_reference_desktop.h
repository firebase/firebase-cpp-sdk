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

#ifndef FIREBASE_DATABASE_SRC_DESKTOP_DATABASE_REFERENCE_DESKTOP_H_
#define FIREBASE_DATABASE_SRC_DESKTOP_DATABASE_REFERENCE_DESKTOP_H_

#include <map>
#include <string>

#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/include/firebase/variant.h"
#include "app/src/reference_counted_future_impl.h"
#include "database/src/desktop/query_desktop.h"
#include "database/src/include/firebase/database/disconnection.h"

namespace firebase {
namespace database {
namespace internal {

class DatabaseReferenceInternal : public QueryInternal {
 public:
  DatabaseReferenceInternal(DatabaseInternal* database, const Path& path);

  ~DatabaseReferenceInternal() override;

  DatabaseReferenceInternal(const DatabaseReferenceInternal& reference);

  DatabaseReferenceInternal& operator=(
      const DatabaseReferenceInternal& reference);

  DatabaseReferenceInternal(DatabaseReferenceInternal&& reference) noexcept;

  DatabaseReferenceInternal& operator=(
      DatabaseReferenceInternal&& reference) noexcept;

  Database* GetDatabase() const;

  const char* GetKey() const { return query_spec_.path.GetBaseName(); }

  std::string GetKeyString() const { return query_spec_.path.GetBaseName(); }

  std::string GetUrl() const;

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

  
  void SetValueAndPriorityImpl(const Variant& value,
                                   const Variant& priority, SafeFutureHandle<void> handle);

  
  void UpdateChildrenImpl(const Variant& values, SafeFutureHandle<void> handle);

  
  DisconnectionHandler* OnDisconnect();

  void GoOffline();

  void GoOnline();

 private:
  // Get the Future for the DatabaseReferenceInternal.
  public:
  ReferenceCountedFutureImpl* ref_future();

  // The memory location of this member variable is used to look up our
  // ReferenceCountedFutureImpl. We can't use "this" because QueryInternal and
  // DatabaseReferenceInternal require two separate ReferenceCountedFutureImpl
  // instances, but have the same "this" pointer as one is a subclass of the
  // other.
  ReferenceCountedFutureImpl ref_future_impl_;
};

}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_SRC_DESKTOP_DATABASE_REFERENCE_DESKTOP_H_
