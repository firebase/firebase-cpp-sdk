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

#ifndef FIREBASE_DATABASE_CLIENT_CPP_SRC_IOS_DATABASE_REFERENCE_IOS_H_
#define FIREBASE_DATABASE_CLIENT_CPP_SRC_IOS_DATABASE_REFERENCE_IOS_H_

#include <map>
#include <string>

#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/include/firebase/variant.h"
#include "app/src/reference_counted_future_impl.h"
#include "database/src/include/firebase/database/disconnection.h"
#include "database/src/include/firebase/database/query.h"
#include "database/src/include/firebase/database/transaction.h"
#include "database/src/ios/query_ios.h"

#ifdef __OBJC__
#import "FIRDatabase.h"
#endif  // __OBJC__

namespace firebase {
namespace database {
namespace internal {

// This defines the class FIRDatabaseReferencePointer, which is a C++-compatible
// wrapper around the FIRDatabaseReference Obj-C class.
OBJ_C_PTR_WRAPPER(FIRDatabaseReference);

#pragma clang assume_nonnull begin

// The iOS implementation of the DatabaseReference class, which represents a
// particular location in your Database and can be used for reading or writing
// data to that Database location.
class DatabaseReferenceInternal : public QueryInternal {
 public:
  explicit DatabaseReferenceInternal(
      DatabaseInternal* database,
      UniquePtr<FIRDatabaseReferencePointer> impl);

  virtual ~DatabaseReferenceInternal();

  DatabaseReferenceInternal(const DatabaseReferenceInternal& reference);

  DatabaseReferenceInternal& operator=(
      const DatabaseReferenceInternal& reference);

#if defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)
  DatabaseReferenceInternal(DatabaseReferenceInternal&& reference);

  DatabaseReferenceInternal& operator=(DatabaseReferenceInternal&& reference);
#endif  // defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)

  // Gets the database to which we refer.
  Database* GetDatabase() const;

  // Gets the string key of this database location.
  const char* GetKey() const;

  // Gets the string key of this database location. The pointer is only valid
  // while the DatabaseReference is in scope.
  std::string GetKeyString() const;

  // Returns true if we are the root of the database.
  bool IsRoot() const;

  // Gets the parent of this location, or get this location again if IsRoot().
  //
  // The returned pointer should be passed to a DatabaseReference for lifetime
  // management.
  DatabaseReferenceInternal* GetParent();

  // Gets the root of the database.
  //
  // The returned pointer should be passed to a DatabaseReference for lifetime
  // management.
  DatabaseReferenceInternal* GetRoot();

  // Gets a reference to a location relative to this one.
  //
  // The returned pointer should be passed to a DatabaseReference for lifetime
  // management.
  DatabaseReferenceInternal* Child(const char* path);

  // Automatically generates a child location, create a reference to it, and
  // returns that reference to it.
  //
  // The returned pointer should be passed to a DatabaseReference for lifetime
  // management.
  DatabaseReferenceInternal* PushChild();

  // Removes the value at this location from the database.
  Future<void> RemoveValue();

  // Gets the result of the most recent call to RemoveValue();
  Future<void> RemoveValueLastResult();

  // Run a user-supplied callback function, possibly multiple times, to perform
  // an atomic transaction on the database.
  Future<DataSnapshot> RunTransaction(
      DoTransactionWithContext transaction_function,
      void* _Nullable context,
      void (* _Nullable delete_context)(void* _Nullable),
      bool trigger_local_events = true);

  // Get the result of the most recent call to RunTransaction().
  Future<DataSnapshot> RunTransactionLastResult();

  // Sets the priority of this field, which controls its sort
  // order relative to its siblings.
  Future<void> SetPriority(const Variant& priority);

  // Gets the result of the most recent call to SetPriority().
  Future<void> SetPriorityLastResult();

  // Sets the data at this location to the given value.
  Future<void> SetValue(const Variant& value);

  // Gets the result of the most recent call to SetValue().
  Future<void> SetValueLastResult();

  // Sets both the data and priority of this location. See SetValue() and
  // SetPriority() for context on the parameters.
  Future<void> SetValueAndPriority(const Variant& value,
                                   const Variant& priority);

  // Get the result of the most recent call to SetValueAndPriority().
  Future<void> SetValueAndPriorityLastResult();

  // Updates the specified child keys to the given values.
  Future<void> UpdateChildren(const Variant& values);

  // Gets the result of the most recent call to either version of
  // UpdateChildren().
  Future<void> UpdateChildrenLastResult();

  // Get the full URL representation of this reference.
  std::string GetUrl() const;

  // Get the disconnect handler, which controls what actions the server will
  // perform to this location's data when this client disconnects.
  DisconnectionHandler* _Nullable OnDisconnect();

  // Manually disconnect Firebase Realtime Database from the server, and disable
  // automatic reconnection.
  static void GoOffline();

  // Manually reestablish connection to the Firebase Realtime Database server
  // and enable automatic reconnection.
  static void GoOnline();

 protected:
#ifdef __OBJC__
  FIRDatabaseReference* impl() const { return impl_->get(); }
#endif  // __OBJC__

 private:
  // Get the Future for the DatabaseReferenceInternal.
  ReferenceCountedFutureImpl* ref_future();

  // Object lifetime managed by Objective C ARC.
  UniquePtr<FIRDatabaseReferencePointer> impl_;

  DisconnectionHandler* _Nullable cached_disconnection_handler_;

  // The memory location of this member variable is used to look up our
  // ReferenceCountedFutureImpl. We can't use "this" because QueryInternal and
  // DatabaseReferenceInternal require two separate ReferenceCountedFutureImpl
  // instances, but have the same "this" pointer as one is a subclass of the
  // other.
  int future_api_id_;
};

#pragma clang assume_nonnull end

}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_CLIENT_CPP_SRC_IOS_DATABASE_REFERENCE_IOS_H_
