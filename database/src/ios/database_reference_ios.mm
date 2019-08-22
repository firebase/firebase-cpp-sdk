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

DatabaseReferenceInternal::DatabaseReferenceInternal(DatabaseInternal* database,
                                                     UniquePtr<FIRDatabaseReferencePointer> impl)
    : QueryInternal(database, MakeUnique<FIRDatabaseQueryPointer>(impl->get())),
      cached_disconnection_handler_(nullptr) {
  impl_ = std::move(impl);
  database_->future_manager().AllocFutureApi(&future_api_id_, kDatabaseReferenceFnCount);
  // Now we need to read the path this db reference refers to, so that it can be put into the
  // QuerySpec.
  // The URL includes the https://... on it, but that doesn't matter on iOS where the QuerySpec path
  // is only used for sorting checking equality.
  query_spec_.path = Path(GetUrl());
}

DatabaseReferenceInternal::~DatabaseReferenceInternal() {
  if (cached_disconnection_handler_) delete cached_disconnection_handler_;
  database_->future_manager().ReleaseFutureApi(&future_api_id_);
}

DatabaseReferenceInternal::DatabaseReferenceInternal(const DatabaseReferenceInternal& other)
    : QueryInternal(other), cached_disconnection_handler_(nullptr) {
  impl_.reset(new FIRDatabaseReferencePointer(*other.impl_));
  database_->future_manager().AllocFutureApi(&future_api_id_, kDatabaseReferenceFnCount);
}

DatabaseReferenceInternal& DatabaseReferenceInternal::operator=(
    const DatabaseReferenceInternal& other) {
  QueryInternal::operator=(other);
  impl_.reset(new FIRDatabaseReferencePointer(*other.impl_));
  return *this;
}

#if defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)
DatabaseReferenceInternal::DatabaseReferenceInternal(DatabaseReferenceInternal&& other)
    : QueryInternal(std::move(other)), impl_(std::move(other.impl_)) {
  other.impl_ = MakeUnique<FIRDatabaseReferencePointer>(nil);
  database_->future_manager().MoveFutureApi(&other.future_api_id_, &future_api_id_);
}

DatabaseReferenceInternal& DatabaseReferenceInternal::operator=(DatabaseReferenceInternal&& other) {
  QueryInternal::operator=(std::move(other));
  impl_ = std::move(other.impl_);
  other.impl_ = MakeUnique<FIRDatabaseReferencePointer>(nil);
  database_->future_manager().MoveFutureApi(&other.future_api_id_, &future_api_id_);
  return *this;
}
#endif  // defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)

Database* DatabaseReferenceInternal::GetDatabase() const {
  return Database::GetInstance(database_->GetApp());
}

const char* DatabaseReferenceInternal::GetKey() const { return [[impl() key] UTF8String]; }

std::string DatabaseReferenceInternal::GetKeyString() const {
  const char* key = [[impl() key] UTF8String];
  return std::string(key ? key : "");
}

bool DatabaseReferenceInternal::IsRoot() const { return !impl().parent; }

DatabaseReferenceInternal* DatabaseReferenceInternal::GetParent() {
  FIRDatabaseReference* parent = impl().parent;
  // If *this* is the root, the iOS SDK returns null.
  // But the Android C++ implementation returns the root again, so do that.
  return parent ? new DatabaseReferenceInternal(database_,
                                                MakeUnique<FIRDatabaseReferencePointer>(parent))
                : new DatabaseReferenceInternal(database_,
                                                MakeUnique<FIRDatabaseReferencePointer>(impl()));
}

DatabaseReferenceInternal* DatabaseReferenceInternal::GetRoot() {
  return new DatabaseReferenceInternal(database_,
                                       MakeUnique<FIRDatabaseReferencePointer>(impl().root));
}

DatabaseReferenceInternal* DatabaseReferenceInternal::Child(const char* path) {
  return new DatabaseReferenceInternal(
      database_, MakeUnique<FIRDatabaseReferencePointer>([impl() child:@(path)]));
}

DatabaseReferenceInternal* DatabaseReferenceInternal::PushChild() {
  return new DatabaseReferenceInternal(
      database_, MakeUnique<FIRDatabaseReferencePointer>([impl() childByAutoId]));
}

Future<void> DatabaseReferenceInternal::RemoveValue() {
  SafeFutureHandle<void> handle = ref_future()->SafeAlloc<void>(kDatabaseReferenceFnRemoveValue);
  [impl() removeValueWithCompletionBlock:CreateCompletionBlock(handle, ref_future())];
  return MakeFuture(ref_future(), handle);
}

Future<void> DatabaseReferenceInternal::RemoveValueLastResult() {
  return
      static_cast<const Future<void>&>(ref_future()->LastResult(kDatabaseReferenceFnRemoveValue));
}

Future<DataSnapshot> DatabaseReferenceInternal::RunTransaction(
    DoTransactionWithContext transaction_function, void* context, void (*delete_context)(void*),
    bool trigger_local_events) {
  // Copy the database_ pointer to a local variable to be captured and used in the following block
  DatabaseInternal* database = database_;
  FIRTransactionResult* _Nonnull (^transaction_block)(FIRMutableData* _Nonnull) =
      ^(FIRMutableData* mutable_data_impl) {
        MutableData mutable_data(new MutableDataInternal(
            database, MakeUnique<FIRMutableDataPointer>(mutable_data_impl)));
        TransactionResult result = transaction_function(&mutable_data, context);
        if (result == kTransactionResultSuccess) {
          return [FIRTransactionResult successWithValue:mutable_data_impl];
        } else {  // result == kTransactionResultAbort
          return [FIRTransactionResult abort];
        }
      };
  SafeFutureHandle<DataSnapshot> handle = ref_future()->SafeAlloc<DataSnapshot>(
      kDatabaseReferenceFnRunTransaction, DataSnapshot(nullptr));
  FutureCallbackData<DataSnapshot>* data =
      new FutureCallbackData<DataSnapshot>(handle, ref_future());
  data->database = database_;
  void (^completion_block)(NSError *_Nullable, BOOL, FIRDataSnapshot *_Nullable) =
      ^(NSError *error, BOOL was_committed, FIRDataSnapshot *snapshot) {
        Error error_code = NSErrorToErrorCode(error);
        const char* error_string = GetErrorMessage(error_code);
        if (error == nil) {
          if (was_committed) {
            // Succeeded.
            data->impl->CompleteWithResult(
                data->handle, error_code, error_string,
                DataSnapshot(new DataSnapshotInternal(
                    data->database, MakeUnique<FIRDataSnapshotPointer>(snapshot))));
          } else {
            // Aborted.
            data->impl->CompleteWithResult(
                data->handle, kErrorTransactionAbortedByUser,
                GetErrorMessage(kErrorTransactionAbortedByUser),
                DataSnapshot(new DataSnapshotInternal(
                    data->database, MakeUnique<FIRDataSnapshotPointer>(snapshot))));
          }
        } else {
          // Error occurred.
          data->impl->Complete(data->handle, error_code, error_string);
        }
        if (delete_context) {
          delete_context(context);
        }
        delete data;
      };
  [impl() runTransactionBlock:transaction_block andCompletionBlock:completion_block];
  return MakeFuture(ref_future(), handle);
}

Future<DataSnapshot> DatabaseReferenceInternal::RunTransactionLastResult() {
  return static_cast<const Future<DataSnapshot>&>(
      ref_future()->LastResult(kDatabaseReferenceFnRunTransaction));
}

Future<void> DatabaseReferenceInternal::SetPriority(const Variant& priority) {
  SafeFutureHandle<void> handle = ref_future()->SafeAlloc<void>(kDatabaseReferenceFnSetPriority);
  if (SetValueAndPriorityLastResult().status() == kFutureStatusPending) {
    ref_future()->Complete(
        handle, kErrorConflictingOperationInProgress, kErrorMsgConflictSetPriority);
  } else if (!IsValidPriority(priority)) {
    ref_future()->Complete(handle, kErrorInvalidVariantType, kErrorMsgInvalidVariantForPriority);
  } else {
    [impl() setPriority:VariantToId(priority)
        withCompletionBlock:CreateCompletionBlock(handle, ref_future())];
  }
  return MakeFuture(ref_future(), handle);
}

Future<void> DatabaseReferenceInternal::SetPriorityLastResult() {
  return
      static_cast<const Future<void>&>(ref_future()->LastResult(kDatabaseReferenceFnSetPriority));
}

Future<void> DatabaseReferenceInternal::SetValue(const Variant& value) {
  SafeFutureHandle<void> handle = ref_future()->SafeAlloc<void>(kDatabaseReferenceFnSetValue);
  if (SetValueAndPriorityLastResult().status() == kFutureStatusPending) {
    ref_future()->Complete(handle, kErrorConflictingOperationInProgress, kErrorMsgConflictSetValue);
  } else {
    [impl() setValue:VariantToId(value)
        withCompletionBlock:CreateCompletionBlock(handle, ref_future())];
  }
  return MakeFuture(ref_future(), handle);
}

Future<void> DatabaseReferenceInternal::SetValueLastResult() {
  return static_cast<const Future<void>&>(ref_future()->LastResult(kDatabaseReferenceFnSetValue));
}

Future<void> DatabaseReferenceInternal::SetValueAndPriority(const Variant& value,
                                                            const Variant& priority) {
  SafeFutureHandle<void> handle =
      ref_future()->SafeAlloc<void>(kDatabaseReferenceFnSetValueAndPriority);
  if (SetValueLastResult().status() == kFutureStatusPending) {
    ref_future()->Complete(handle, kErrorConflictingOperationInProgress, kErrorMsgConflictSetValue);
  } else if (SetPriorityLastResult().status() == kFutureStatusPending) {
    ref_future()->Complete(
        handle, kErrorConflictingOperationInProgress, kErrorMsgConflictSetPriority);
  } else if (!IsValidPriority(priority)) {
    ref_future()->Complete(handle, kErrorInvalidVariantType, kErrorMsgInvalidVariantForPriority);
  } else {
    [impl() setValue:VariantToId(value)
                andPriority:VariantToId(priority)
        withCompletionBlock:CreateCompletionBlock(handle, ref_future())];
  }
  return MakeFuture(ref_future(), handle);
}

Future<void> DatabaseReferenceInternal::SetValueAndPriorityLastResult() {
  return static_cast<const Future<void>&>(
      ref_future()->LastResult(kDatabaseReferenceFnSetValueAndPriority));
}

Future<void> DatabaseReferenceInternal::UpdateChildren(const Variant& values) {
  SafeFutureHandle<void> handle = ref_future()->SafeAlloc<void>(kDatabaseReferenceFnUpdateChildren);
  id values_id = VariantToId(values);
  if (!values.is_map()) {
    ref_future()->Complete(
        handle, kErrorInvalidVariantType, kErrorMsgInvalidVariantForUpdateChildren);
  } else {
    id values_id = VariantToId(values);
    NSDictionary *values_dict = (NSDictionary *)values_id;
    [impl() updateChildValues:values_dict
          withCompletionBlock:CreateCompletionBlock(handle, ref_future())];
  }
  return MakeFuture(ref_future(), handle);
}

Future<void> DatabaseReferenceInternal::UpdateChildrenLastResult() {
  return static_cast<const Future<void>&>(
      ref_future()->LastResult(kDatabaseReferenceFnUpdateChildren));
}

std::string DatabaseReferenceInternal::GetUrl() const {
  return std::string([[impl() description] UTF8String]);
}

DisconnectionHandler* DatabaseReferenceInternal::OnDisconnect() {
  if (cached_disconnection_handler_ == nullptr) {
    cached_disconnection_handler_ = new DisconnectionHandler(new DisconnectionHandlerInternal(
        database_, MakeUnique<FIRDatabaseReferencePointer>(impl())));
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
