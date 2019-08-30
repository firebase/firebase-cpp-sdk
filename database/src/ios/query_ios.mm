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

@implementation FIRCPPDatabaseQueryCallbackState
NSMutableArray* _observers;

- (_Nonnull instancetype)
    initWithDatabase:(firebase::database::internal::DatabaseInternal* _Nonnull)databaseInternal
            andQuery:(FIRDatabaseQuery* _Nonnull)databaseQuery
    andValueListener:(firebase::database::ValueListener* _Nullable)valueListener
    andChildListener:(firebase::database::ChildListener* _Nullable)childListener {
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
void LogException(Logger* logger, const char* name, const char* url, NSException* exception) {
  logger->LogError(
      [[NSString stringWithFormat:@"Query::%s (URL = %s): %@", name, url, exception] UTF8String]);
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
  impl_.reset(new FIRDatabaseQueryPointer(*other.impl_));
  database_->future_manager().AllocFutureApi(&future_api_id_, kQueryFnCount);
}

QueryInternal& QueryInternal::operator=(const QueryInternal& other) {
  impl_.reset(new FIRDatabaseQueryPointer(*other.impl_));
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

QueryInternal::~QueryInternal() { database_->future_manager().ReleaseFutureApi(&future_api_id_); }

SingleValueListener::SingleValueListener(
    ReferenceCountedFutureImpl* future, const SafeFutureHandle<DataSnapshot>& handle,
    const FIRCPPDatabaseQueryCallbackStatePointer& callback_state)
    : future_(future),
      handle_(handle),
      callback_state_(MakeUnique<FIRCPPDatabaseQueryCallbackStatePointer>(callback_state)) {}

SingleValueListener::~SingleValueListener() { [callback_state_->get() removeAllObservers]; }

void SingleValueListener::OnValueChanged(const DataSnapshot& snapshot) {
  future_->Complete<DataSnapshot>(handle_, kErrorNone, "",
                                  [&snapshot](DataSnapshot* data) { *data = snapshot; });
}

void SingleValueListener::OnCancelled(const Error& error_code, const char* error_message) {
  future_->Complete(handle_, error_code, error_message);
}

typedef void (*NotifyValueListenerFunc)(DatabaseInternal* _Nonnull database,
                                        ValueListener* _Nonnull listener,
                                        const DataSnapshot& snapshot);

// Safely attempt to notify a value listener.
static void NotifyValueListener(FIRCPPDatabaseQueryCallbackState* _Nonnull callback_state,
                                FIRDataSnapshot* _Nonnull snapshot_impl,
                                NotifyValueListenerFunc _Nonnull notify) {
  [callback_state.lock lock];
  DatabaseInternal* database = callback_state.databaseInternal;
  ValueListener* listener = callback_state.valueListener;
  if (database && listener) {
    notify(database, listener,
           DataSnapshot(new DataSnapshotInternal(
               database, MakeUnique<FIRDataSnapshotPointer>(snapshot_impl))));
  }
  [callback_state.lock unlock];
}

typedef void (*CancelValueListenerFunc)(DatabaseInternal* _Nonnull database,
                                        ValueListener* _Nonnull listener, const Error& error_code,
                                        const char* _Nonnull error_string);

// Safely attempt to cancel a value listener.
static void CancelValueListener(FIRCPPDatabaseQueryCallbackState* _Nonnull callback_state,
                                NSError* _Nonnull error, CancelValueListenerFunc _Nonnull notify) {
  Error error_code = NSErrorToErrorCode(error);
  const char* error_string = GetErrorMessage(error_code);
  [callback_state.lock lock];
  DatabaseInternal* database = callback_state.databaseInternal;
  ValueListener* listener = callback_state.valueListener;
  if (database && listener) notify(database, listener, error_code, error_string);
  [callback_state.lock unlock];
}

Future<DataSnapshot> QueryInternal::GetValue() {
  // Register a one-time ValueEventListener with the query.
  FIRCPPDatabaseQueryCallbackState* callback_state =
      [[FIRCPPDatabaseQueryCallbackState alloc] initWithDatabase:database_
                                                        andQuery:impl()
                                                andValueListener:nullptr
                                                andChildListener:nullptr];
  SafeFutureHandle<DataSnapshot> future_handle =
      query_future()->SafeAlloc<DataSnapshot>(kQueryFnGetValue, DataSnapshot(nullptr));
  SingleValueListener* single_listener = new SingleValueListener(
      query_future(), future_handle, FIRCPPDatabaseQueryCallbackStatePointer(callback_state));
  callback_state.valueListener = single_listener;

  void (^block)(FIRDataSnapshot* _Nonnull) = ^(FIRDataSnapshot* _Nonnull snapshot_impl) {
    NotifyValueListener(callback_state, snapshot_impl,
                        [](DatabaseInternal* _Nonnull database, ValueListener* _Nonnull listener,
                           const DataSnapshot& snapshot) {
                          listener->OnValueChanged(snapshot);
                          database->RemoveSingleValueListener(listener);
                          delete listener;
                        });
  };

  void (^cancel_block)(NSError* _Nonnull) = ^(NSError* _Nonnull error) {
    CancelValueListener(callback_state, error,
                        [](DatabaseInternal* _Nonnull database, ValueListener* _Nonnull listener,
                           const Error& error_code, const char* _Nonnull error_string) {
                          listener->OnCancelled(error_code, error_string);
                          database->RemoveSingleValueListener(listener);
                          delete listener;
                        });
  };

  [impl() observeSingleEventOfType:FIRDataEventTypeValue
                         withBlock:block
                   withCancelBlock:cancel_block];

  database_->AddSingleValueListener(single_listener);

  return MakeFuture(query_future(), future_handle);
}

Future<DataSnapshot> QueryInternal::GetValueLastResult() {
  return static_cast<const Future<DataSnapshot>&>(query_future()->LastResult(kQueryFnGetValue));
}

void QueryInternal::AddValueListener(ValueListener* value_listener) {
  FIRCPPDatabaseQueryCallbackState* callback_state =
      [[FIRCPPDatabaseQueryCallbackState alloc] initWithDatabase:database_
                                                        andQuery:impl()
                                                andValueListener:value_listener
                                                andChildListener:nullptr];
  void (^block)(FIRDataSnapshot* _Nonnull) = ^(FIRDataSnapshot* _Nonnull snapshot_impl) {
    NotifyValueListener(callback_state, snapshot_impl,
                        [](DatabaseInternal* _Nonnull database, ValueListener* _Nonnull listener,
                           const DataSnapshot& snapshot) {
                          (void)database;
                          listener->OnValueChanged(snapshot);
                        });
  };

  void (^cancel_block)(NSError* _Nonnull) = ^(NSError* _Nonnull error) {
    CancelValueListener(callback_state, error,
                        [](DatabaseInternal* _Nonnull database, ValueListener* _Nonnull listener,
                           const Error& error_code, const char* _Nonnull error_string) {
                          (void)database;
                          listener->OnCancelled(error_code, error_string);
                        });
  };

  [callback_state addObserverHandle:[impl() observeEventType:FIRDataEventTypeValue
                                                   withBlock:block
                                             withCancelBlock:cancel_block]];
  if (!database_->RegisterValueListener(query_spec_, value_listener, callback_state)) {
    database_->logger()->LogWarning(
        "Query::AddValueListener (URL = %s): You may not register the same ValueListener"
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

typedef void (*NotifyChildListenerFunc)(ChildListener* listener, const DataSnapshot& snapshot,
                                        const char* key);

// Safely attempt to notify a child listener.
static void NotifyChildListener(FIRCPPDatabaseQueryCallbackState* callback_state,
                                FIRDataSnapshot* _Nonnull snapshot_impl, NSString* previous_key,
                                NotifyChildListenerFunc _Nonnull notify) {
  [callback_state.lock lock];
  DatabaseInternal* database = callback_state.databaseInternal;
  ChildListener* listener = callback_state.childListener;
  if (database && listener) {
    notify(listener,
           DataSnapshot(new DataSnapshotInternal(
               database, MakeUnique<FIRDataSnapshotPointer>(snapshot_impl))),
           [previous_key UTF8String]);
  }
  [callback_state.lock unlock];
}

void QueryInternal::AddChildListener(ChildListener* child_listener) {
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
                            [](ChildListener* listener, const DataSnapshot& snapshot,
                               const char* key) { listener->OnChildAdded(snapshot, key); });
      };

  CallbackBlock child_changed_block =
      ^(FIRDataSnapshot* _Nonnull snapshot_impl, NSString* previous_key) {
        NotifyChildListener(callback_state, snapshot_impl, previous_key,
                            [](ChildListener* listener, const DataSnapshot& snapshot,
                               const char* key) { listener->OnChildChanged(snapshot, key); });
      };

  CallbackBlock child_moved_block =
      ^(FIRDataSnapshot* _Nonnull snapshot_impl, NSString* previous_key) {
        NotifyChildListener(callback_state, snapshot_impl, previous_key,
                            [](ChildListener* listener, const DataSnapshot& snapshot,
                               const char* key) { listener->OnChildMoved(snapshot, key); });
      };

  CallbackBlock child_removed_block = ^(FIRDataSnapshot* _Nonnull snapshot_impl,
                                        NSString* previous_key) {
    NotifyChildListener(callback_state, snapshot_impl, previous_key,
                        [](ChildListener* listener, const DataSnapshot& snapshot, const char* key) {
                          (void)key;
                          listener->OnChildRemoved(snapshot);
                        });
  };

  CancelBlock cancel_block = ^(NSError* _Nonnull error) {
    Error error_code = NSErrorToErrorCode(error);
    const char* error_string = GetErrorMessage(error_code);
    [callback_state.lock lock];
    ChildListener* listener = callback_state.childListener;
    if (listener) listener->OnCancelled(error_code, error_string);
    [callback_state.lock unlock];
  };

  FIRDatabaseQuery* query = impl();
  [callback_state addObserverHandle:[query observeEventType:FIRDataEventTypeChildAdded
                                        andPreviousSiblingKeyWithBlock:child_added_block
                                                       withCancelBlock:cancel_block]];
  [callback_state addObserverHandle:[query observeEventType:FIRDataEventTypeChildChanged
                                        andPreviousSiblingKeyWithBlock:child_changed_block]];
  [callback_state addObserverHandle:[query observeEventType:FIRDataEventTypeChildMoved
                                        andPreviousSiblingKeyWithBlock:child_moved_block]];
  [callback_state addObserverHandle:[query observeEventType:FIRDataEventTypeChildRemoved
                                        andPreviousSiblingKeyWithBlock:child_removed_block]];

  if (!database_->RegisterChildListener(query_spec_, child_listener, callback_state)) {
    Logger* logger = database_->logger();
    logger->LogWarning("Query::AddChildListener (URL = %s): You may not register the same "
                       "ChildListener more than once on the same Query.",
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
  } @catch (NSException* exception) {
    LogException(database_->logger(), "OrderByChild", query_spec_.path.c_str(), exception);
    return nullptr;
  }
}

QueryInternal* QueryInternal::OrderByKey() {
  @try {
    internal::QuerySpec spec = query_spec_;
    spec.params.order_by = internal::QueryParams::kOrderByKey;
    return new QueryInternal(database_,
                             MakeUnique<FIRDatabaseQueryPointer>([impl() queryOrderedByKey]), spec);
  } @catch (NSException* exception) {
    LogException(database_->logger(), "OrderByKey", query_spec_.path.c_str(), exception);
    return nullptr;
  }
}

QueryInternal* QueryInternal::OrderByPriority() {
  @try {
    internal::QuerySpec spec = query_spec_;
    spec.params.order_by = internal::QueryParams::kOrderByPriority;
    return new QueryInternal(
        database_, MakeUnique<FIRDatabaseQueryPointer>([impl() queryOrderedByPriority]), spec);
  } @catch (NSException* exception) {
    LogException(database_->logger(), "OrderByPriority", query_spec_.path.c_str(), exception);
    return nullptr;
  }
}

QueryInternal* QueryInternal::OrderByValue() {
  @try {
    internal::QuerySpec spec = query_spec_;
    spec.params.order_by = internal::QueryParams::kOrderByValue;
    return new QueryInternal(
        database_, MakeUnique<FIRDatabaseQueryPointer>([impl() queryOrderedByValue]), spec);
  } @catch (NSException* exception) {
    LogException(database_->logger(), "OrderByValue", query_spec_.path.c_str(), exception);
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
  } @catch (NSException* exception) {
    LogException(database_->logger(), "StartAt", query_spec_.path.c_str(), exception);
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
  } @catch (NSException* exception) {
    LogException(database_->logger(), "StartAt", query_spec_.path.c_str(), exception);
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
  } @catch (NSException* exception) {
    LogException(database_->logger(), "EndAt", query_spec_.path.c_str(), exception);
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
  } @catch (NSException* exception) {
    LogException(database_->logger(), "EndAt", query_spec_.path.c_str(), exception);
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
  } @catch (NSException* exception) {
    LogException(database_->logger(), "EqualTo", query_spec_.path.c_str(), exception);
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
  } @catch (NSException* exception) {
    LogException(database_->logger(), "EqualTo", query_spec_.path.c_str(), exception);
    return nullptr;
  }
}

QueryInternal* QueryInternal::LimitToFirst(size_t limit) {
  @try {
    internal::QuerySpec spec = query_spec_;
    spec.params.limit_first = limit;
    return new QueryInternal(
        database_, MakeUnique<FIRDatabaseQueryPointer>([impl() queryLimitedToFirst:limit]), spec);
  } @catch (NSException* exception) {
    LogException(database_->logger(), "LimitToFirst", query_spec_.path.c_str(), exception);
    return nullptr;
  }
}

QueryInternal* QueryInternal::LimitToLast(size_t limit) {
  @try {
    internal::QuerySpec spec = query_spec_;
    spec.params.limit_last = limit;
    return new QueryInternal(
        database_, MakeUnique<FIRDatabaseQueryPointer>([impl() queryLimitedToLast:limit]), spec);
  } @catch (NSException* exception) {
    LogException(database_->logger(), "LimitToLast", query_spec_.path.c_str(), exception);
    return nullptr;
  }
}

ReferenceCountedFutureImpl* QueryInternal::query_future() {
  return database_->future_manager().GetFutureApi(&future_api_id_);
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
