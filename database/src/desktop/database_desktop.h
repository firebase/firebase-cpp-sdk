// Copyright 2017 Google LLC
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

#ifndef FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_DATABASE_DESKTOP_H_
#define FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_DATABASE_DESKTOP_H_

#include <list>
#include <memory>
#include <string>

#include "app/src/cleanup_notifier.h"
#include "app/src/future_manager.h"
#include "app/src/include/firebase/app.h"
#include "app/src/logger.h"
#include "app/src/mutex.h"
#include "app/src/safe_reference.h"
#include "app/src/scheduler.h"
#include "app/memory/unique_ptr.h"
#include "database/src/common/listener.h"
#include "database/src/common/query_spec.h"
#include "database/src/desktop/connection/host_info.h"
#include "database/src/desktop/connection/persistent_connection.h"
#include "database/src/desktop/core/indexed_variant.h"
#include "database/src/desktop/core/repo.h"
#include "database/src/desktop/push_child_name_generator.h"
#include "database/src/desktop/query_desktop.h"
#include "database/src/desktop/transaction_data.h"
#include "database/src/desktop/util_desktop.h"
#include "database/src/include/firebase/database/database_reference.h"

namespace firebase {
namespace database {
namespace internal {

// For constructing, copying or moving DatabaseReferences atomically.
extern Mutex g_database_reference_constructor_mutex;

typedef int64_t WriteId;

class SingleValueListener : public ValueListener {
 public:
  SingleValueListener(DatabaseInternal* database, const QuerySpec& query_spec,
                      ReferenceCountedFutureImpl* future,
                      SafeFutureHandle<DataSnapshot> handle);
  // Unregister ourselves from the database.
  ~SingleValueListener() override;
  void OnValueChanged(const DataSnapshot& snapshot) override;
  void OnCancelled(const Error& error_code, const char* error_message) override;
  const QuerySpec& query_spec() { return query_spec_; }

 private:
  DatabaseInternal* database_;
  QuerySpec query_spec_;
  ReferenceCountedFutureImpl* future_;
  SafeFutureHandle<DataSnapshot> handle_;
};

// This is the desktop implementation of Database.
class DatabaseInternal {
 public:
  explicit DatabaseInternal(App* app);

  DatabaseInternal(App* app, const char* url);

  ~DatabaseInternal();

  App* GetApp();

  DatabaseReference GetReference();

  DatabaseReference GetReference(const char* path);

  DatabaseReference GetReferenceFromUrl(const char* url);

  void GoOffline();

  void GoOnline();

  void PurgeOutstandingWrites();

  const char* GetSdkVersion();

  void SetPersistenceEnabled(bool enabled);

  // Set the logging verbosity.
  void set_log_level(LogLevel log_level);

  // Get the logging verbosity.
  LogLevel log_level() const;

  FutureManager& future_manager() { return future_manager_; }

  // Whether this object was successfully initialized by the constructor.
  bool initialized() const { return app_ != nullptr; }

  const char* database_url() const { return database_url_.c_str(); }

  CleanupNotifier& cleanup() { return cleanup_; }

  bool RegisterValueListener(const QuerySpec& spec, ValueListener* listener,
                             ValueListenerCleanupData cleanup_data);

  bool UnregisterValueListener(const QuerySpec& spec, ValueListener* listener);

  void UnregisterAllValueListeners(const QuerySpec& spec);

  bool RegisterChildListener(const QuerySpec& spec, ChildListener* listener,
                             ChildListenerCleanupData cleanup_data);

  bool UnregisterChildListener(const QuerySpec& spec, ChildListener* listener);

  void UnregisterAllChildListeners(const QuerySpec& spec);

  PushChildNameGenerator& name_generator() { return name_generator_; }

  // Track a transient listener. If the database is deleted before the listener
  // finishes, it should discard its pointers.
  SingleValueListener** AddSingleValueListener(SingleValueListener* listener) {
    MutexLock lock(listener_mutex_);
    // If the listener is already being tracked, just return the existing
    // listener holder.
    for (SingleValueListener** listener_holder : single_value_listeners_) {
      if (*listener_holder == listener) {
        return listener_holder;
      }
    }
    // If the listener was not found, register create a new holder and return
    // it.
    SingleValueListener** holder = new SingleValueListener*(listener);
    single_value_listeners_.insert(holder);
    return holder;
  }

  // Finish tracking a transient listener. If the database is deleted before the
  // listener finishes, it should discard its pointers.
  void RemoveSingleValueListener(SingleValueListener* listener) {
    MutexLock lock(listener_mutex_);
    auto iter = std::find_if(
        single_value_listeners_.begin(), single_value_listeners_.end(),
        [listener](const SingleValueListener* const* listener_holder) {
          return *listener_holder == listener;
        });
    if (iter != single_value_listeners_.end()) {
      repo_->RemoveEventCallback(listener, listener->query_spec());
      delete *iter;
      single_value_listeners_.erase(iter);
    }
  }

  typedef firebase::internal::SafeReference<DatabaseInternal> ThisRef;
  typedef firebase::internal::SafeReferenceLock<DatabaseInternal> ThisRefLock;

  // Call from transaction response to handle the response result.
  void HandleTransactionResponse(const connection::ResponsePtr& ptr);

  // The url that was passed to the constructor.
  const std::string& constructor_url() const { return constructor_url_; }

  Repo* repo() { return repo_.get(); }

  Mutex* listener_mutex() { return &listener_mutex_; }

  Logger* logger() { return &logger_; }

 private:
  void EnsureRepo();

  App* app_;

  ListenerCollection<ValueListener> value_listeners_by_query_;
  ListenerCollection<ChildListener> child_listeners_by_query_;

  std::map<ValueListener*, ValueListenerCleanupData>
      cleanup_value_listener_lookup_;
  std::map<ChildListener*, ChildListenerCleanupData>
      cleanup_child_listener_lookup_;
  std::set<SingleValueListener**> single_value_listeners_;

  Mutex listener_mutex_;

  FutureManager future_manager_;

  CleanupNotifier cleanup_;

  // Needed to generate names that are guarenteed to be unique.
  PushChildNameGenerator name_generator_;

  std::string database_url_;

  // The url passed to the constructor (or "" if none was passed).
  // We keep it so that we can find the database in our cache.
  std::string constructor_url_;

  bool persistence_enabled_;

  // The logger for this instance of the database.
  Logger logger_;

  Mutex repo_mutex_;
  // The local copy of the repository, for offline support and local caching.
  UniquePtr<Repo> repo_;
};

}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_DATABASE_DESKTOP_H_
