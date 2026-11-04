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

#ifndef FIREBASE_DATABASE_SRC_IOS_QUERY_IOS_H_
#define FIREBASE_DATABASE_SRC_IOS_QUERY_IOS_H_

#include <cstddef>
#include <memory>

#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/reference_counted_future_impl.h"
#include "app/src/safe_reference.h"
#include "app/src/util_ios.h"
#include "database/src/common/query.h"
#include "database/src/common/query_spec.h"
#include "database/src/include/firebase/database.h"
#include "database/src/include/firebase/database/listener.h"

#ifdef __OBJC__
#include <Foundation/Foundation.h>

#import "FIRDatabase.h"

@interface FIRCPPDatabaseQueryCallbackState : NSObject
@property(nonatomic, nullable) firebase::database::internal::DatabaseInternal*
    databaseInternal;
@property(nonatomic, nullable) FIRDatabaseQuery* databaseQuery;
@property(nonatomic, nullable) firebase::database::ValueListener* valueListener;
@property(nonatomic, nullable) firebase::database::ChildListener* childListener;
@property(nonatomic, readonly) NSRecursiveLock* _Nonnull lock;
- (_Nullable instancetype)init NS_UNAVAILABLE;
- (_Nonnull instancetype)
    initWithDatabase:(firebase::database::internal::DatabaseInternal* _Nonnull)
                         databaseInternal
            andQuery:(FIRDatabaseQuery* _Nonnull)databaseQuery
    andValueListener:(firebase::database::ValueListener* _Nullable)valueListener
    andChildListener:(firebase::database::ChildListener* _Nullable)childListener
    NS_DESIGNATED_INITIALIZER;
- (void)addObserverHandle:(FIRDatabaseHandle)handle;
- (void)removeAllObservers;
@end
#endif  // __OBJC__

namespace firebase {
class ReferenceCountedFutureImpl;
namespace database {
namespace internal {

OBJ_C_PTR_WRAPPER(FIRDatabaseQuery);
OBJ_C_PTR_WRAPPER(FIRCPPDatabaseQueryCallbackState);

class QueryInternal {
 public:
  QueryInternal(DatabaseInternal* database,
                std::unique_ptr<FIRDatabaseQueryPointer> query);
  QueryInternal(DatabaseInternal* database,
                std::unique_ptr<FIRDatabaseQueryPointer> query,
                const internal::QuerySpec& query_spec);
  QueryInternal(const QueryInternal& query);
  QueryInternal& operator=(const QueryInternal& query);
  QueryInternal(QueryInternal&& query) noexcept;
  QueryInternal& operator=(QueryInternal&& query) noexcept;
  virtual ~QueryInternal();

  void GetValueImpl(SafeFutureHandle<DataSnapshot> handle);
  void AddValueListener(ValueListener* listener);
  void RemoveValueListener(ValueListener* listener);
  void RemoveAllValueListeners();
  void AddChildListener(ChildListener* listener);
  void RemoveChildListener(ChildListener* listener);
  void RemoveAllChildListeners();

  DatabaseReferenceInternal* GetReference();
  void SetKeepSynchronized(bool keep_sync);

  QueryInternal* OrderByChild(const char* path);
  QueryInternal* OrderByKey();
  QueryInternal* OrderByPriority();
  QueryInternal* OrderByValue();

  QueryInternal* StartAt(Variant order_value);
  QueryInternal* StartAt(Variant order_value, const char* child_key);
  QueryInternal* EndAt(Variant order_value);
  QueryInternal* EndAt(Variant order_value, const char* child_key);
  QueryInternal* EqualTo(Variant order_value);
  QueryInternal* EqualTo(Variant order_value, const char* child_key);

  QueryInternal* LimitToFirst(size_t limit);
  QueryInternal* LimitToLast(size_t limit);

  const internal::QuerySpec& query_spec() const { return query_spec_; }
  DatabaseInternal* database_internal() const { return database_; }
  ReferenceCountedFutureImpl* query_future() { return future_api_.get(); }

 protected:
#ifdef __OBJC__
  FIRDatabaseQuery* impl() const { return impl_ ? impl_->get() : nil; }
#endif  // __OBJC__

  internal::QuerySpec query_spec_;
  DatabaseInternal* database_;

 public:
  std::unique_ptr<FIRDatabaseQueryPointer> impl_;

 private:
  ScopedFutureApi<DatabaseInternal, kQueryFnCount> future_api_;
};

class SingleValueListener : public ValueListener {
 public:
  SingleValueListener(
      DatabaseInternal* database, ReferenceCountedFutureImpl* future,
      const SafeFutureHandle<DataSnapshot>& handle,
      const Future<DataSnapshot>& backing_future,
      const FIRCPPDatabaseQueryCallbackStatePointer& callback_state);
  ~SingleValueListener() override;
  void OnValueChanged(const DataSnapshot& snapshot) override;
  void OnCancelled(const Error& error_code, const char* error_message) override;

 private:
  firebase::internal::SafeReference<DatabaseInternal> db_ref_;
  ReferenceCountedFutureImpl* future_;
  SafeFutureHandle<DataSnapshot> handle_;
  Future<DataSnapshot> backing_future_;
  std::unique_ptr<FIRCPPDatabaseQueryCallbackStatePointer> callback_state_;
};

}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_SRC_IOS_QUERY_IOS_H_
