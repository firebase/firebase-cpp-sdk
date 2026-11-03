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

#include "database/src/ios/database_reference_ios.h"

#include <memory>
#include <string>
#include <utility>

#include "FIRDatabase.h"
#include "app/src/util_ios.h"
#include "database/src/common/database_reference.h"
#include "database/src/include/firebase/database/database_reference.h"
#include "database/src/ios/completion_block_ios.h"
#include "database/src/ios/data_snapshot_ios.h"
#include "database/src/ios/database_ios.h"
#include "database/src/ios/disconnection_ios.h"
#include "database/src/ios/mutable_data_ios.h"
#include "database/src/ios/util_ios.h"

namespace firebase {
namespace database {
namespace internal {

using util::VariantToId;

namespace {

struct TransactionCallbackState {
  TransactionCallbackState(SafeFutureHandle<DataSnapshot> handle_in,
                           ReferenceCountedFutureImpl* impl_in,
                           const Future<DataSnapshot>& backing_future_in,
                           const DatabaseInternal::ThisRef& db_ref_in)
      : handle(handle_in),
        impl(impl_in),
        backing_future(backing_future_in),
        db_ref(db_ref_in) {}

  SafeFutureHandle<DataSnapshot> handle;
  ReferenceCountedFutureImpl* impl;
  Future<DataSnapshot> backing_future;
  DatabaseInternal::ThisRef db_ref;
};

}  // namespace

DatabaseReferenceInternal::DatabaseReferenceInternal(
    DatabaseInternal* database,
    std::unique_ptr<FIRDatabaseReferencePointer> impl)
    : QueryInternal(database, std::make_unique<FIRDatabaseQueryPointer>(
                                  impl ? impl->get() : nil)),
      impl_(std::move(impl)),
      cached_disconnection_handler_(nullptr) {
  const char* key = impl_ && impl_->get() ? [[impl_->get() key] UTF8String] : nullptr;
  cached_key_ = key ? key : "";
  if (database_) {
    database_->future_manager().AllocFutureApi(&future_api_id_,
                                               kDatabaseReferenceFnCount);
  }
  // Read the path this db reference refers to so that it can be put into the
  // QuerySpec. The URL includes https://... on it, which is fine on iOS where
  // the QuerySpec path is only used for sorting and checking equality.
  query_spec_.path = Path(GetUrl());
}

DatabaseReferenceInternal::~DatabaseReferenceInternal() {
  delete cached_disconnection_handler_;
  cached_disconnection_handler_ = nullptr;
  if (database_) {
    database_->future_manager().ReleaseFutureApi(&future_api_id_);
  }
}

DatabaseReferenceInternal::DatabaseReferenceInternal(
    const DatabaseReferenceInternal& other)
    : QueryInternal(other),
      impl_(std::make_unique<FIRDatabaseReferencePointer>(other.impl())),
      cached_disconnection_handler_(nullptr),
      cached_key_(other.cached_key_) {
  if (database_) {
    database_->future_manager().AllocFutureApi(&future_api_id_,
                                               kDatabaseReferenceFnCount);
  }
}

DatabaseReferenceInternal& DatabaseReferenceInternal::operator=(
    const DatabaseReferenceInternal& other) {
  if (&other == this) return *this;
  delete cached_disconnection_handler_;
  cached_disconnection_handler_ = nullptr;
  cached_key_ = other.cached_key_;
  if (database_ != other.database_) {
    if (database_) {
      database_->future_manager().ReleaseFutureApi(&future_api_id_);
    }
    QueryInternal::operator=(other);
    if (database_) {
      database_->future_manager().AllocFutureApi(&future_api_id_,
                                                 kDatabaseReferenceFnCount);
    }
  } else {
    QueryInternal::operator=(other);
  }
  impl_ = std::make_unique<FIRDatabaseReferencePointer>(other.impl());
  return *this;
}

DatabaseReferenceInternal::DatabaseReferenceInternal(
    DatabaseReferenceInternal&& other) noexcept
    : QueryInternal(std::move(other)),
      impl_(std::move(other.impl_)),
      cached_disconnection_handler_(other.cached_disconnection_handler_),
      cached_key_(std::move(other.cached_key_)) {
  other.cached_disconnection_handler_ = nullptr;
  if (database_) {
    database_->future_manager().MoveFutureApi(&other.future_api_id_,
                                              &future_api_id_);
  }
}

DatabaseReferenceInternal& DatabaseReferenceInternal::operator=(
    DatabaseReferenceInternal&& other) noexcept {
  if (&other == this) return *this;
  delete cached_disconnection_handler_;
  cached_disconnection_handler_ = other.cached_disconnection_handler_;
  other.cached_disconnection_handler_ = nullptr;
  cached_key_ = std::move(other.cached_key_);
  if (database_) {
    database_->future_manager().ReleaseFutureApi(&future_api_id_);
  }
  QueryInternal::operator=(std::move(other));
  impl_ = std::move(other.impl_);
  if (database_) {
    database_->future_manager().MoveFutureApi(&other.future_api_id_,
                                              &future_api_id_);
  }
  return *this;
}

Database* DatabaseReferenceInternal::GetDatabase() const {
  if (!database_) return nullptr;
  return database_->constructor_url().empty()
             ? Database::GetInstance(database_->GetApp())
             : Database::GetInstance(database_->GetApp(),
                                     database_->constructor_url().c_str());
}

const char* DatabaseReferenceInternal::GetKey() const {
  return cached_key_.c_str();
}

std::string DatabaseReferenceInternal::GetKeyString() const {
  return cached_key_;
}

bool DatabaseReferenceInternal::IsRoot() const {
  return impl() ? !impl().parent : true;
}

DatabaseReferenceInternal* DatabaseReferenceInternal::GetParent() {
  if (!database_ || !impl()) return nullptr;
  FIRDatabaseReference* parent = impl().parent;
  // If *this* is the root, the iOS SDK returns null.
  // But the Android C++ implementation returns the root again, so do that.
  return parent ? new DatabaseReferenceInternal(
                      database_,
                      std::make_unique<FIRDatabaseReferencePointer>(parent))
                : new DatabaseReferenceInternal(
                      database_,
                      std::make_unique<FIRDatabaseReferencePointer>(impl()));
}

DatabaseReferenceInternal* DatabaseReferenceInternal::GetRoot() {
  if (!database_ || !impl()) return nullptr;
  return new DatabaseReferenceInternal(
      database_, std::make_unique<FIRDatabaseReferencePointer>(impl().root));
}

DatabaseReferenceInternal* DatabaseReferenceInternal::Child(const char* path) {
  if (path == nullptr || !database_ || !impl()) return nullptr;
  @try {
    NSString* path_str = @(path);
    if (!path_str) return nullptr;
    FIRDatabaseReference* child_ref = [impl() child:path_str];
    if (!child_ref) return nullptr;
    return new DatabaseReferenceInternal(
        database_, std::make_unique<FIRDatabaseReferencePointer>(child_ref));
  } @catch (NSException* exception) {
    const char* reason = exception.reason.UTF8String
                             ? exception.reason.UTF8String
                             : "Unknown exception";
    database_->logger()->LogWarning(
        "DatabaseReference::Child: (URL = %s) Couldn't create child reference "
        "%s: %s",
        query_spec_.path.c_str(), path, reason);
    return nullptr;
  }
}

DatabaseReferenceInternal* DatabaseReferenceInternal::PushChild() {
  if (!database_ || !impl()) return nullptr;
  return new DatabaseReferenceInternal(
      database_,
      std::make_unique<FIRDatabaseReferencePointer>([impl() childByAutoId]));
}

Future<void> DatabaseReferenceInternal::RemoveValue() {
  if (!database_) return Future<void>();
  ReferenceCountedFutureImpl* future_api = ref_future();
  SafeFutureHandle<void> handle =
      future_api->SafeAlloc<void>(kDatabaseReferenceFnRemoveValue);
  Future<void> result_future = MakeFuture(future_api, handle);
  if (!impl()) {
    future_api->Complete(handle, kErrorOperationFailed,
                         GetErrorMessage(kErrorOperationFailed));
    return result_future;
  }
  [impl()
      removeValueWithCompletionBlock:CreateCompletionBlock(handle, future_api)];
  return result_future;
}

Future<void> DatabaseReferenceInternal::RemoveValueLastResult() {
  return static_cast<const Future<void>&>(
      ref_future()->LastResult(kDatabaseReferenceFnRemoveValue));
}

Future<DataSnapshot> DatabaseReferenceInternal::RunTransaction(
    DoTransactionWithContext transaction_function, void* context,
    void (*delete_context)(void*), bool trigger_local_events) {
  if (!database_) {
    if (delete_context) delete_context(context);
    return Future<DataSnapshot>();
  }
  ReferenceCountedFutureImpl* future_api = ref_future();
  SafeFutureHandle<DataSnapshot> handle = future_api->SafeAlloc<DataSnapshot>(
      kDatabaseReferenceFnRunTransaction, DataSnapshot(nullptr));
  Future<DataSnapshot> result_future = MakeFuture(future_api, handle);
  if (!impl()) {
    future_api->Complete(handle, kErrorOperationFailed,
                         GetErrorMessage(kErrorOperationFailed));
    if (delete_context) delete_context(context);
    return result_future;
  }

  DatabaseInternal::ThisRef db_ref = database_->this_ref();
  FIRTransactionResult* _Nonnull (^transaction_block)(
      FIRMutableData* _Nonnull) = ^(FIRMutableData* mutable_data_impl) {
    DatabaseInternal::ThisRefLock lock(&db_ref);
    DatabaseInternal* db = lock.GetReference();
    if (!db) {
      return [FIRTransactionResult abort];
    }
    MutableData mutable_data(new MutableDataInternal(
        db, std::make_unique<FIRMutableDataPointer>(mutable_data_impl)));
    TransactionResult result = transaction_function(&mutable_data, context);
    if (result == kTransactionResultSuccess) {
      return [FIRTransactionResult successWithValue:mutable_data_impl];
    } else {  // result == kTransactionResultAbort
      return [FIRTransactionResult abort];
    }
  };

  TransactionCallbackState* state =
      new TransactionCallbackState(handle, future_api, result_future, db_ref);
  void (^completion_block)(NSError* _Nullable, BOOL,
                           FIRDataSnapshot* _Nullable) =
      ^(NSError* error, BOOL was_committed, FIRDataSnapshot* snapshot) {
        {
          DatabaseInternal::ThisRefLock lock(&state->db_ref);
          DatabaseInternal* db = lock.GetReference();
          if (db != nullptr) {
            Error error_code = NSErrorToErrorCode(error);
            const char* error_string = GetErrorMessage(error_code);
            if (error == nil) {
              DataSnapshot result_snapshot =
                  (snapshot != nil)
                      ? DataSnapshot(new DataSnapshotInternal(
                            db,
                            std::make_unique<FIRDataSnapshotPointer>(snapshot)))
                      : DataSnapshot(nullptr);
              if (was_committed) {
                // Succeeded.
                state->impl->CompleteWithResult(state->handle, error_code,
                                                error_string, result_snapshot);
              } else {
                // Aborted.
                state->impl->CompleteWithResult(
                    state->handle, kErrorTransactionAbortedByUser,
                    GetErrorMessage(kErrorTransactionAbortedByUser),
                    result_snapshot);
              }
            } else {
              // Error occurred.
              state->impl->Complete(state->handle, error_code, error_string);
            }
          }
        }
        if (delete_context) {
          delete_context(context);
        }
        delete state;
      };
  [impl() runTransactionBlock:transaction_block
           andCompletionBlock:completion_block
              withLocalEvents:trigger_local_events ? YES : NO];
  return result_future;
}

Future<DataSnapshot> DatabaseReferenceInternal::RunTransactionLastResult() {
  return static_cast<const Future<DataSnapshot>&>(
      ref_future()->LastResult(kDatabaseReferenceFnRunTransaction));
}

Future<void> DatabaseReferenceInternal::SetPriority(const Variant& priority) {
  if (!database_) return Future<void>();
  ReferenceCountedFutureImpl* future_api = ref_future();
  SafeFutureHandle<void> handle =
      future_api->SafeAlloc<void>(kDatabaseReferenceFnSetPriority);
  Future<void> result_future = MakeFuture(future_api, handle);
  if (!impl()) {
    future_api->Complete(handle, kErrorOperationFailed,
                         GetErrorMessage(kErrorOperationFailed));
    return result_future;
  }
  if (SetValueAndPriorityLastResult().status() == kFutureStatusPending) {
    future_api->Complete(handle, kErrorConflictingOperationInProgress,
                           kErrorMsgConflictSetPriority);
  } else if (!IsValidPriority(priority)) {
    future_api->Complete(handle, kErrorInvalidVariantType,
                           kErrorMsgInvalidVariantForPriority);
  } else {
    [impl() setPriority:VariantToId(priority)
        withCompletionBlock:CreateCompletionBlock(handle, future_api)];
  }
  return result_future;
}

Future<void> DatabaseReferenceInternal::SetPriorityLastResult() {
  return static_cast<const Future<void>&>(
      ref_future()->LastResult(kDatabaseReferenceFnSetPriority));
}

Future<void> DatabaseReferenceInternal::SetValue(const Variant& value) {
  if (!database_) return Future<void>();
  ReferenceCountedFutureImpl* future_api = ref_future();
  SafeFutureHandle<void> handle =
      future_api->SafeAlloc<void>(kDatabaseReferenceFnSetValue);
  Future<void> result_future = MakeFuture(future_api, handle);
  if (!impl()) {
    future_api->Complete(handle, kErrorOperationFailed,
                         GetErrorMessage(kErrorOperationFailed));
    return result_future;
  }
  if (SetValueAndPriorityLastResult().status() == kFutureStatusPending) {
    future_api->Complete(handle, kErrorConflictingOperationInProgress,
                           kErrorMsgConflictSetValue);
  } else {
    [impl() setValue:VariantToId(value)
        withCompletionBlock:CreateCompletionBlock(handle, future_api)];
  }
  return result_future;
}

Future<void> DatabaseReferenceInternal::SetValueLastResult() {
  return static_cast<const Future<void>&>(
      ref_future()->LastResult(kDatabaseReferenceFnSetValue));
}

Future<void> DatabaseReferenceInternal::SetValueAndPriority(
    const Variant& value, const Variant& priority) {
  if (!database_) return Future<void>();
  ReferenceCountedFutureImpl* future_api = ref_future();
  SafeFutureHandle<void> handle =
      future_api->SafeAlloc<void>(kDatabaseReferenceFnSetValueAndPriority);
  Future<void> result_future = MakeFuture(future_api, handle);
  if (!impl()) {
    future_api->Complete(handle, kErrorOperationFailed,
                         GetErrorMessage(kErrorOperationFailed));
    return result_future;
  }
  if (SetValueLastResult().status() == kFutureStatusPending) {
    future_api->Complete(handle, kErrorConflictingOperationInProgress,
                           kErrorMsgConflictSetValue);
  } else if (SetPriorityLastResult().status() == kFutureStatusPending) {
    future_api->Complete(handle, kErrorConflictingOperationInProgress,
                           kErrorMsgConflictSetPriority);
  } else if (!IsValidPriority(priority)) {
    future_api->Complete(handle, kErrorInvalidVariantType,
                           kErrorMsgInvalidVariantForPriority);
  } else {
    [impl() setValue:VariantToId(value)
                andPriority:VariantToId(priority)
        withCompletionBlock:CreateCompletionBlock(handle, future_api)];
  }
  return result_future;
}

Future<void> DatabaseReferenceInternal::SetValueAndPriorityLastResult() {
  return static_cast<const Future<void>&>(
      ref_future()->LastResult(kDatabaseReferenceFnSetValueAndPriority));
}

Future<void> DatabaseReferenceInternal::UpdateChildren(const Variant& values) {
  if (!database_) return Future<void>();
  ReferenceCountedFutureImpl* future_api = ref_future();
  SafeFutureHandle<void> handle =
      future_api->SafeAlloc<void>(kDatabaseReferenceFnUpdateChildren);
  Future<void> result_future = MakeFuture(future_api, handle);
  if (!impl()) {
    future_api->Complete(handle, kErrorOperationFailed,
                         GetErrorMessage(kErrorOperationFailed));
    return result_future;
  }
  if (!values.is_map()) {
    future_api->Complete(handle, kErrorInvalidVariantType,
                           kErrorMsgInvalidVariantForUpdateChildren);
  } else {
    id values_id = VariantToId(values);
    NSDictionary* values_dict = (NSDictionary*)values_id;
    [impl() updateChildValues:values_dict
          withCompletionBlock:CreateCompletionBlock(handle, future_api)];
  }
  return result_future;
}

Future<void> DatabaseReferenceInternal::UpdateChildrenLastResult() {
  return static_cast<const Future<void>&>(
      ref_future()->LastResult(kDatabaseReferenceFnUpdateChildren));
}

std::string DatabaseReferenceInternal::GetUrl() const {
  NSString* desc = impl() ? [impl() description] : nil;
  const char* url_cstr = desc ? desc.UTF8String : nullptr;
  return url_cstr ? std::string(url_cstr) : std::string();
}

DisconnectionHandler* DatabaseReferenceInternal::OnDisconnect() {
  if (!database_ || !impl()) return nullptr;
  if (cached_disconnection_handler_ == nullptr) {
    cached_disconnection_handler_ =
        new DisconnectionHandler(new DisconnectionHandlerInternal(
            database_, std::make_unique<FIRDatabaseReferencePointer>(impl())));
  }
  return cached_disconnection_handler_;
}

void DatabaseReferenceInternal::GoOffline() {
  [FIRDatabaseReference goOffline];
}

void DatabaseReferenceInternal::GoOnline() { [FIRDatabaseReference goOnline]; }

ReferenceCountedFutureImpl* DatabaseReferenceInternal::ref_future() {
  return database_->future_manager().GetFutureApi(&future_api_id_);
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
