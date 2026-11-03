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

#ifndef FIREBASE_DATABASE_SRC_DESKTOP_DATABASE_DESKTOP_H_
#define FIREBASE_DATABASE_SRC_DESKTOP_DATABASE_DESKTOP_H_

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "app/src/cleanup_notifier.h"
#include "app/src/future_manager.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/logger.h"
#include "app/src/safe_reference.h"
#include "app/src/scheduler.h"
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

class DatabaseInternal;

// For constructing, copying or moving DatabaseReferences atomically.
extern Mutex g_database_reference_constructor_mutex;

class SingleValueListener : public ValueListener {
 public:
  SingleValueListener(DatabaseInternal* database, const QuerySpec& query_spec,
                      ReferenceCountedFutureImpl* future,
                      SafeFutureHandle<DataSnapshot> handle);
  SingleValueListener(const SingleValueListener&) = delete;
  SingleValueListener& operator=(const SingleValueListener&) = delete;
  SingleValueListener(SingleValueListener&&) = delete;
  SingleValueListener& operator=(SingleValueListener&&) = delete;

  // Unregister ourselves from the database.
  ~SingleValueListener() override;
  void OnValueChanged(const DataSnapshot& snapshot) override;
  void OnCancelled(const Error& error_code, const char* error_message) override;
  const QuerySpec& query_spec() const { return query_spec_; }

 private:
  firebase::internal::SafeReference<DatabaseInternal> database_ref_;
  QuerySpec query_spec_;
  ReferenceCountedFutureImpl* future_ = nullptr;
  SafeFutureHandle<DataSnapshot> handle_;
  Future<DataSnapshot> backing_future_;
  bool completed_ = false;
};

// This is the desktop implementation of Database.
class DatabaseInternal {
 public:
  explicit DatabaseInternal(App* app);

  DatabaseInternal(App* app, const char* url);

  DatabaseInternal(const DatabaseInternal&) = delete;
  DatabaseInternal& operator=(const DatabaseInternal&) = delete;

  ~DatabaseInternal();

  App* GetApp() const;

  DatabaseReference GetReference();

  DatabaseReference GetReference(const char* path);

  DatabaseReference GetReferenceFromUrl(const char* url);

  void GoOffline();

  void GoOnline();

  void PurgeOutstandingWrites();

  const char* GetSdkVersion() const;

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

  void AddEventRegistration(const QuerySpec& query_spec, void* listener_ptr,
                            EventRegistration* event_registration);

  void RemoveEventRegistration(const QuerySpec& query_spec, void* listener_ptr,
                               EventRegistration* event_registration);

  void RemoveActiveEventRegistrations(const QuerySpec& query_spec,
                                      void* listener_ptr);

  PushChildNameGenerator& name_generator() { return name_generator_; }

  typedef firebase::internal::SafeReference<DatabaseInternal> ThisRef;
  typedef firebase::internal::SafeReferenceLock<DatabaseInternal> ThisRefLock;

  ThisRef& this_ref() { return safe_this_; }

  // Call from transaction response to handle the response result.
  void HandleTransactionResponse(const connection::ResponsePtr& ptr);

  // The url that was passed to the constructor.
  const std::string& constructor_url() const { return constructor_url_; }

  Repo* repo() { return EnsureRepo(); }

  Repo::ThisRef EnsureRepoRef();

  Mutex* listener_mutex() { return &listener_mutex_; }

  Logger* logger() { return &logger_; }

 private:
  Repo* EnsureRepo();

  bool UnregisterValueListenerLocked(const QuerySpec& spec,
                                     ValueListener* listener);

  bool UnregisterChildListenerLocked(const QuerySpec& spec,
                                     ChildListener* listener);

  void RemoveActiveEventRegistrationsLocked(const QuerySpec& query_spec,
                                            void* listener_ptr);

  App* app_ = nullptr;

  ListenerCollection<ValueListener> value_listeners_by_query_;
  ListenerCollection<ChildListener> child_listeners_by_query_;

  std::map<std::pair<QuerySpec, ValueListener*>, ValueListenerCleanupData>
      cleanup_value_listener_lookup_;
  std::map<std::pair<QuerySpec, ChildListener*>, ChildListenerCleanupData>
      cleanup_child_listener_lookup_;

  std::map<QuerySpec, std::map<void*, std::vector<EventRegistration*>>>
      event_registration_lookup_;

  Mutex listener_mutex_;

  FutureManager future_manager_;

  CleanupNotifier cleanup_;

  // Needed to generate names that are guarenteed to be unique.
  PushChildNameGenerator name_generator_;

  std::string database_url_;

  // The url passed to the constructor (or "" if none was passed).
  // We keep it so that we can find the database in our cache.
  std::string constructor_url_;

  bool persistence_enabled_ = false;

  // The logger for this instance of the database.
  Logger logger_;

  ThisRef safe_this_;

  Mutex repo_mutex_;
  // The local copy of the repository, for offline support and local caching.
  std::unique_ptr<Repo> repo_;
};

}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_SRC_DESKTOP_DATABASE_DESKTOP_H_
