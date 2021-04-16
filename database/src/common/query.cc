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

#include "database/src/common/query.h"

#include "app/src/assert.h"
#include "app/src/include/firebase/internal/platform.h"
#include "database/src/include/firebase/database.h"
#include "database/src/include/firebase/database/query.h"

// QueryInternal is defined in these 3 files, one implementation for each OS.
#if FIREBASE_PLATFORM_ANDROID
#include "database/src/android/database_android.h"
#include "database/src/android/query_android.h"
#elif FIREBASE_PLATFORM_IOS
#include "database/src/ios/database_ios.h"
#include "database/src/ios/query_ios.h"
#elif defined(FIREBASE_TARGET_DESKTOP)
#include "database/src/desktop/database_desktop.h"
#include "database/src/desktop/query_desktop.h"
#else
#include "database/src/stub/database_stub.h"
#include "database/src/stub/query_stub.h"
#endif  // FIREBASE_PLATFORM_ANDROID, FIREBASE_PLATFORM_IOS,
        // defined(FIREBASE_TARGET_DESKTOP)

#include "database/src/common/cleanup.h"

namespace firebase {
namespace database {

using internal::QueryInternal;

static Query GetInvalidQuery() { return Query(); }

typedef CleanupFn<Query, QueryInternal> CleanupFnQuery;

template <>
CleanupFnQuery::CreateInvalidObjectFn CleanupFnQuery::create_invalid_object =
    GetInvalidQuery;

Query::Query(QueryInternal* internal) : internal_(internal) {
  RegisterCleanup();
}

void Query::SetInternal(QueryInternal* internal) {
  UnregisterCleanup();
  if (internal_) delete internal_;
  internal_ = internal;
  RegisterCleanup();
}

void Query::RegisterCleanup() {
  if (internal_) CleanupFnQuery::Register(this, internal_);
}

void Query::UnregisterCleanup() {
  if (internal_) CleanupFnQuery::Unregister(this, internal_);
}

Query::Query(const Query& src)
    : internal_(src.internal_ ? new QueryInternal(*src.internal_) : nullptr) {
  RegisterCleanup();
}

Query& Query::operator=(const Query& src) {
  SetInternal(src.internal_ ? new QueryInternal(*src.internal_) : nullptr);
  return *this;
}

#if defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)
Query::Query(Query&& query) : internal_(query.internal_) {
  query.UnregisterCleanup();
  query.internal_ = nullptr;
  RegisterCleanup();
}

Query& Query::operator=(Query&& query) {
  QueryInternal* internal = query.internal_;
  query.UnregisterCleanup();
  query.internal_ = nullptr;
  SetInternal(internal);
  return *this;
}
#endif  // defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)

bool operator==(const Query& lhs, const Query& rhs) {
  if (!lhs.is_valid() && !rhs.is_valid()) return true;   // both invalid
  if (!lhs.is_valid() || !rhs.is_valid()) return false;  // only one invalid
  return lhs.internal_->query_spec() == rhs.internal_->query_spec();
}

Query::~Query() {
  UnregisterCleanup();
  if (internal_) delete internal_;
}

Future<DataSnapshot> Query::GetValue() {
  if (internal_)
    return internal_->GetValue();
  else
    return Future<DataSnapshot>();
}

Future<DataSnapshot> Query::GetValueLastResult() {
  if (internal_)
    return internal_->GetValueLastResult();
  else
    return Future<DataSnapshot>();
}

void Query::AddValueListener(ValueListener* listener) {
  if (internal_ && listener) internal_->AddValueListener(listener);
}

void Query::RemoveValueListener(ValueListener* listener) {
  // listener is allowed to be a nullptr. nullptr represents removing all
  // listeners at this location.
  if (internal_) internal_->RemoveValueListener(listener);
}

void Query::RemoveAllValueListeners() {
  if (internal_) internal_->RemoveAllValueListeners();
}

void Query::AddChildListener(ChildListener* listener) {
  if (internal_ && listener) internal_->AddChildListener(listener);
}

void Query::RemoveChildListener(ChildListener* listener) {
  // listener is allowed to be a nullptr. nullptr represents removing all
  // listeners at this location.
  if (internal_) internal_->RemoveChildListener(listener);
}

void Query::RemoveAllChildListeners() {
  if (internal_) internal_->RemoveAllChildListeners();
}

DatabaseReference Query::GetReference() const {
  return internal_ ? DatabaseReference(internal_->GetReference())
                   : DatabaseReference(nullptr);
}

void Query::SetKeepSynchronized(bool keep_sync) {
  if (internal_) internal_->SetKeepSynchronized(keep_sync);
}

Query Query::OrderByChild(const char* path) {
  return internal_ && path ? Query(internal_->OrderByChild(path))
                           : Query(nullptr);
}

Query Query::OrderByChild(const std::string& path) {
  return OrderByChild(path.c_str());
}

Query Query::OrderByKey() {
  return internal_ ? Query(internal_->OrderByKey()) : Query(nullptr);
}

Query Query::OrderByPriority() {
  return internal_ ? Query(internal_->OrderByPriority()) : Query(nullptr);
}

Query Query::OrderByValue() {
  return internal_ ? Query(internal_->OrderByValue()) : Query(nullptr);
}

Query Query::StartAt(Variant order_value) {
  return internal_ ? Query(internal_->StartAt(order_value)) : Query(nullptr);
}

Query Query::StartAt(Variant order_value, const char* child_key) {
  return internal_ && child_key
             ? Query(internal_->StartAt(order_value, child_key))
             : Query(nullptr);
}

Query Query::EndAt(Variant order_value) {
  return internal_ ? Query(internal_->EndAt(order_value)) : Query(nullptr);
}

Query Query::EndAt(Variant order_value, const char* child_key) {
  return internal_ && child_key
             ? Query(internal_->EndAt(order_value, child_key))
             : Query(nullptr);
}

Query Query::EqualTo(Variant order_value) {
  return internal_ ? Query(internal_->EqualTo(order_value)) : Query(nullptr);
}

Query Query::EqualTo(Variant order_value, const char* child_key) {
  return internal_ && child_key
             ? Query(internal_->EqualTo(order_value, child_key))
             : Query(nullptr);
}

Query Query::LimitToFirst(size_t limit) {
  return internal_ ? Query(internal_->LimitToFirst(limit)) : Query(nullptr);
}

Query Query::LimitToLast(size_t limit) {
  return internal_ ? Query(internal_->LimitToLast(limit)) : Query(nullptr);
}

bool Query::is_valid() const { return internal_ != nullptr; }

}  // namespace database
}  // namespace firebase
