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

#include "database/src/ios/query_ios.h"
#include "database/src/common/query.h"

#include "app/src/include/firebase/variant.h"
#include "app/src/util_ios.h"
#include "database/src/include/firebase/database.h"
#include "database/src/include/firebase/database/query.h"
#include "database/src/ios/data_snapshot_ios.h"
#include "database/src/ios/database_ios.h"
#include "database/src/ios/database_reference_ios.h"
#include "database/src/ios/util_ios.h"

#import "FIRDatabase.h"

namespace firebase {
namespace database {
namespace internal {

using util::VariantToId;

namespace {
void LogException(const char* name, const char* url, NSException* exception) {
  LogError(
    [[NSString stringWithFormat:@"Query::%s (URL = %s): %@",
        name, url, exception] UTF8String]);
}
};

QueryInternal::QueryInternal(DatabaseInternal* database, UniquePtr<FIRDatabaseQueryPointer> query)
    : database_(database), impl_(std::move(query)) {
  database_->future_manager().AllocFutureApi(&future_api_id_, kQueryFnCount);
}

QueryInternal::QueryInternal(DatabaseInternal* database, UniquePtr<FIRDatabaseQueryPointer> query,
                             const QuerySpec& query_spec)
    : query_spec_(query_spec), database_(database), impl_(std::move(query)) {
  database_->future_manager().AllocFutureApi(&future_api_id_, kQueryFnCount);
}

QueryInternal::QueryInternal(const QueryInternal& other)
    : query_spec_(other.query_spec_), database_(other.database_) {
  impl_.reset(new FIRDatabaseQueryPointer(other.impl_->ptr));
  database_->future_manager().AllocFutureApi(&future_api_id_, kQueryFnCount);
}

QueryInternal& QueryInternal::operator=(const QueryInternal& other) {
  impl_.reset(new FIRDatabaseQueryPointer(other.impl_->ptr));
  query_spec_ = other.query_spec_;
  database_->future_manager().AllocFutureApi(&future_api_id_, kQueryFnCount);
  return *this;
}

#if defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)
QueryInternal::QueryInternal(QueryInternal&& other) : database_(other.database_) {
  impl_ = std::move(other.impl_);
  other.impl_ = MakeUnique<FIRDatabaseQueryPointer>(nil);
  database_->future_manager().MoveFutureApi(&other.future_api_id_, &future_api_id_);
  query_spec_ = other.query_spec_;
}

QueryInternal& QueryInternal::operator=(QueryInternal&& other) {
  impl_ = std::move(other.impl_);
  other.impl_ = MakeUnique<FIRDatabaseQueryPointer>(nil);
  database_->future_manager().MoveFutureApi(&other.future_api_id_, &future_api_id_);
  query_spec_ = other.query_spec_;
  return *this;
}
#endif  // defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)

QueryInternal::~QueryInternal() {
  database_->future_manager().ReleaseFutureApi(&future_api_id_);
}

SingleValueListener::SingleValueListener(DatabaseInternal* database,
                                         ReferenceCountedFutureImpl* future,
                                         const SafeFutureHandle<DataSnapshot>& handle)
    : database_(database), future_(future), handle_(handle) {}

SingleValueListener::~SingleValueListener() {}

void SingleValueListener::OnValueChanged(const DataSnapshot& snapshot) {
  database_->RemoveSingleValueListener(this);
  future_->Complete<DataSnapshot>(
      handle_, kErrorNone, "", [&snapshot](DataSnapshot* data) { *data = snapshot; });
  delete this;
}

void SingleValueListener::OnCancelled(const Error& error_code, const char* error_message) {
  database_->RemoveSingleValueListener(this);
  future_->Complete(handle_, error_code, error_message);
  delete this;
}

Future<DataSnapshot> QueryInternal::GetValue() {
  // Register a one-time ValueEventListener with the query.
  SafeFutureHandle<DataSnapshot> future_handle =
      query_future()->SafeAlloc<DataSnapshot>(kQueryFnGetValue, DataSnapshot(nullptr));
  SingleValueListener* single_listener =
      new SingleValueListener(database_, query_future(), future_handle);

  // If the database goes away, we need to be able to reach into these blocks and clear their
  // single_listener pointer. We can't do that directly, but we can cache a pointer to the pointer,
  // and clear that instead.
  SingleValueListener** single_listener_holder = database_->AddSingleValueListener(single_listener);

  DatabaseInternal* database = database_;  // in case `this` goes away.
  void (^block)(FIRDataSnapshot *_Nonnull) = ^(FIRDataSnapshot *_Nonnull snapshot_impl) {
    if (*single_listener_holder) {
      DataSnapshot snapshot(
          new DataSnapshotInternal(database, MakeUnique<FIRDataSnapshotPointer>(snapshot_impl)));
      (*single_listener_holder)->OnValueChanged(snapshot);
    }
  };

  void (^cancel_block)(NSError *_Nonnull) = ^(NSError *_Nonnull error) {
    if (*single_listener_holder) {
      Error error_code = NSErrorToErrorCode(error);
      const char* error_string = GetErrorMessage(error_code);
      (*single_listener_holder)->OnCancelled(error_code, error_string);
    }
  };

  [impl() observeSingleEventOfType:FIRDataEventTypeValue
                         withBlock:block
                   withCancelBlock:cancel_block];

  return MakeFuture(query_future(), future_handle);
}

Future<DataSnapshot> QueryInternal::GetValueLastResult() {
  return static_cast<const Future<DataSnapshot>&>(query_future()->LastResult(kQueryFnGetValue));
}

void QueryInternal::AddValueListener(ValueListener* value_listener) {
  DatabaseInternal* database = database_;  // in case `this` goes away.
  void (^block)(FIRDataSnapshot* _Nonnull) = ^(FIRDataSnapshot* _Nonnull snapshot_impl) {
    DataSnapshot snapshot(
        new DataSnapshotInternal(database, MakeUnique<FIRDataSnapshotPointer>(snapshot_impl)));
    value_listener->OnValueChanged(snapshot);
  };

  void (^cancel_block)(NSError *_Nonnull) = ^(NSError *_Nonnull error) {
    Error error_code = NSErrorToErrorCode(error);
    const char* error_string = GetErrorMessage(error_code);
    value_listener->OnCancelled(error_code, error_string);
  };

  FIRDatabaseHandle handle = [impl() observeEventType:FIRDataEventTypeValue
                                            withBlock:block
                                      withCancelBlock:cancel_block];

  ValueListenerCleanupData cleanup_data;
  cleanup_data.observer_handle = handle;
  if (!database_->RegisterValueListener(query_spec_, value_listener, cleanup_data)) {
    LogWarning("Query::AddValueListener (URL = %s): You may not register the same ValueListener"
               "more than once on the same Query.",
               query_spec_.path.c_str());
  }
}

void QueryInternal::RemoveValueListener(ValueListener* value_listener) {
  database_->UnregisterValueListener(query_spec_, value_listener, impl());
}

void QueryInternal::RemoveAllValueListeners() {
  database_->UnregisterAllValueListeners(query_spec_, impl());
}

void QueryInternal::AddChildListener(ChildListener* child_listener) {
  typedef void (^CallbackBlock)(FIRDataSnapshot *_Nonnull, NSString *_Nullable);
  typedef void (^CancelBlock)(NSError *_Nonnull);
  DatabaseInternal* database = database_;  // in case `this` goes away.
  CallbackBlock child_added_block =
      ^(FIRDataSnapshot* _Nonnull snapshot_impl, NSString* previous_key) {
        DataSnapshot snapshot(
            new DataSnapshotInternal(database, MakeUnique<FIRDataSnapshotPointer>(snapshot_impl)));
        child_listener->OnChildAdded(snapshot, [previous_key UTF8String]);
      };

  CallbackBlock child_changed_block =
      ^(FIRDataSnapshot* _Nonnull snapshot_impl, NSString* previous_key) {
        DataSnapshot snapshot(
            new DataSnapshotInternal(database, MakeUnique<FIRDataSnapshotPointer>(snapshot_impl)));
        child_listener->OnChildChanged(snapshot, [previous_key UTF8String]);
      };

  CallbackBlock child_moved_block =
      ^(FIRDataSnapshot* _Nonnull snapshot_impl, NSString* previous_key) {
        DataSnapshot snapshot(
            new DataSnapshotInternal(database, MakeUnique<FIRDataSnapshotPointer>(snapshot_impl)));
        child_listener->OnChildMoved(snapshot, [previous_key UTF8String]);
      };

  CallbackBlock child_removed_block = ^(FIRDataSnapshot *_Nonnull snapshot_impl,
                                        NSString *previous_key) {
    (void)previous_key;
    DataSnapshot snapshot(
        new DataSnapshotInternal(database, MakeUnique<FIRDataSnapshotPointer>(snapshot_impl)));
    child_listener->OnChildRemoved(snapshot);
  };

  CancelBlock cancel_block = ^(NSError *_Nonnull error) {
    Error error_code = NSErrorToErrorCode(error);
    const char* error_string = GetErrorMessage(error_code);
    child_listener->OnCancelled(error_code, error_string);
  };

  FIRDatabaseHandle child_added_handle = [impl() observeEventType:FIRDataEventTypeChildAdded
                                   andPreviousSiblingKeyWithBlock:child_added_block
                                                  withCancelBlock:cancel_block];
  FIRDatabaseHandle child_changed_handle = [impl() observeEventType:FIRDataEventTypeChildChanged
                                     andPreviousSiblingKeyWithBlock:child_changed_block];
  FIRDatabaseHandle child_moved_handle = [impl() observeEventType:FIRDataEventTypeChildMoved
                                   andPreviousSiblingKeyWithBlock:child_moved_block];
  FIRDatabaseHandle child_removed_handle = [impl() observeEventType:FIRDataEventTypeChildRemoved
                                     andPreviousSiblingKeyWithBlock:child_removed_block];

  ChildListenerCleanupData cleanup_data;
  cleanup_data.child_added_handle = child_added_handle;
  cleanup_data.child_changed_handle = child_changed_handle;
  cleanup_data.child_moved_handle = child_moved_handle;
  cleanup_data.child_removed_handle = child_removed_handle;
  if (!database_->RegisterChildListener(query_spec_, child_listener, cleanup_data)) {
    LogWarning("Query::AddChildListener (URL = %s): You may not register the same ChildListener"
               "more than once on the same Query.",
               query_spec_.path.c_str());
  }
}

void QueryInternal::RemoveChildListener(ChildListener* child_listener) {
  database_->UnregisterChildListener(query_spec_, child_listener, impl());
}

void QueryInternal::RemoveAllChildListeners() {
  database_->UnregisterAllChildListeners(query_spec_, impl());
}

DatabaseReferenceInternal* QueryInternal::GetReference() {
  return new DatabaseReferenceInternal(database_,
                                       MakeUnique<FIRDatabaseReferencePointer>(impl().ref));
}

void QueryInternal::SetKeepSynchronized(bool keep_sync) {
  [impl() keepSynced:keep_sync ? YES : NO];
}

QueryInternal* QueryInternal::OrderByChild(const char* path) {
  @try {
    internal::QuerySpec spec = query_spec_;
    spec.params.order_by = internal::QueryParams::kOrderByChild;
    spec.params.order_by_child = path;
    return new QueryInternal(
        database_, MakeUnique<FIRDatabaseQueryPointer>([impl() queryOrderedByChild:@(path)]), spec);
  }
  @catch (NSException* exception) {
    LogException("OrderByChild", query_spec_.path.c_str(), exception);
    return nullptr;
  }
}

QueryInternal* QueryInternal::OrderByKey() {
  @try {
    internal::QuerySpec spec = query_spec_;
    spec.params.order_by = internal::QueryParams::kOrderByKey;
    return new QueryInternal(database_,
                             MakeUnique<FIRDatabaseQueryPointer>([impl() queryOrderedByKey]), spec);
  }
  @catch (NSException* exception) {
    LogException("OrderByKey", query_spec_.path.c_str(), exception);
    return nullptr;
  }
}

QueryInternal* QueryInternal::OrderByPriority() {
  @try {
    internal::QuerySpec spec = query_spec_;
    spec.params.order_by = internal::QueryParams::kOrderByPriority;
    return new QueryInternal(
        database_, MakeUnique<FIRDatabaseQueryPointer>([impl() queryOrderedByPriority]), spec);
  }
  @catch (NSException* exception) {
    LogException("OrderByPriority", query_spec_.path.c_str(), exception);
    return nullptr;
  }
}

QueryInternal* QueryInternal::OrderByValue() {
  @try {
    internal::QuerySpec spec = query_spec_;
    spec.params.order_by = internal::QueryParams::kOrderByValue;
    return new QueryInternal(
        database_, MakeUnique<FIRDatabaseQueryPointer>([impl() queryOrderedByValue]), spec);
  }
  @catch (NSException* exception) {
    LogException("OrderByValue", query_spec_.path.c_str(), exception);
    return nullptr;
  }
}

QueryInternal* QueryInternal::StartAt(Variant start_value) {
  @try {
    internal::QuerySpec spec = query_spec_;
    spec.params.start_at_value = start_value;
    return new QueryInternal(
        database_,
        MakeUnique<FIRDatabaseQueryPointer>([impl() queryStartingAtValue:VariantToId(start_value)]),
        spec);
  }
  @catch (NSException* exception) {
    LogException("StartAt", query_spec_.path.c_str(), exception);
    return nullptr;
  }
}

QueryInternal* QueryInternal::StartAt(Variant start_value, const char* child_key) {
  @try {
    internal::QuerySpec spec = query_spec_;
    spec.params.start_at_value = start_value;
    spec.params.start_at_child_key = child_key;
    return new QueryInternal(
        database_,
        MakeUnique<FIRDatabaseQueryPointer>([impl() queryStartingAtValue:VariantToId(start_value)
                                                                childKey:@(child_key)]),
        spec);
  }
  @catch (NSException* exception) {
    LogException("StartAt", query_spec_.path.c_str(), exception);
    return nullptr;
  }
}

QueryInternal* QueryInternal::EndAt(Variant end_value) {
  @try {
    internal::QuerySpec spec = query_spec_;
    spec.params.end_at_value = end_value;
    return new QueryInternal(
        database_,
        MakeUnique<FIRDatabaseQueryPointer>([impl() queryEndingAtValue:VariantToId(end_value)]),
        spec);
  }
  @catch (NSException* exception) {
    LogException("EndAt", query_spec_.path.c_str(), exception);
    return nullptr;
  }
}

QueryInternal* QueryInternal::EndAt(Variant end_value, const char* child_key) {
  @try {
    internal::QuerySpec spec = query_spec_;
    spec.params.end_at_value = end_value;
    spec.params.end_at_child_key = child_key;
    return new QueryInternal(
        database_,
        MakeUnique<FIRDatabaseQueryPointer>([impl() queryEndingAtValue:VariantToId(end_value)
                                                              childKey:@(child_key)]),
        spec);
  }
  @catch (NSException* exception) {
    LogException("EndAt", query_spec_.path.c_str(), exception);
    return nullptr;
  }
}

QueryInternal* QueryInternal::EqualTo(Variant value) {
  @try {
    internal::QuerySpec spec = query_spec_;
    spec.params.equal_to_value = value;
    return new QueryInternal(
        database_,
        MakeUnique<FIRDatabaseQueryPointer>([impl() queryEqualToValue:VariantToId(value)]), spec);
  }
  @catch (NSException* exception) {
    LogException("EqualTo", query_spec_.path.c_str(), exception);
    return nullptr;
  }
}

QueryInternal* QueryInternal::EqualTo(Variant value, const char* child_key) {
  @try {
    internal::QuerySpec spec = query_spec_;
    spec.params.equal_to_value = value;
    spec.params.equal_to_child_key = child_key;
    return new QueryInternal(
        database_,
        MakeUnique<FIRDatabaseQueryPointer>([impl() queryEqualToValue:VariantToId(value)]), spec);
  }
  @catch (NSException* exception) {
    LogException("EqualTo", query_spec_.path.c_str(), exception);
    return nullptr;
  }
}

QueryInternal* QueryInternal::LimitToFirst(size_t limit) {
  @try {
    internal::QuerySpec spec = query_spec_;
    spec.params.limit_first = limit;
    return new QueryInternal(
        database_, MakeUnique<FIRDatabaseQueryPointer>([impl() queryLimitedToFirst:limit]), spec);
  }
  @catch (NSException* exception) {
    LogException("LimitToFirst", query_spec_.path.c_str(), exception);
    return nullptr;
  }
}

QueryInternal* QueryInternal::LimitToLast(size_t limit) {
  @try {
    internal::QuerySpec spec = query_spec_;
    spec.params.limit_last = limit;
    return new QueryInternal(
        database_, MakeUnique<FIRDatabaseQueryPointer>([impl() queryLimitedToLast:limit]), spec);
  }
  @catch (NSException* exception) {
    LogException("LimitToLast", query_spec_.path.c_str(), exception);
    return nullptr;
  }
}

ReferenceCountedFutureImpl* QueryInternal::query_future() {
  return database_->future_manager().GetFutureApi(&future_api_id_);
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
