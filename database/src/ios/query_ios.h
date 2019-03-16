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

#ifndef FIREBASE_DATABASE_CLIENT_CPP_SRC_IOS_QUERY_IOS_H_
#define FIREBASE_DATABASE_CLIENT_CPP_SRC_IOS_QUERY_IOS_H_

#include "app/memory/unique_ptr.h"
#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/reference_counted_future_impl.h"
#include "app/src/util_ios.h"
#include "database/src/common/query_spec.h"
#include "database/src/include/firebase/database.h"
#include "database/src/include/firebase/database/listener.h"

#ifdef __OBJC__
#import "FIRDatabase.h"
#endif  // __OBJC__

namespace firebase {
class ReferenceCountedFutureImpl;
namespace database {
namespace internal {

// This defines the class FIRDatabaseQueryPointer, which is a C++-compatible
// wrapper around the FIRDatabaseQuery Obj-C class.
OBJ_C_PTR_WRAPPER(FIRDatabaseQuery);

/// The iOS implementation of the Query class, used for reading data.
class QueryInternal {
 public:
  QueryInternal(DatabaseInternal* database,
                UniquePtr<FIRDatabaseQueryPointer> query);

  QueryInternal(DatabaseInternal* database,
                UniquePtr<FIRDatabaseQueryPointer> query,
                const internal::QuerySpec& query_spec);

  QueryInternal(const QueryInternal& query);

  QueryInternal& operator=(const QueryInternal& query);

#if defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)
  QueryInternal(QueryInternal&& query);

  QueryInternal& operator=(QueryInternal&& query);
#endif  // defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)

  virtual ~QueryInternal();

  // Gets the value of the query for the given location a single time.
  Future<DataSnapshot> GetValue();
  // Gets the result of the most recent call to GetValue().
  Future<DataSnapshot> GetValueLastResult();

  // Adds a listener that will be called immediately and then again any time the
  // data changes.
  void AddValueListener(ValueListener* listener);

  // Removes a listener that was previously added with AddValueListener().
  void RemoveValueListener(ValueListener* listener);

  // Removes all value listeners that were added with AddValueListener().
  void RemoveAllValueListeners();

  // Adds a listener that will be called any time a child is added, removed,
  // modified, or reordered.
  void AddChildListener(ChildListener* listener);

  // Removes a listener that was previously added with AddChildListener().
  void RemoveChildListener(ChildListener* listener);

  // Removes all child listeners that were added by AddChildListener().
  void RemoveAllChildListeners();

  // Gets a DatabaseReference corresponding to the given location.
  //
  // The returned pointer should be passed to a DatabaseReference for lifetime
  // management.
  DatabaseReferenceInternal* GetReference();

  // Sets whether this location's data should be kept in sync even if there are
  // no active Listeners.
  void SetKeepSynchronized(bool keep_sync);

  // Gets a query in which child nodes are ordered by the values of the
  // specified path. Any previous OrderBy directive will be replaced in the
  // returned Query.
  //
  // The returned pointer should be passed to a Query for lifetime management.
  QueryInternal* OrderByChild(const char* path);
  // Gets a query in which child nodes are ordered by the values of the
  // specified path. Any previous OrderBy directive will be replaced in the
  // returned Query.
  //
  // The returned pointer should be passed to a Query for lifetime management.
  QueryInternal* OrderByKey();
  // Gets a query in which child nodes are ordered by their priority.  Any
  // previous OrderBy directive will be replaced in the returned Query.
  //
  // The returned pointer should be passed to a Query for lifetime management.
  QueryInternal* OrderByPriority();
  // Create a query in which nodes are ordered by their value.
  //
  // The returned pointer should be passed to a Query for lifetime management.
  QueryInternal* OrderByValue();

  // Get a Query constrained to nodes with the given sort value or higher.
  //
  // The returned pointer should be passed to a Query for lifetime management.
  QueryInternal* StartAt(Variant order_value);
  // Get a Query constrained to nodes with the given sort value or higher, and
  // the given key or higher.
  //
  // The returned pointer should be passed to a Query for lifetime management.
  QueryInternal* StartAt(Variant order_value, const char* child_key);

  // Get a Query constrained to nodes with the given sort value or lower.
  //
  // The returned pointer should be passed to a Query for lifetime management.
  QueryInternal* EndAt(Variant order_value);
  // Get a Query constrained to nodes with the given sort value or lower, and
  // the given key or lower.
  //
  // The returned pointer should be passed to a Query for lifetime management.
  QueryInternal* EndAt(Variant order_value, const char* child_key);

  // Get a Query constrained to nodes with the exact given sort value.
  //
  // The returned pointer should be passed to a Query for lifetime management.
  QueryInternal* EqualTo(Variant order_value);
  // Get a Query constrained to nodes with the exact given sort value, and the
  // exact given key.
  //
  // The returned pointer should be passed to a Query for lifetime management.
  QueryInternal* EqualTo(Variant order_value, const char* child_key);

  // Gets a Query limited to only the first results.
  //
  // The returned pointer should be passed to a Query for lifetime management.
  QueryInternal* LimitToFirst(size_t limit);
  // Gets a Query limited to only the last results.
  //
  // The returned pointer should be passed to a Query for lifetime management.
  QueryInternal* LimitToLast(size_t limit);

  const internal::QuerySpec& query_spec() const { return query_spec_; }

  DatabaseInternal* database_internal() const { return database_; }

 protected:
#ifdef __OBJC__
  FIRDatabaseQuery* _Nonnull impl() const { return impl_->ptr; }
#endif  // __OBJC__

  internal::QuerySpec query_spec_;

  DatabaseInternal* database_;

 private:
  // Get the Future for the QueryInternal.
  ReferenceCountedFutureImpl* query_future();

  // Object lifetime managed by Objective C ARC.
  UniquePtr<FIRDatabaseQueryPointer> impl_;

  // The memory location of this member variable is used to look up our
  // ReferenceCountedFutureImpl. We can't use "this" because QueryInternal and
  // DatabaseReferenceInternal require two separate ReferenceCountedFutureImpl
  // instances, but have the same "this" pointer as one is a subclass of the
  // other.
  int future_api_id_;
};

#ifdef __OBJC__
struct ValueListenerCleanupData {
  FIRDatabaseHandle observer_handle;
};

struct ChildListenerCleanupData {
  FIRDatabaseHandle child_added_handle;
  FIRDatabaseHandle child_changed_handle;
  FIRDatabaseHandle child_moved_handle;
  FIRDatabaseHandle child_removed_handle;
};
#else  // !__OBJC__
struct ChildListenerCleanupData;
struct ValueListenerCleanupData;

#endif  // __OBJC__

// Used by Query::GetValue().
class SingleValueListener : public ValueListener {
 public:
  SingleValueListener(DatabaseInternal* database,
                      ReferenceCountedFutureImpl* future,
                      const SafeFutureHandle<DataSnapshot>& handle);
  // Unregister ourselves from the database.
  virtual ~SingleValueListener();
  virtual void OnValueChanged(const DataSnapshot& snapshot);
  virtual void OnCancelled(const Error& error_code, const char* error_message);

 private:
  DatabaseInternal* database_;
  ReferenceCountedFutureImpl* future_;
  SafeFutureHandle<DataSnapshot> handle_;
};

}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_CLIENT_CPP_SRC_IOS_QUERY_IOS_H_
