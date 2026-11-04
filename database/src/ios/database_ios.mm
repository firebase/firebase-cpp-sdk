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

#include "database/src/ios/database_ios.h"

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "app/src/app_common.h"
#include "app/src/app_ios.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/future.h"
#include "app/src/reference_counted_future_impl.h"
#include "app/src/util.h"
#include "database/src/include/firebase/database/database_reference.h"
#include "database/src/ios/database_reference_ios.h"

namespace firebase {
namespace database {
namespace internal {

Mutex g_database_reference_constructor_mutex;  // NOLINT

DatabaseInternal::DatabaseInternal(App* app)
    : logger_(app ? app_common::FindAppLoggerByName(app->name()) : nullptr),
      app_(app),
      this_ref_(this),
      query_lock_(std::make_unique<NSRecursiveLockPointer>(
          [[NSRecursiveLock alloc] init])) {
  if (!app_) {
    impl_ = std::make_unique<FIRDatabasePointer>(nil);
    return;
  }
  @try {
    impl_ = std::make_unique<FIRDatabasePointer>(
        [FIRDatabase databaseForApp:app_->GetPlatformApp()]);
  } @catch (NSException* exception) {
    logger_.LogError(
        "%s",
        [[NSString stringWithFormat:@"Database::GetInstance(): %@", exception]
            UTF8String]);
    impl_ = std::make_unique<FIRDatabasePointer>(nil);
  }
}

DatabaseInternal::DatabaseInternal(App* app, const char* url)
    : logger_(app ? app_common::FindAppLoggerByName(app->name()) : nullptr),
      app_(app),
      this_ref_(this),
      query_lock_(std::make_unique<NSRecursiveLockPointer>(
          [[NSRecursiveLock alloc] init])),
      constructor_url_(url ? url : "") {
  if (!app_ || !url) {
    impl_ = std::make_unique<FIRDatabasePointer>(nil);
    return;
  }
  @try {
    NSString* url_str = @(url);
    impl_ = std::make_unique<FIRDatabasePointer>(
        url_str ? [FIRDatabase databaseForApp:app_->GetPlatformApp()
                                          URL:url_str]
                : nil);
  } @catch (NSException* exception) {
    logger_.LogError(
        "%s",
        [[NSString stringWithFormat:@"Database::GetInstance(%s): %@",
                                    url ? url : "(null)", exception]
            UTF8String]);
    impl_ = std::make_unique<FIRDatabasePointer>(nil);
  }
}

DatabaseInternal::~DatabaseInternal() {
  this_ref_.ClearReference();
  cleanup_.CleanupAll();
  std::vector<FIRCPPDatabaseQueryCallbackStatePointer> callbacks_to_remove;
  std::set<ValueListener*> single_listeners_to_delete;
  {
    MutexLock lock(listener_mutex_);
    callbacks_to_remove.reserve(cleanup_value_listener_lookup_.size() +
                                cleanup_child_listener_lookup_.size());
    for (auto& [k, v] : cleanup_value_listener_lookup_) {
      callbacks_to_remove.push_back(std::move(v));
    }
    cleanup_value_listener_lookup_.clear();
    for (auto& [k, v] : cleanup_child_listener_lookup_) {
      callbacks_to_remove.push_back(std::move(v));
    }
    cleanup_child_listener_lookup_.clear();
    single_listeners_to_delete = std::move(single_value_listeners_);
  }
  for (auto& callback_state : callbacks_to_remove) {
    [callback_state.get() removeAllObservers];
  }
  for (ValueListener* listener : single_listeners_to_delete) delete listener;
  impl_.reset();
}

App* DatabaseInternal::GetApp() { return app_; }

DatabaseReference DatabaseInternal::GetReference() const {
  if (!initialized()) return DatabaseReference();
  return DatabaseReference(new DatabaseReferenceInternal(
      const_cast<DatabaseInternal*>(this),
      std::make_unique<FIRDatabaseReferencePointer>([impl() reference])));
}

DatabaseReference DatabaseInternal::GetReference(const char* path) const {
  if (path == nullptr || !initialized()) return DatabaseReference();
  @try {
    NSString* path_str = @(path);
    if (!path_str) return DatabaseReference();
    return DatabaseReference(new DatabaseReferenceInternal(
        const_cast<DatabaseInternal*>(this),
        std::make_unique<FIRDatabaseReferencePointer>(
            [impl() referenceWithPath:path_str])));
  } @catch (NSException* e) {
    logger_.LogWarning(
        "Database::GetReference(): Invalid path specified: %s (%s)", path,
        e.reason.UTF8String ? e.reason.UTF8String : "Unknown exception");
    return DatabaseReference();
  }
}

DatabaseReference DatabaseInternal::GetReferenceFromUrl(const char* url) const {
  if (url == nullptr || !initialized()) return DatabaseReference();
  @try {
    NSString* url_str = @(url);
    if (!url_str) return DatabaseReference();
    return DatabaseReference(new DatabaseReferenceInternal(
        const_cast<DatabaseInternal*>(this),
        std::make_unique<FIRDatabaseReferencePointer>(
            [impl() referenceFromURL:url_str])));
  } @catch (NSException* e) {
    logger_.LogWarning(
        "Database::GetReferenceFromUrl(): Invalid URL '%s': %s", url,
        e.reason.UTF8String ? e.reason.UTF8String : "Unknown exception");
    return DatabaseReference();
  }
}

void DatabaseInternal::GoOffline() { [impl() goOffline]; }
void DatabaseInternal::GoOnline() { [impl() goOnline]; }
void DatabaseInternal::PurgeOutstandingWrites() {
  [impl() purgeOutstandingWrites];
}

const char* DatabaseInternal::GetSdkVersion() {
  static const std::string* const kSdkVersion =
      new std::string([[FIRDatabase sdkVersion] UTF8String]);
  return kSdkVersion->c_str();
}

void DatabaseInternal::SetPersistenceEnabled(bool enabled) {
  @try {
    impl().persistenceEnabled = enabled;
  } @catch (NSException* e) {
    logger_.LogError(
        "SetPersistenceEnabled error: %s",
        e.reason.UTF8String ? e.reason.UTF8String : "Unknown exception");
  }
}

void DatabaseInternal::set_log_level(LogLevel log_level) {
  [FIRDatabase setLoggingEnabled:(log_level == kLogLevelVerbose ||
                                  log_level == kLogLevelDebug)
                                     ? YES
                                     : NO];
  logger_.SetLogLevel(log_level);
}

LogLevel DatabaseInternal::log_level() const { return logger_.GetLogLevel(); }

template <typename ListenerT>
static bool RegisterListenerHelper(
    Mutex* mutex, ListenerCollection<ListenerT>* collection,
    std::map<std::pair<internal::QuerySpec, ListenerT*>,
             FIRCPPDatabaseQueryCallbackStatePointer>* lookup,
    const internal::QuerySpec& spec, ListenerT* listener,
    FIRCPPDatabaseQueryCallbackState* callback_state) {
  MutexLock lock(*mutex);
  if (!collection->Register(spec, listener)) return false;
  lookup->try_emplace(std::make_pair(spec, listener),
                      FIRCPPDatabaseQueryCallbackStatePointer(callback_state));
  return true;
}

template <typename ListenerT>
static bool UnregisterListenerHelper(
    Mutex* mutex, ListenerCollection<ListenerT>* collection,
    std::map<std::pair<internal::QuerySpec, ListenerT*>,
             FIRCPPDatabaseQueryCallbackStatePointer>* lookup,
    const internal::QuerySpec& spec, ListenerT* listener) {
  FIRCPPDatabaseQueryCallbackStatePointer callback_to_remove(nil);
  bool unregistered = false;
  {
    MutexLock lock(*mutex);
    if (collection->Unregister(spec, listener)) {
      unregistered = true;
      auto found = lookup->find(std::make_pair(spec, listener));
      if (found != lookup->end()) {
        callback_to_remove = std::move(found->second);
        lookup->erase(found);
      }
    }
  }
  if (callback_to_remove.get() != nil) {
    [callback_to_remove.get() removeAllObservers];
  }
  return unregistered;
}

template <typename ListenerT>
static void UnregisterAllListenersHelper(
    Mutex* mutex, ListenerCollection<ListenerT>* collection,
    std::map<std::pair<internal::QuerySpec, ListenerT*>,
             FIRCPPDatabaseQueryCallbackStatePointer>* lookup,
    const internal::QuerySpec& spec) {
  std::vector<FIRCPPDatabaseQueryCallbackStatePointer> callbacks_to_remove;
  {
    MutexLock lock(*mutex);
    std::vector<ListenerT*> listeners;
    if (collection->Get(spec, &listeners)) {
      callbacks_to_remove.reserve(listeners.size());
      for (ListenerT* listener : listeners) {
        if (collection->Unregister(spec, listener)) {
          auto found = lookup->find(std::make_pair(spec, listener));
          if (found != lookup->end()) {
            callbacks_to_remove.push_back(std::move(found->second));
            lookup->erase(found);
          }
        }
      }
    }
  }
  for (auto& callback_state : callbacks_to_remove) {
    [callback_state.get() removeAllObservers];
  }
}

bool DatabaseInternal::RegisterValueListener(
    const internal::QuerySpec& spec, ValueListener* listener,
    FIRCPPDatabaseQueryCallbackState* callback_state) {
  return RegisterListenerHelper(
      &listener_mutex_, &value_listeners_by_query_,
      &cleanup_value_listener_lookup_, spec, listener, callback_state);
}

bool DatabaseInternal::UnregisterValueListener(
    const internal::QuerySpec& spec, ValueListener* listener,
    [[maybe_unused]] FIRDatabaseQuery* query_impl) {
  return UnregisterListenerHelper(&listener_mutex_, &value_listeners_by_query_,
                                  &cleanup_value_listener_lookup_, spec,
                                  listener);
}

void DatabaseInternal::UnregisterAllValueListeners(
    const internal::QuerySpec& spec,
    [[maybe_unused]] FIRDatabaseQuery* query_impl) {
  UnregisterAllListenersHelper(&listener_mutex_, &value_listeners_by_query_,
                               &cleanup_value_listener_lookup_, spec);
}

bool DatabaseInternal::RegisterChildListener(
    const internal::QuerySpec& spec, ChildListener* listener,
    FIRCPPDatabaseQueryCallbackState* _Nonnull callback_state) {
  return RegisterListenerHelper(
      &listener_mutex_, &child_listeners_by_query_,
      &cleanup_child_listener_lookup_, spec, listener, callback_state);
}

bool DatabaseInternal::UnregisterChildListener(
    const internal::QuerySpec& spec, ChildListener* listener,
    [[maybe_unused]] FIRDatabaseQuery* query_impl) {
  return UnregisterListenerHelper(&listener_mutex_, &child_listeners_by_query_,
                                  &cleanup_child_listener_lookup_, spec,
                                  listener);
}

void DatabaseInternal::UnregisterAllChildListeners(
    const internal::QuerySpec& spec,
    [[maybe_unused]] FIRDatabaseQuery* query_impl) {
  UnregisterAllListenersHelper(&listener_mutex_, &child_listeners_by_query_,
                               &cleanup_child_listener_lookup_, spec);
}

bool DatabaseInternal::initialized() const { return impl() != nullptr; }

}  // namespace internal
}  // namespace database
}  // namespace firebase
