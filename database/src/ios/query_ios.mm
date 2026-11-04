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

#include <cstddef>
#include <memory>
#include <utility>

#include "app/src/include/firebase/variant.h"
#include "app/src/util_ios.h"
#include "database/src/common/query.h"
#include "database/src/include/firebase/database.h"
#include "database/src/include/firebase/database/query.h"
#include "database/src/ios/data_snapshot_ios.h"
#include "database/src/ios/database_ios.h"
#include "database/src/ios/database_reference_ios.h"
#include "database/src/ios/util_ios.h"

#import "FIRDatabase.h"

@implementation FIRCPPDatabaseQueryCallbackState {
  NSMutableArray* _observers;
}

- (_Nonnull instancetype)
    initWithDatabase:(firebase::database::internal::DatabaseInternal* _Nonnull)
                         databaseInternal
            andQuery:(FIRDatabaseQuery* _Nonnull)databaseQuery
    andValueListener:(firebase::database::ValueListener* _Nullable)valueListener
    andChildListener:
        (firebase::database::ChildListener* _Nullable)childListener {
  self = [super init];
  if (self) {
    // "4" is the maximum number of observers for a child listener.
    _observers = [NSMutableArray arrayWithCapacity:4];
    _databaseInternal = databaseInternal;
    _lock = databaseInternal->query_lock();
    _databaseQuery = databaseQuery;
    _valueListener = valueListener;
    _childListener = childListener;
  }
  return self;
}

- (void)addObserverHandle:(FIRDatabaseHandle)handle {
  [_lock lock];
  [_observers addObject:[NSNumber numberWithUnsignedLong:handle]];
  [_lock unlock];
}

- (void)removeAllObservers {
  [_lock lock];
  for (NSNumber* handleValue in _observers) {
    FIRDatabaseHandle handle = handleValue.unsignedLongValue;
    [_databaseQuery removeObserverWithHandle:handle];
  }
  [_observers removeAllObjects];
  _databaseInternal = nullptr;
  _databaseQuery = nil;
  _valueListener = nullptr;
  _childListener = nullptr;
  [_lock unlock];
}
@end

namespace firebase {
namespace database {
namespace internal {

using util::VariantToId;

namespace {

void LogException(Logger* logger, const char* name, const char* url,
                  NSException* exception) {
  if (!logger) return;
  logger->LogError(
      "%s",
      [[NSString stringWithFormat:@"Query::%s (URL = %s): %@", name, url,
                                  exception] UTF8String]);
}

}  // namespace

QueryInternal::QueryInternal(DatabaseInternal* database,
                             std::unique_ptr<FIRDatabaseQueryPointer> query)
    : database_(database), impl_(std::move(query)) {
  if (database_) {
    database_->future_manager().AllocFutureApi(&future_api_id_, kQueryFnCount);
  }
}

QueryInternal::QueryInternal(DatabaseInternal* database,
                             std::unique_ptr<FIRDatabaseQueryPointer> query,
                             const QuerySpec& query_spec)
    : query_spec_(query_spec), database_(database), impl_(std::move(query)) {
  if (database_) {
    database_->future_manager().AllocFutureApi(&future_api_id_, kQueryFnCount);
  }
}

QueryInternal::QueryInternal(const QueryInternal& other)
    : query_spec_(other.query_spec_),
      database_(other.database_),
      impl_(std::make_unique<FIRDatabaseQueryPointer>(other.impl())) {
  if (database_) {
    database_->future_manager().AllocFutureApi(&future_api_id_, kQueryFnCount);
  }
}

QueryInternal& QueryInternal::operator=(const QueryInternal& other) {
  if (&other == this) return *this;
  impl_ = std::make_unique<FIRDatabaseQueryPointer>(other.impl());
  query_spec_ = other.query_spec_;
  if (database_ != other.database_) {
    if (database_) {
      database_->future_manager().ReleaseFutureApi(&future_api_id_);
    }
    database_ = other.database_;
    if (database_) {
      database_->future_manager().AllocFutureApi(&future_api_id_,
                                                 kQueryFnCount);
    }
  }
  return *this;
}

QueryInternal::QueryInternal(QueryInternal&& other) noexcept
    : query_spec_(std::move(other.query_spec_)),
      database_(other.database_),
      impl_(std::move(other.impl_)) {
  other.database_ = nullptr;
  if (database_) {
    database_->future_manager().MoveFutureApi(&other.future_api_id_,
                                              &future_api_id_);
  }
}

QueryInternal& QueryInternal::operator=(QueryInternal&& other) noexcept {
  if (&other == this) return *this;
  impl_ = std::move(other.impl_);
  if (database_) {
    database_->future_manager().ReleaseFutureApi(&future_api_id_);
  }
  database_ = other.database_;
  other.database_ = nullptr;
  if (database_) {
    database_->future_manager().MoveFutureApi(&other.future_api_id_,
                                              &future_api_id_);
  }
  query_spec_ = std::move(other.query_spec_);
  return *this;
}

QueryInternal::~QueryInternal() {
  if (database_) {
    database_->future_manager().ReleaseFutureApi(&future_api_id_);
  }
}

SingleValueListener::SingleValueListener(
    DatabaseInternal* database, ReferenceCountedFutureImpl* future,
    const SafeFutureHandle<DataSnapshot>& handle,
    const Future<DataSnapshot>& backing_future,
    const FIRCPPDatabaseQueryCallbackStatePointer& callback_state)
    : db_ref_(database ? database->this_ref()
                       : DatabaseInternal::ThisRef(nullptr)),
      future_(future),
      handle_(handle),
      backing_future_(backing_future),
      callback_state_(std::make_unique<FIRCPPDatabaseQueryCallbackStatePointer>(
          callback_state)) {}

SingleValueListener::~SingleValueListener() {
  if (callback_state_ && callback_state_->get() != nil) {
    [callback_state_->get() removeAllObservers];
  }
}

void SingleValueListener::OnValueChanged(const DataSnapshot& snapshot) {
  DatabaseInternal::ThisRefLock lock(&db_ref_);
  if (lock.GetReference() != nullptr && future_ != nullptr) {
    future_->Complete<DataSnapshot>(
        handle_, kErrorNone, "",
        [&snapshot](DataSnapshot* data) { *data = snapshot; });
  }
}

void SingleValueListener::OnCancelled(const Error& error_code,
                                      const char* error_message) {
  DatabaseInternal::ThisRefLock lock(&db_ref_);
  if (lock.GetReference() != nullptr && future_ != nullptr) {
    future_->Complete(handle_, error_code, error_message ? error_message : "");
  }
}

typedef void (*NotifyValueListenerFunc)(DatabaseInternal* _Nonnull database,
                                        ValueListener* _Nonnull listener,
                                        const DataSnapshot& snapshot);

// Safely attempt to notify a value listener.
static void NotifyValueListener(
    FIRCPPDatabaseQueryCallbackState* _Nonnull callback_state,
    FIRDataSnapshot* _Nonnull snapshot_impl,
    NotifyValueListenerFunc _Nonnull notify) {
  [callback_state.lock lock];
  DatabaseInternal* database = callback_state.databaseInternal;
  ValueListener* listener = callback_state.valueListener;
  if (database && listener) {
    notify(database, listener,
           DataSnapshot(new DataSnapshotInternal(
               database,
               std::make_unique<FIRDataSnapshotPointer>(snapshot_impl))));
  }
  [callback_state.lock unlock];
}

typedef void (*CancelValueListenerFunc)(DatabaseInternal* _Nonnull database,
                                        ValueListener* _Nonnull listener,
                                        const Error& error_code,
                                        const char* _Nonnull error_string);

// Safely attempt to cancel a value listener.
static void CancelValueListener(
    FIRCPPDatabaseQueryCallbackState* _Nonnull callback_state,
    NSError* _Nonnull error, CancelValueListenerFunc _Nonnull notify) {
  Error error_code = NSErrorToErrorCode(error);
  const char* error_string = GetErrorMessage(error_code);
  [callback_state.lock lock];
  DatabaseInternal* database = callback_state.databaseInternal;
  ValueListener* listener = callback_state.valueListener;
  if (database && listener) {
    notify(database, listener, error_code, error_string);
  }
  [callback_state.lock unlock];
}

void QueryInternal::GetValueImpl(SafeFutureHandle<DataSnapshot> handle) {
  if (!database_) return Future<DataSnapshot>();
  ReferenceCountedFutureImpl* future_api = query_future();
  SafeFutureHandle<DataSnapshot> future_handle =
      future_api->SafeAlloc<DataSnapshot>(kQueryFnGetValue,
                                          DataSnapshot(nullptr));
  Future<DataSnapshot> result_future = MakeFuture(future_api, future_handle);
  if (!impl()) {
    future_api->Complete(future_handle, kErrorOperationFailed,
                         GetErrorMessage(kErrorOperationFailed));
    return result_future;
  }

  // Register a one-time ValueEventListener with the query.
  FIRCPPDatabaseQueryCallbackState* callback_state =
      [[FIRCPPDatabaseQueryCallbackState alloc] initWithDatabase:database_
                                                        andQuery:impl()
                                                andValueListener:nullptr
                                                andChildListener:nullptr];
  SingleValueListener* single_listener = new SingleValueListener(
      database_, future_api, future_handle, result_future,
      FIRCPPDatabaseQueryCallbackStatePointer(callback_state));
  callback_state.valueListener = single_listener;

  void (^block)(FIRDataSnapshot* _Nonnull) =
      ^(FIRDataSnapshot* _Nonnull snapshot_impl) {
        NotifyValueListener(callback_state, snapshot_impl,
                            [](DatabaseInternal* _Nonnull database,
                               ValueListener* _Nonnull listener,
                               const DataSnapshot& snapshot) {
                              if (database->RemoveSingleValueListener(
                                      listener)) {
                                listener->OnValueChanged(snapshot);
                                delete listener;
                              }
                            });
      };

  void (^cancel_block)(NSError* _Nonnull) = ^(NSError* _Nonnull error) {
    CancelValueListener(callback_state, error,
                        [](DatabaseInternal* _Nonnull database,
                           ValueListener* _Nonnull listener,
                           const Error& error_code,
                           const char* _Nonnull error_string) {
                          if (database->RemoveSingleValueListener(listener)) {
                            listener->OnCancelled(error_code, error_string);
                            delete listener;
                          }
                        });
  };

  [callback_state.lock lock];
  @try {
    database_->AddSingleValueListener(single_listener);
    FIRDatabaseHandle handle =
        [impl() observeEventType:FIRDataEventTypeValue
                       withBlock:block
                 withCancelBlock:cancel_block];
    [callback_state addObserverHandle:handle];
  } @finally {
    [callback_state.lock unlock];
  }

  return result_future;
}


void QueryInternal::AddValueListener(ValueListener* value_listener) {
  if (!database_ || !impl() || !value_listener) return;
  FIRCPPDatabaseQueryCallbackState* callback_state =
      [[FIRCPPDatabaseQueryCallbackState alloc] initWithDatabase:database_
                                                        andQuery:impl()
                                                andValueListener:value_listener
                                                andChildListener:nullptr];

  void (^block)(FIRDataSnapshot* _Nonnull) =
      ^(FIRDataSnapshot* _Nonnull snapshot_impl) {
        NotifyValueListener(callback_state, snapshot_impl,
                            [](DatabaseInternal* _Nonnull database,
                               ValueListener* _Nonnull listener,
                               const DataSnapshot& snapshot) {
                              (void)database;
                              listener->OnValueChanged(snapshot);
                            });
      };

  void (^cancel_block)(NSError* _Nonnull) = ^(NSError* _Nonnull error) {
    CancelValueListener(callback_state, error,
                        [](DatabaseInternal* _Nonnull database,
                           ValueListener* _Nonnull listener,
                           const Error& error_code,
                           const char* _Nonnull error_string) {
                          (void)database;
                          listener->OnCancelled(error_code, error_string);
                        });
  };

  [callback_state.lock lock];
  @try {
    if (!database_->RegisterValueListener(query_spec_, value_listener,
                                          callback_state)) {
      database_->logger()->LogWarning(
          "Query::AddValueListener (URL = %s): You may not register the same "
          "ValueListener more than once on the same Query.",
          query_spec_.path.c_str());
      return;
    }
    [callback_state
        addObserverHandle:[impl() observeEventType:FIRDataEventTypeValue
                                         withBlock:block
                                   withCancelBlock:cancel_block]];
  } @finally {
    [callback_state.lock unlock];
  }
}

void QueryInternal::RemoveValueListener(ValueListener* value_listener) {
  if (!database_) return;
  database_->UnregisterValueListener(query_spec_, value_listener, impl());
}

void QueryInternal::RemoveAllValueListeners() {
  if (!database_) return;
  database_->UnregisterAllValueListeners(query_spec_, impl());
}

typedef void (*NotifyChildListenerFunc)(ChildListener* listener,
                                        const DataSnapshot& snapshot,
                                        const char* key);

// Safely attempt to notify a child listener.
static void NotifyChildListener(
    FIRCPPDatabaseQueryCallbackState* callback_state,
    FIRDataSnapshot* _Nonnull snapshot_impl, NSString* previous_key,
    NotifyChildListenerFunc _Nonnull notify) {
  [callback_state.lock lock];
  DatabaseInternal* database = callback_state.databaseInternal;
  ChildListener* listener = callback_state.childListener;
  if (database && listener) {
    notify(listener,
           DataSnapshot(new DataSnapshotInternal(
               database,
               std::make_unique<FIRDataSnapshotPointer>(snapshot_impl))),
           [previous_key UTF8String]);
  }
  [callback_state.lock unlock];
}

void QueryInternal::AddChildListener(ChildListener* child_listener) {
  if (!database_ || !impl() || !child_listener) return;
  typedef void (^CallbackBlock)(FIRDataSnapshot* _Nonnull, NSString* _Nullable);
  typedef void (^CancelBlock)(NSError* _Nonnull);
  FIRCPPDatabaseQueryCallbackState* callback_state =
      [[FIRCPPDatabaseQueryCallbackState alloc] initWithDatabase:database_
                                                        andQuery:impl()
                                                andValueListener:nullptr
                                                andChildListener:child_listener];

  CallbackBlock child_added_block =
      ^(FIRDataSnapshot* _Nonnull snapshot_impl, NSString* previous_key) {
        NotifyChildListener(callback_state, snapshot_impl, previous_key,
                            [](ChildListener* listener,
                               const DataSnapshot& snapshot, const char* key) {
                              listener->OnChildAdded(snapshot, key);
                            });
      };

  CallbackBlock child_changed_block =
      ^(FIRDataSnapshot* _Nonnull snapshot_impl, NSString* previous_key) {
        NotifyChildListener(callback_state, snapshot_impl, previous_key,
                            [](ChildListener* listener,
                               const DataSnapshot& snapshot, const char* key) {
                              listener->OnChildChanged(snapshot, key);
                            });
      };

  CallbackBlock child_moved_block =
      ^(FIRDataSnapshot* _Nonnull snapshot_impl, NSString* previous_key) {
        NotifyChildListener(callback_state, snapshot_impl, previous_key,
                            [](ChildListener* listener,
                               const DataSnapshot& snapshot, const char* key) {
                              listener->OnChildMoved(snapshot, key);
                            });
      };

  CallbackBlock child_removed_block = ^(FIRDataSnapshot* _Nonnull snapshot_impl,
                                        NSString* previous_key) {
    NotifyChildListener(
        callback_state, snapshot_impl, previous_key,
        [](ChildListener* listener, const DataSnapshot& snapshot,
           const char* key) {
          (void)key;
          listener->OnChildRemoved(snapshot);
        });
  };

  CancelBlock cancel_block = ^(NSError* _Nonnull error) {
    Error error_code = NSErrorToErrorCode(error);
    const char* error_string = GetErrorMessage(error_code);
    [callback_state.lock lock];
    DatabaseInternal* database = callback_state.databaseInternal;
    ChildListener* listener = callback_state.childListener;
    if (database && listener) listener->OnCancelled(error_code, error_string);
    [callback_state.lock unlock];
  };

  [callback_state.lock lock];
  @try {
    if (!database_->RegisterChildListener(query_spec_, child_listener,
                                          callback_state)) {
      database_->logger()->LogWarning(
          "Query::AddChildListener (URL = %s): You may not register the same "
          "ChildListener more than once on the same Query.",
          query_spec_.path.c_str());
      return;
    }
    FIRDatabaseQuery* query = impl();
    [callback_state
        addObserverHandle:[query
                              observeEventType:FIRDataEventTypeChildAdded
                andPreviousSiblingKeyWithBlock:child_added_block
                               withCancelBlock:cancel_block]];
    [callback_state
        addObserverHandle:[query
                              observeEventType:FIRDataEventTypeChildChanged
                andPreviousSiblingKeyWithBlock:child_changed_block]];
    [callback_state
        addObserverHandle:[query
                              observeEventType:FIRDataEventTypeChildMoved
                andPreviousSiblingKeyWithBlock:child_moved_block]];
    [callback_state
        addObserverHandle:[query
                              observeEventType:FIRDataEventTypeChildRemoved
                andPreviousSiblingKeyWithBlock:child_removed_block]];
  } @finally {
    [callback_state.lock unlock];
  }
}

void QueryInternal::RemoveChildListener(ChildListener* child_listener) {
  if (!database_) return;
  database_->UnregisterChildListener(query_spec_, child_listener, impl());
}

void QueryInternal::RemoveAllChildListeners() {
  if (!database_) return;
  database_->UnregisterAllChildListeners(query_spec_, impl());
}

DatabaseReferenceInternal* QueryInternal::GetReference() {
  if (!database_ || !impl()) return nullptr;
  return new DatabaseReferenceInternal(
      database_, std::make_unique<FIRDatabaseReferencePointer>(impl().ref));
}

void QueryInternal::SetKeepSynchronized(bool keep_sync) {
  [impl() keepSynced:keep_sync ? YES : NO];
}

QueryInternal* QueryInternal::OrderByChild(const char* path) {
  if (path == nullptr || !database_ || !impl()) {
    if (database_ && database_->logger()) {
      database_->logger()->LogError(
          "Query::OrderByChild (URL = %s): Path must not be nullptr.",
          query_spec_.path.c_str());
    }
    return nullptr;
  }
  @try {
    NSString* path_str = @(path);
    if (!path_str) return nullptr;
    internal::QuerySpec spec = query_spec_;
    spec.params.order_by = internal::QueryParams::kOrderByChild;
    spec.params.order_by_child = path;
    return new QueryInternal(
        database_,
        std::make_unique<FIRDatabaseQueryPointer>(
            [impl() queryOrderedByChild:path_str]),
        spec);
  } @catch (NSException* exception) {
    LogException(database_->logger(), "OrderByChild", query_spec_.path.c_str(),
                 exception);
    return nullptr;
  }
}

QueryInternal* QueryInternal::OrderByKey() {
  if (!database_ || !impl()) return nullptr;
  @try {
    internal::QuerySpec spec = query_spec_;
    spec.params.order_by = internal::QueryParams::kOrderByKey;
    return new QueryInternal(
        database_,
        std::make_unique<FIRDatabaseQueryPointer>([impl() queryOrderedByKey]),
        spec);
  } @catch (NSException* exception) {
    LogException(database_->logger(), "OrderByKey", query_spec_.path.c_str(),
                 exception);
    return nullptr;
  }
}

QueryInternal* QueryInternal::OrderByPriority() {
  if (!database_ || !impl()) return nullptr;
  @try {
    internal::QuerySpec spec = query_spec_;
    spec.params.order_by = internal::QueryParams::kOrderByPriority;
    return new QueryInternal(
        database_,
        std::make_unique<FIRDatabaseQueryPointer>(
            [impl() queryOrderedByPriority]),
        spec);
  } @catch (NSException* exception) {
    LogException(database_->logger(), "OrderByPriority",
                 query_spec_.path.c_str(), exception);
    return nullptr;
  }
}

QueryInternal* QueryInternal::OrderByValue() {
  if (!database_ || !impl()) return nullptr;
  @try {
    internal::QuerySpec spec = query_spec_;
    spec.params.order_by = internal::QueryParams::kOrderByValue;
    return new QueryInternal(
        database_,
        std::make_unique<FIRDatabaseQueryPointer>([impl() queryOrderedByValue]),
        spec);
  } @catch (NSException* exception) {
    LogException(database_->logger(), "OrderByValue", query_spec_.path.c_str(),
                 exception);
    return nullptr;
  }
}

QueryInternal* QueryInternal::StartAt(Variant start_value) {
  if (!database_ || !impl()) return nullptr;
  @try {
    internal::QuerySpec spec = query_spec_;
    spec.params.start_at_value = start_value;
    return new QueryInternal(
        database_,
        std::make_unique<FIRDatabaseQueryPointer>(
            [impl() queryStartingAtValue:VariantToId(start_value)]),
        spec);
  } @catch (NSException* exception) {
    LogException(database_->logger(), "StartAt", query_spec_.path.c_str(),
                 exception);
    return nullptr;
  }
}

QueryInternal* QueryInternal::StartAt(Variant start_value,
                                      const char* child_key) {
  if (child_key == nullptr || !database_ || !impl()) {
    if (database_ && database_->logger()) {
      database_->logger()->LogError(
          "Query::StartAt (URL = %s): Child key must not be nullptr.",
          query_spec_.path.c_str());
    }
    return nullptr;
  }
  @try {
    NSString* child_key_str = @(child_key);
    if (!child_key_str) return nullptr;
    internal::QuerySpec spec = query_spec_;
    spec.params.start_at_value = start_value;
    spec.params.start_at_child_key = child_key;
    return new QueryInternal(
        database_,
        std::make_unique<FIRDatabaseQueryPointer>(
            [impl() queryStartingAtValue:VariantToId(start_value)
                                childKey:child_key_str]),
        spec);
  } @catch (NSException* exception) {
    LogException(database_->logger(), "StartAt", query_spec_.path.c_str(),
                 exception);
    return nullptr;
  }
}

QueryInternal* QueryInternal::EndAt(Variant end_value) {
  if (!database_ || !impl()) return nullptr;
  @try {
    internal::QuerySpec spec = query_spec_;
    spec.params.end_at_value = end_value;
    return new QueryInternal(
        database_,
        std::make_unique<FIRDatabaseQueryPointer>(
            [impl() queryEndingAtValue:VariantToId(end_value)]),
        spec);
  } @catch (NSException* exception) {
    LogException(database_->logger(), "EndAt", query_spec_.path.c_str(),
                 exception);
    return nullptr;
  }
}

QueryInternal* QueryInternal::EndAt(Variant end_value, const char* child_key) {
  if (child_key == nullptr || !database_ || !impl()) {
    if (database_ && database_->logger()) {
      database_->logger()->LogError(
          "Query::EndAt (URL = %s): Child key must not be nullptr.",
          query_spec_.path.c_str());
    }
    return nullptr;
  }
  @try {
    NSString* child_key_str = @(child_key);
    if (!child_key_str) return nullptr;
    internal::QuerySpec spec = query_spec_;
    spec.params.end_at_value = end_value;
    spec.params.end_at_child_key = child_key;
    return new QueryInternal(
        database_,
        std::make_unique<FIRDatabaseQueryPointer>(
            [impl() queryEndingAtValue:VariantToId(end_value)
                              childKey:child_key_str]),
        spec);
  } @catch (NSException* exception) {
    LogException(database_->logger(), "EndAt", query_spec_.path.c_str(),
                 exception);
    return nullptr;
  }
}

QueryInternal* QueryInternal::EqualTo(Variant value) {
  if (!database_ || !impl()) return nullptr;
  @try {
    internal::QuerySpec spec = query_spec_;
    spec.params.equal_to_value = value;
    return new QueryInternal(
        database_,
        std::make_unique<FIRDatabaseQueryPointer>(
            [impl() queryEqualToValue:VariantToId(value)]),
        spec);
  } @catch (NSException* exception) {
    LogException(database_->logger(), "EqualTo", query_spec_.path.c_str(),
                 exception);
    return nullptr;
  }
}

QueryInternal* QueryInternal::EqualTo(Variant value, const char* child_key) {
  if (child_key == nullptr || !database_ || !impl()) {
    if (database_ && database_->logger()) {
      database_->logger()->LogError(
          "Query::EqualTo (URL = %s): Child key must not be nullptr.",
          query_spec_.path.c_str());
    }
    return nullptr;
  }
  @try {
    NSString* child_key_str = @(child_key);
    if (!child_key_str) return nullptr;
    internal::QuerySpec spec = query_spec_;
    spec.params.equal_to_value = value;
    spec.params.equal_to_child_key = child_key;
    return new QueryInternal(
        database_,
        std::make_unique<FIRDatabaseQueryPointer>(
            [impl() queryEqualToValue:VariantToId(value)
                             childKey:child_key_str]),
        spec);
  } @catch (NSException* exception) {
    LogException(database_->logger(), "EqualTo", query_spec_.path.c_str(),
                 exception);
    return nullptr;
  }
}

QueryInternal* QueryInternal::LimitToFirst(size_t limit) {
  if (!database_ || !impl()) return nullptr;
  @try {
    internal::QuerySpec spec = query_spec_;
    spec.params.limit_first = limit;
    return new QueryInternal(
        database_,
        std::make_unique<FIRDatabaseQueryPointer>(
            [impl() queryLimitedToFirst:limit]),
        spec);
  } @catch (NSException* exception) {
    LogException(database_->logger(), "LimitToFirst", query_spec_.path.c_str(),
                 exception);
    return nullptr;
  }
}

QueryInternal* QueryInternal::LimitToLast(size_t limit) {
  if (!database_ || !impl()) return nullptr;
  @try {
    internal::QuerySpec spec = query_spec_;
    spec.params.limit_last = limit;
    return new QueryInternal(
        database_,
        std::make_unique<FIRDatabaseQueryPointer>(
            [impl() queryLimitedToLast:limit]),
        spec);
  } @catch (NSException* exception) {
    LogException(database_->logger(), "LimitToLast", query_spec_.path.c_str(),
                 exception);
    return nullptr;
  }
}

ReferenceCountedFutureImpl* QueryInternal::query_future() {
  return database_->future_manager().GetFutureApi(&future_api_id_);
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
