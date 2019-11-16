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

#include "database/src/include/firebase/database/database_reference.h"

#include "app/src/assert.h"
#include "app/src/include/firebase/internal/platform.h"
#include "database/src/common/database_reference.h"

// DatabaseReferenceInternal is defined in these 3 files, one implementation for
// each OS.
#if FIREBASE_PLATFORM_ANDROID
#include "database/src/android/database_android.h"
#include "database/src/android/database_reference_android.h"
#elif FIREBASE_PLATFORM_IOS
#include "database/src/ios/database_ios.h"
#include "database/src/ios/database_reference_ios.h"
#elif defined(FIREBASE_TARGET_DESKTOP)
#include "database/src/desktop/database_desktop.h"
#include "database/src/desktop/database_reference_desktop.h"
#else
#include "database/src/stub/database_reference_stub.h"
#include "database/src/stub/database_stub.h"
#endif  // FIREBASE_PLATFORM_ANDROID, FIREBASE_PLATFORM_IOS,
        // defined(FIREBASE_TARGET_DESKTOP)

#include "database/src/common/cleanup.h"

namespace firebase {
namespace database {
namespace internal {

const char kErrorMsgConflictSetPriority[] =
    "You may not use SetPriority and SetValueAndPriority at the same time.";
const char kErrorMsgConflictSetValue[] =
    "You may not use SetValue and SetValueAndPriority at the same time.";
const char kErrorMsgInvalidVariantForPriority[] =
    "Invalid Variant type, expected only fundamental types (number, string).";
const char kErrorMsgInvalidVariantForUpdateChildren[] =
    "Invalid Variant type, expected a Map.";

}  // namespace internal

using internal::DatabaseReferenceInternal;

static DatabaseReference GetInvalidDatabaseReference() {
  return DatabaseReference();
}

typedef CleanupFn<DatabaseReference, DatabaseReferenceInternal>
    CleanupFnDatabaseReference;

template <>
CleanupFnDatabaseReference::CreateInvalidObjectFn
    CleanupFnDatabaseReference::create_invalid_object =
        GetInvalidDatabaseReference;

void DatabaseReference::SwitchCleanupRegistrationToDatabaseReference() {
  Query::UnregisterCleanup();
  if (internal_) CleanupFnDatabaseReference::Register(this, internal_);
}

void DatabaseReference::SwitchCleanupRegistrationBackToQuery() {
  if (internal_) CleanupFnDatabaseReference::Unregister(this, internal_);
  Query::RegisterCleanup();
}

DatabaseReference::DatabaseReference(DatabaseReferenceInternal* internal)
    : Query(internal), internal_(internal) {
  MutexLock lock(internal::g_database_reference_constructor_mutex);

  SwitchCleanupRegistrationToDatabaseReference();
}

DatabaseReference::DatabaseReference(const DatabaseReference& reference)
    : Query(), internal_(nullptr) {
  MutexLock lock(internal::g_database_reference_constructor_mutex);

  internal_ = reference.internal_
                  ? new DatabaseReferenceInternal(*reference.internal_)
                  : nullptr;
  Query::SetInternal(internal_);
  SwitchCleanupRegistrationToDatabaseReference();
}

DatabaseReference& DatabaseReference::operator=(
    const DatabaseReference& reference) {
  MutexLock lock(internal::g_database_reference_constructor_mutex);

  internal_ = reference.internal_
                  ? new DatabaseReferenceInternal(*reference.internal_)
                  : nullptr;
  Query::SetInternal(internal_);
  SwitchCleanupRegistrationToDatabaseReference();

  return *this;
}

#if defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)
DatabaseReference::DatabaseReference(DatabaseReference&& reference)
    : Query(), internal_(reference.internal_) {
  MutexLock lock(internal::g_database_reference_constructor_mutex);

  reference.internal_ = nullptr;
  Query::operator=(std::move(reference));
  SwitchCleanupRegistrationToDatabaseReference();
}

DatabaseReference& DatabaseReference::operator=(DatabaseReference&& reference) {
  MutexLock lock(internal::g_database_reference_constructor_mutex);

  internal_ = reference.internal_;
  reference.internal_ = nullptr;
  Query::operator=(std::move(reference));
  SwitchCleanupRegistrationToDatabaseReference();

  return *this;
}
#endif  // defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)

DatabaseReference::~DatabaseReference() {
  SwitchCleanupRegistrationBackToQuery();
  internal_ = nullptr;
}

bool operator==(const DatabaseReference& lhs, const DatabaseReference& rhs) {
  return lhs.url() == rhs.url();
}

Database* DatabaseReference::database() const {
  return internal_ ? internal_->GetDatabase() : nullptr;
}

const char* DatabaseReference::key() const {
  return internal_ ? internal_->GetKey() : nullptr;
}

std::string DatabaseReference::key_string() const {
  return internal_ ? internal_->GetKeyString() : std::string();
}

bool DatabaseReference::is_root() const {
  return internal_ ? internal_->IsRoot() : false;
}

bool DatabaseReference::is_valid() const { return internal_ != nullptr; }

DatabaseReference DatabaseReference::GetParent() const {
  return internal_ ? DatabaseReference(internal_->GetParent())
                   : DatabaseReference(nullptr);
}

DatabaseReference DatabaseReference::GetRoot() const {
  return internal_ ? DatabaseReference(internal_->GetRoot())
                   : DatabaseReference(nullptr);
}

DatabaseReference DatabaseReference::Child(const char* path) const {
  return internal_ && path ? DatabaseReference(internal_->Child(path))
                           : DatabaseReference(nullptr);
}

DatabaseReference DatabaseReference::Child(const std::string& path) const {
  return Child(path.c_str());
}

DatabaseReference DatabaseReference::PushChild() const {
  return internal_ ? DatabaseReference(internal_->PushChild())
                   : DatabaseReference(nullptr);
}

Future<void> DatabaseReference::RemoveValue() {
  return internal_ ? internal_->RemoveValue() : Future<void>();
}

Future<void> DatabaseReference::RemoveValueLastResult() {
  return internal_ ? internal_->RemoveValueLastResult() : Future<void>();
}

Future<DataSnapshot> DatabaseReference::RunTransaction(
    DoTransactionWithContext transaction_function, void* context,
    bool trigger_local_events) {
  return internal_ ? internal_->RunTransaction(transaction_function, context,
                                               nullptr, trigger_local_events)
                   : Future<DataSnapshot>();
}

#if defined(FIREBASE_USE_STD_FUNCTION)
static TransactionResult CallStdFunction(MutableData* data,
                                         void* function_void) {
  if (function_void) {
    DoTransactionFunction* function =
        reinterpret_cast<DoTransactionFunction*>(function_void);
    return (*function)(data);
  } else {
    return kTransactionResultAbort;
  }
}

static void DeleteStdFunction(void* function_void) {
  if (function_void) {
    DoTransactionFunction* function =
        reinterpret_cast<DoTransactionFunction*>(function_void);
    delete function;
  }
}

Future<DataSnapshot> DatabaseReference::RunTransaction(
    DoTransactionFunction transaction_function, bool trigger_local_events) {
  if (!internal_) return Future<DataSnapshot>();
  void* function_void = reinterpret_cast<void*>(
      new DoTransactionFunction(transaction_function));  // NOLINT
  return internal_->RunTransaction(CallStdFunction, function_void,
                                   DeleteStdFunction, trigger_local_events);
}
#else
TransactionResult CallFunctionPtrWithNoArgs(MutableData* data, void* fn_void) {
  DoTransaction fn = reinterpret_cast<DoTransaction>(fn_void);
  return fn(data);
}

Future<DataSnapshot> DatabaseReference::RunTransaction(
    DoTransaction transaction_function, bool trigger_local_events) {
  return internal_ ? internal_->RunTransaction(
                         CallFunctionPtrWithNoArgs,
                         reinterpret_cast<void*>(transaction_function), nullptr,
                         trigger_local_events)
                   : Future<DataSnapshot>();
}
#endif  // defined(FIREBASE_USE_STD_FUNCTION)

Future<DataSnapshot> DatabaseReference::RunTransactionLastResult() {
  return internal_ ? internal_->RunTransactionLastResult()
                   : Future<DataSnapshot>();
}

Future<void> DatabaseReference::SetPriority(Variant priority) {
  return internal_ ? internal_->SetPriority(priority) : Future<void>();
}

Future<void> DatabaseReference::SetPriorityLastResult() {
  return internal_ ? internal_->SetPriorityLastResult() : Future<void>();
}

Future<void> DatabaseReference::SetValue(Variant value) {
  return internal_ ? internal_->SetValue(value) : Future<void>();
}

Future<void> DatabaseReference::SetValueLastResult() {
  return internal_ ? internal_->SetValueLastResult() : Future<void>();
}

Future<void> DatabaseReference::SetValueAndPriority(Variant value,
                                                    Variant priority) {
  return internal_ ? internal_->SetValueAndPriority(value, priority)
                   : Future<void>();
}

Future<void> DatabaseReference::SetValueAndPriorityLastResult() {
  return internal_ ? internal_->SetValueAndPriorityLastResult()
                   : Future<void>();
}

Future<void> DatabaseReference::UpdateChildren(Variant values) {
  return internal_ ? internal_->UpdateChildren(values) : Future<void>();
}

Future<void> DatabaseReference::UpdateChildrenLastResult() {
  return internal_ ? internal_->UpdateChildrenLastResult() : Future<void>();
}

std::string DatabaseReference::url() const {
  return internal_ ? internal_->GetUrl() : std::string();
}

DisconnectionHandler* DatabaseReference::OnDisconnect() {
  return internal_ ? internal_->OnDisconnect() : nullptr;
}

void DatabaseReference::GoOffline() {
  if (internal_) internal_->GoOffline();
}

void DatabaseReference::GoOnline() {
  if (internal_) internal_->GoOnline();
}

}  // namespace database
}  // namespace firebase
