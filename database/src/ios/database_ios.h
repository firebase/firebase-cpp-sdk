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

#ifndef FIREBASE_DATABASE_SRC_IOS_DATABASE_IOS_H_
#define FIREBASE_DATABASE_SRC_IOS_DATABASE_IOS_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include "app/src/cleanup_notifier.h"
#include "app/src/future_manager.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/logger.h"
#include "app/src/safe_reference.h"
#include "app/src/util_ios.h"
#include "database/src/common/listener.h"
#include "database/src/include/firebase/database/database_reference.h"
#include "database/src/ios/query_ios.h"

#ifdef __OBJC__
#import "FIRDatabase.h"
#endif  // __OBJC__

namespace firebase {
class ReferenceCountedFutureImpl;
namespace database {
namespace internal {

// Defined in database_ios.mm and referenced by
// database/src/common/database_reference.cc to synchronize DatabaseReference
// construction, copy/move, and destruction across threads.
extern Mutex g_database_reference_constructor_mutex;

// This defines the class FIRDatabasePointer, which is a C++-compatible wrapper
// around the FIRDatabase Obj-C class.
OBJ_C_PTR_WRAPPER(FIRDatabase);
// This defines the class firebase::database::internal::NSRecursiveLockPointer,
// which is a C++-compatible wrapper around the NSRecursiveLock Obj-C class,
// used by observer callbacks of FIRDatabaseQuery.
OBJ_C_PTR_WRAPPER(NSRecursiveLock);

// This is the iOS implementation of Database.
class DatabaseInternal {
 public:
  using ThisRef = firebase::internal::SafeReference<DatabaseInternal>;
  using ThisRefLock = firebase::internal::SafeReferenceLock<DatabaseInternal>;

  explicit DatabaseInternal(App* app);

  DatabaseInternal(App* app, const char* url);

  DatabaseInternal(const DatabaseInternal&) = delete;
  DatabaseInternal& operator=(const DatabaseInternal&) = delete;
  DatabaseInternal(DatabaseInternal&&) = delete;
  DatabaseInternal& operator=(DatabaseInternal&&) = delete;

  ~DatabaseInternal();

  // Get the firebase::App that this Database was created with.
  App* GetApp();

  // Get a DatabaseReference to the root of the database.
  DatabaseReference GetReference() const;

  // Get a DatabaseReference for the specified path.
  DatabaseReference GetReference(const char* path) const;

  // Get a DatabaseReference for the provided URL.
  DatabaseReference GetReferenceFromUrl(const char* url) const;

  // Shuts down the connection to the Firebase Realtime Database backend until
  // GoOnline() is called.
  void GoOffline();

  // Resumes the connection to the Firebase Realtime Database backend after a
  // previous GoOffline() call.
  void GoOnline();

  // Purge all pending writes to the Firebase Realtime Database server.
  void PurgeOutstandingWrites();

  // Gets the SDK version for the running library.
  const char* GetSdkVersion();

  // Sets whether pending write data will persist between application exits.
  void SetPersistenceEnabled(bool enabled);

  // Set the logging verbosity.
  // The iOS implementation only enables logging for kLogLevelVerbose &
  // kLogLevelDebug, logging is disabled in for all other levels.
  void set_log_level(LogLevel log_level);

  // Get the logging verbosity.
  LogLevel log_level() const;

#ifdef __OBJC__
  bool RegisterValueListener(const internal::QuerySpec& spec,
                             ValueListener* listener,
                             FIRCPPDatabaseQueryCallbackState* callback_state);

  bool UnregisterValueListener(const internal::QuerySpec& spec,
                               ValueListener* listener,
                               FIRDatabaseQuery* query_impl);

  void UnregisterAllValueListeners(const internal::QuerySpec& spec,
                                   FIRDatabaseQuery* query_impl);

  bool RegisterChildListener(const internal::QuerySpec& spec,
                             ChildListener* listener,
                             FIRCPPDatabaseQueryCallbackState* callback_state);

  bool UnregisterChildListener(const internal::QuerySpec& spec,
                               ChildListener* listener,
                               FIRDatabaseQuery* query_impl);

  void UnregisterAllChildListeners(const internal::QuerySpec& spec,
                                   FIRDatabaseQuery* query_impl);
#endif  // __OBJC__

  // Track a transient listener.
  void AddSingleValueListener(ValueListener* listener) {
    MutexLock lock(listener_mutex_);
    single_value_listeners_.insert(listener);
  }

  // Finish tracking a transient listener. Returns true if the listener was
  // found and erased (meaning the caller owns deleting it), or false if it was
  // already removed (e.g. during ~DatabaseInternal() teardown).
  bool RemoveSingleValueListener(ValueListener* listener) {
    MutexLock lock(listener_mutex_);
    auto it = single_value_listeners_.find(listener);
    if (it == single_value_listeners_.end()) return false;
    single_value_listeners_.erase(it);
    return true;
  }

  FutureManager& future_manager() { return future_manager_; }

  // Whether this object was successfully initialized by the constructor.
  bool initialized() const;

  // When this is deleted, it will clean up all DatabaseReferences,
  // DataSnapshots, and other such objects.
  CleanupNotifier& cleanup() { return cleanup_; }

  // The url that was passed to the constructor.
  const std::string& constructor_url() const { return constructor_url_; }

#ifdef __OBJC__
  // Guard access to C++ objects referenced by
  // FIRCPPDatabaseQueryCallbackStatePointer.
  NSRecursiveLock* query_lock() const {
    return query_lock_ ? query_lock_->get() : nil;
  }
#endif  // __OBJC__

  Logger* logger() { return &logger_; }

  ThisRef& this_ref() { return this_ref_; }

 private:
#ifdef __OBJC__
  FIRDatabase* impl() const { return impl_ ? impl_->get() : nil; }
#endif  // __OBJC__

  // Declared first so logger_ outlives cleanup_, future_manager_, and listener
  // lookup containers during destruction.
  Logger logger_;

  // The firebase::App that this Database was created with.
  App* app_;

  // Safe reference to this instance for asynchronous callbacks.
  ThisRef this_ref_;

  // Object lifetime managed by Objective C ARC.
  std::unique_ptr<FIRDatabasePointer> impl_;

  // Lock used to guard access to C++ objects referenced by FIRDatabaseQuery
  // callbacks.
  std::unique_ptr<NSRecursiveLockPointer> query_lock_;

  // For registering listeners.
  Mutex listener_mutex_;

  // Listeners indexed by QuerySpec.
  ListenerCollection<ValueListener> value_listeners_by_query_;
  ListenerCollection<ChildListener> child_listeners_by_query_;

  std::map<std::pair<internal::QuerySpec, ValueListener*>,
           FIRCPPDatabaseQueryCallbackStatePointer>
      cleanup_value_listener_lookup_;
  std::map<std::pair<internal::QuerySpec, ChildListener*>,
           FIRCPPDatabaseQueryCallbackStatePointer>
      cleanup_child_listener_lookup_;
  std::set<ValueListener*> single_value_listeners_;

  FutureManager future_manager_;

  CleanupNotifier cleanup_;

  // The url passed to the constructor (or "" if none was passed).
  // We keep it so that we can find the database in our cache.
  std::string constructor_url_;
};

}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_SRC_IOS_DATABASE_IOS_H_
