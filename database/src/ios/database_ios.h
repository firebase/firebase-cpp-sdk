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

extern Mutex g_database_reference_constructor_mutex;

OBJ_C_PTR_WRAPPER(FIRDatabase);
OBJ_C_PTR_WRAPPER(NSRecursiveLock);

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

  App* GetApp();
  DatabaseReference GetReference() const;
  DatabaseReference GetReference(const char* path) const;
  DatabaseReference GetReferenceFromUrl(const char* url) const;
  void GoOffline();
  void GoOnline();
  void PurgeOutstandingWrites();
  const char* GetSdkVersion();
  void SetPersistenceEnabled(bool enabled);
  void set_log_level(LogLevel log_level);
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

  void AddSingleValueListener(ValueListener* listener) {
    MutexLock lock(listener_mutex_);
    single_value_listeners_.insert(listener);
  }

  bool RemoveSingleValueListener(ValueListener* listener) {
    MutexLock lock(listener_mutex_);
    return single_value_listeners_.erase(listener) > 0;
  }

  FutureManager& future_manager() { return future_manager_; }
  bool initialized() const;
  CleanupNotifier& cleanup() { return cleanup_; }
  const std::string& constructor_url() const { return constructor_url_; }

#ifdef __OBJC__
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

  Logger logger_;
  App* app_;
  ThisRef this_ref_;
  std::unique_ptr<FIRDatabasePointer> impl_;
  std::unique_ptr<NSRecursiveLockPointer> query_lock_;
  Mutex listener_mutex_;
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
  std::string constructor_url_;
};

}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_SRC_IOS_DATABASE_IOS_H_
