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
      ref_future_api_(database) {
  const char* key =
      impl_ && impl_->get() ? [[impl_->get() key] UTF8String] : nullptr;
  cached_key_ = key ? key : "";
  query_spec_.path = Path(GetUrl());
}

DatabaseReferenceInternal::~DatabaseReferenceInternal() {
  delete cached_disconnection_handler_;
  cached_disconnection_handler_ = nullptr;
}

DatabaseReferenceInternal::DatabaseReferenceInternal(
    const DatabaseReferenceInternal& other)
    : QueryInternal(other),
      impl_(std::make_unique<FIRDatabaseReferencePointer>(other.impl())),
      cached_key_(other.cached_key_),
      ref_future_api_(other.ref_future_api_) {}

DatabaseReferenceInternal& DatabaseReferenceInternal::operator=(
    const DatabaseReferenceInternal& other) {
  if (&other == this) return *this;
  delete cached_disconnection_handler_;
  cached_disconnection_handler_ = nullptr;
  cached_key_ = other.cached_key_;
  QueryInternal::operator=(other);
  ref_future_api_ = other.ref_future_api_;
  impl_ = std::make_unique<FIRDatabaseReferencePointer>(other.impl());
  return *this;
}

DatabaseReferenceInternal::DatabaseReferenceInternal(
    DatabaseReferenceInternal&& other) noexcept
    : QueryInternal(std::move(other)),
      impl_(std::move(other.impl_)),
      cached_disconnection_handler_(other.cached_disconnection_handler_),
      cached_key_(std::move(other.cached_key_)),
      ref_future_api_(std::move(other.ref_future_api_)) {
  other.cached_disconnection_handler_ = nullptr;
}

DatabaseReferenceInternal& DatabaseReferenceInternal::operator=(
    DatabaseReferenceInternal&& other) noexcept {
  if (&other == this) return *this;
  delete cached_disconnection_handler_;
  cached_disconnection_handler_ = other.cached_disconnection_handler_;
  other.cached_disconnection_handler_ = nullptr;
  cached_key_ = std::move(other.cached_key_);
  QueryInternal::operator=(std::move(other));
  impl_ = std::move(other.impl_);
  ref_future_api_ = std::move(other.ref_future_api_);
  return *this;
}

Database* DatabaseReferenceInternal::GetDatabase() const {
  if (!database_) return nullptr;
  return database_->constructor_url().empty()
             ? Database::GetInstance(database_->GetApp())
             : Database::GetInstance(database_->GetApp(),
                                     database_->constructor_url().c_str());
}

bool DatabaseReferenceInternal::IsRoot() const {
  return impl() ? !impl().parent : true;
}

DatabaseReferenceInternal* DatabaseReferenceInternal::GetParent() {
  if (!database_ || !impl()) return nullptr;
  FIRDatabaseReference* parent = impl().parent;
  return new DatabaseReferenceInternal(
      database_,
      std::make_unique<FIRDatabaseReferencePointer>(parent ? parent : impl()));
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
    return child_ref ? new DatabaseReferenceInternal(
                           database_,
                           std::make_unique<FIRDatabaseReferencePointer>(
                               child_ref))
                     : nullptr;
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

Future<DataSnapshot> DatabaseReferenceInternal::RunTransaction(
    DoTransactionWithContext transaction_function, void* context,
    void (*delete_context)(void*), bool trigger_local_events) {
  ReferenceCountedFutureImpl* future_api = ref_future();
  if (!database_ || !future_api) {
    if (delete_context) delete_context(context);
    return Future<DataSnapshot>();
  }
  SafeFutureHandle<DataSnapshot> handle = future_api->SafeAlloc<DataSnapshot>(
      kDatabaseReferenceFnRunTransaction, DataSnapshot(nullptr));
  Future<DataSnapshot> result_future = MakeFuture(future_api, handle);
  if (!impl()) {
    future_api->Complete(handle, kErrorOperationFailed,
                         GetErrorMessage(kErrorOperationFailed));
    if (delete_context) delete_context(context);
    return result_future;
  }

  auto* state = new TransactionCallbackState{handle, future_api, result_future,
                                             database_->this_ref()};
  FIRTransactionResult* _Nonnull (^transaction_block)(
      FIRMutableData* _Nonnull) = ^(FIRMutableData* mutable_data_impl) {
    DatabaseInternal::ThisRefLock lock(&state->db_ref);
    DatabaseInternal* db = lock.GetReference();
    if (!db) return [FIRTransactionResult abort];
    MutableData mutable_data(new MutableDataInternal(
        db, std::make_unique<FIRMutableDataPointer>(mutable_data_impl)));
    return transaction_function(&mutable_data, context) ==
                   kTransactionResultSuccess
               ? [FIRTransactionResult successWithValue:mutable_data_impl]
               : [FIRTransactionResult abort];
  };
  void (^completion_block)(NSError* _Nullable, BOOL,
                           FIRDataSnapshot* _Nullable) =
      ^(NSError* error, BOOL was_committed, FIRDataSnapshot* snapshot) {
        {
          DatabaseInternal::ThisRefLock lock(&state->db_ref);
          if (DatabaseInternal* db = lock.GetReference()) {
            Error error_code = NSErrorToErrorCode(error);
            const char* error_string = GetErrorMessage(error_code);
            if (error == nil) {
              DataSnapshot result_snapshot =
                  snapshot != nil
                      ? DataSnapshot(new DataSnapshotInternal(
                            db,
                            std::make_unique<FIRDataSnapshotPointer>(snapshot)))
                      : DataSnapshot(nullptr);
              Error final_err =
                  was_committed ? error_code : kErrorTransactionAbortedByUser;
              state->impl->CompleteWithResult(
                  state->handle, final_err, GetErrorMessage(final_err),
                  result_snapshot);
            } else {
              state->impl->Complete(state->handle, error_code, error_string);
            }
          }
        }
        if (delete_context) delete_context(context);
        delete state;
      };
  [impl() runTransactionBlock:transaction_block
           andCompletionBlock:completion_block
              withLocalEvents:trigger_local_events ? YES : NO];
  return result_future;
}

void DatabaseReferenceInternal::RemoveValueImpl(SafeFutureHandle<void> handle) {
  ReferenceCountedFutureImpl* api = ref_future();
  if (!database_ || !api) return;
  if (!impl()) {
    api->Complete(handle, kErrorOperationFailed,
                  GetErrorMessage(kErrorOperationFailed));
    return;
  }
  [impl() removeValueWithCompletionBlock:CreateCompletionBlock(handle, api)];
}

void DatabaseReferenceInternal::SetPriorityImpl(const Variant& priority,
                                                SafeFutureHandle<void> handle) {
  ReferenceCountedFutureImpl* api = ref_future();
  if (!database_ || !api) return;
  if (!impl()) {
    api->Complete(handle, kErrorOperationFailed,
                  GetErrorMessage(kErrorOperationFailed));
    return;
  }
  [impl() setPriority:VariantToId(priority)
      withCompletionBlock:CreateCompletionBlock(handle, api)];
}

void DatabaseReferenceInternal::SetValueImpl(const Variant& value,
                                             SafeFutureHandle<void> handle) {
  ReferenceCountedFutureImpl* api = ref_future();
  if (!database_ || !api) return;
  if (!impl()) {
    api->Complete(handle, kErrorOperationFailed,
                  GetErrorMessage(kErrorOperationFailed));
    return;
  }
  [impl() setValue:VariantToId(value)
      withCompletionBlock:CreateCompletionBlock(handle, api)];
}

void DatabaseReferenceInternal::SetValueAndPriorityImpl(
    const Variant& value, const Variant& priority,
    SafeFutureHandle<void> handle) {
  ReferenceCountedFutureImpl* api = ref_future();
  if (!database_ || !api) return;
  if (!impl()) {
    api->Complete(handle, kErrorOperationFailed,
                  GetErrorMessage(kErrorOperationFailed));
    return;
  }
  [impl() setValue:VariantToId(value)
              andPriority:VariantToId(priority)
      withCompletionBlock:CreateCompletionBlock(handle, api)];
}

void DatabaseReferenceInternal::UpdateChildrenImpl(
    const Variant& values, SafeFutureHandle<void> handle) {
  ReferenceCountedFutureImpl* api = ref_future();
  if (!database_ || !api) return;
  if (!impl()) {
    api->Complete(handle, kErrorOperationFailed,
                  GetErrorMessage(kErrorOperationFailed));
    return;
  }
  [impl() updateChildValues:(NSDictionary*)VariantToId(values)
        withCompletionBlock:CreateCompletionBlock(handle, api)];
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

}  // namespace internal
}  // namespace database
}  // namespace firebase
