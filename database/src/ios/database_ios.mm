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

// Defined here for iOS builds and referenced by
// database/src/common/database_reference.cc to synchronize DatabaseReference
// construction, copy/move, and destruction across threads.
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
  // Collect pending callback states and transient listeners under
  // listener_mutex_, then release the mutex before detaching FIRDatabaseQuery
  // observers or deleting listeners to avoid lock-order inversion.
  std::vector<FIRCPPDatabaseQueryCallbackStatePointer> callbacks_to_remove;
  std::set<ValueListener*> single_listeners_to_delete;
  {
    MutexLock lock(listener_mutex_);
    callbacks_to_remove.reserve(cleanup_value_listener_lookup_.size() +
                                cleanup_child_listener_lookup_.size());
    for (auto& kv : cleanup_value_listener_lookup_) {
      callbacks_to_remove.push_back(std::move(kv.second));
    }
    cleanup_value_listener_lookup_.clear();
    for (auto& kv : cleanup_child_listener_lookup_) {
      callbacks_to_remove.push_back(std::move(kv.second));
    }
    cleanup_child_listener_lookup_.clear();
    single_listeners_to_delete = std::move(single_value_listeners_);
    single_value_listeners_.clear();
  }
  for (auto& callback_state : callbacks_to_remove) {
    [callback_state.get() removeAllObservers];
  }
  for (ValueListener* listener : single_listeners_to_delete) {
    delete listener;
  }
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
  } @catch (NSException* exception) {
    const char* reason = exception.reason.UTF8String
                             ? exception.reason.UTF8String
                             : "Unknown exception";
    logger_.LogWarning(
        "Database::GetReference(): Invalid path specified: %s (%s)", path,
        reason);
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
  } @catch (NSException* exception) {
    const char* reason = exception.reason.UTF8String
                             ? exception.reason.UTF8String
                             : "Unknown exception";
    logger_.LogWarning("Database::GetReferenceFromUrl(): Invalid URL '%s': %s",
                       url, reason);
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
    const char* reason =
        e.reason.UTF8String ? e.reason.UTF8String : "Unknown exception";
    logger_.LogError("SetPersistenceEnabled error: %s", reason);
  }
}

void DatabaseInternal::set_log_level(LogLevel log_level) {
  // iOS FIRDatabase only supports logging or not logging. Since the default
  // logging level (Info) should be quiet except for notices that can be
  // resolved by the developer fixing their code, logging is enabled for only
  // kLogLevelVerbose & kLogLevelDebug.
  switch (log_level) {
    case kLogLevelVerbose:
      FIREBASE_CASE_FALLTHROUGH;
    case kLogLevelDebug:
      [FIRDatabase setLoggingEnabled:YES];
      break;
    case kLogLevelInfo:
      FIREBASE_CASE_FALLTHROUGH;
    case kLogLevelWarning:
      FIREBASE_CASE_FALLTHROUGH;
    case kLogLevelError:
      FIREBASE_CASE_FALLTHROUGH;
    case kLogLevelAssert:
      [FIRDatabase setLoggingEnabled:NO];
      break;
  }
  logger_.SetLogLevel(log_level);
}

LogLevel DatabaseInternal::log_level() const { return logger_.GetLogLevel(); }

bool DatabaseInternal::RegisterValueListener(
    const internal::QuerySpec& spec, ValueListener* listener,
    FIRCPPDatabaseQueryCallbackState* callback_state) {
  MutexLock lock(listener_mutex_);
  if (value_listeners_by_query_.Register(spec, listener)) {
    auto key = std::make_pair(spec, listener);
    auto found = cleanup_value_listener_lookup_.find(key);
    if (found == cleanup_value_listener_lookup_.end()) {
      cleanup_value_listener_lookup_.insert(std::make_pair(
          key, FIRCPPDatabaseQueryCallbackStatePointer(callback_state)));
    }
    return true;
  }
  return false;
}

bool DatabaseInternal::UnregisterValueListener(
    const internal::QuerySpec& spec, ValueListener* listener,
    [[maybe_unused]] FIRDatabaseQuery* query_impl) {
  FIRCPPDatabaseQueryCallbackStatePointer callback_to_remove(nil);
  bool unregistered = false;
  {
    MutexLock lock(listener_mutex_);
    if (value_listeners_by_query_.Unregister(spec, listener)) {
      unregistered = true;
      auto found =
          cleanup_value_listener_lookup_.find(std::make_pair(spec, listener));
      if (found != cleanup_value_listener_lookup_.end()) {
        callback_to_remove = std::move(found->second);
        cleanup_value_listener_lookup_.erase(found);
      }
    }
  }
  if (callback_to_remove.get() != nil) {
    [callback_to_remove.get() removeAllObservers];
  }
  return unregistered;
}

void DatabaseInternal::UnregisterAllValueListeners(
    const internal::QuerySpec& spec,
    [[maybe_unused]] FIRDatabaseQuery* query_impl) {
  std::vector<FIRCPPDatabaseQueryCallbackStatePointer> callbacks_to_remove;
  {
    MutexLock lock(listener_mutex_);
    std::vector<ValueListener*> listeners;
    if (value_listeners_by_query_.Get(spec, &listeners)) {
      callbacks_to_remove.reserve(listeners.size());
      for (ValueListener* listener : listeners) {
        if (value_listeners_by_query_.Unregister(spec, listener)) {
          auto found = cleanup_value_listener_lookup_.find(
              std::make_pair(spec, listener));
          if (found != cleanup_value_listener_lookup_.end()) {
            callbacks_to_remove.push_back(std::move(found->second));
            cleanup_value_listener_lookup_.erase(found);
          }
        }
      }
    }
  }
  for (auto& callback_state : callbacks_to_remove) {
    [callback_state.get() removeAllObservers];
  }
}

bool DatabaseInternal::RegisterChildListener(
    const internal::QuerySpec& spec, ChildListener* listener,
    FIRCPPDatabaseQueryCallbackState* _Nonnull callback_state) {
  MutexLock lock(listener_mutex_);
  if (child_listeners_by_query_.Register(spec, listener)) {
    auto key = std::make_pair(spec, listener);
    auto found = cleanup_child_listener_lookup_.find(key);
    if (found == cleanup_child_listener_lookup_.end()) {
      cleanup_child_listener_lookup_.insert(std::make_pair(
          key, FIRCPPDatabaseQueryCallbackStatePointer(callback_state)));
    }
    return true;
  }
  return false;
}

bool DatabaseInternal::UnregisterChildListener(
    const internal::QuerySpec& spec, ChildListener* listener,
    [[maybe_unused]] FIRDatabaseQuery* query_impl) {
  FIRCPPDatabaseQueryCallbackStatePointer callback_to_remove(nil);
  bool unregistered = false;
  {
    MutexLock lock(listener_mutex_);
    if (child_listeners_by_query_.Unregister(spec, listener)) {
      unregistered = true;
      auto found =
          cleanup_child_listener_lookup_.find(std::make_pair(spec, listener));
      if (found != cleanup_child_listener_lookup_.end()) {
        callback_to_remove = std::move(found->second);
        cleanup_child_listener_lookup_.erase(found);
      }
    }
  }
  if (callback_to_remove.get() != nil) {
    [callback_to_remove.get() removeAllObservers];
  }
  return unregistered;
}

void DatabaseInternal::UnregisterAllChildListeners(
    const internal::QuerySpec& spec,
    [[maybe_unused]] FIRDatabaseQuery* query_impl) {
  std::vector<FIRCPPDatabaseQueryCallbackStatePointer> callbacks_to_remove;
  {
    MutexLock lock(listener_mutex_);
    std::vector<ChildListener*> listeners;
    if (child_listeners_by_query_.Get(spec, &listeners)) {
      callbacks_to_remove.reserve(listeners.size());
      for (ChildListener* listener : listeners) {
        if (child_listeners_by_query_.Unregister(spec, listener)) {
          auto found = cleanup_child_listener_lookup_.find(
              std::make_pair(spec, listener));
          if (found != cleanup_child_listener_lookup_.end()) {
            callbacks_to_remove.push_back(std::move(found->second));
            cleanup_child_listener_lookup_.erase(found);
          }
        }
      }
    }
  }
  for (auto& callback_state : callbacks_to_remove) {
    [callback_state.get() removeAllObservers];
  }
}

bool DatabaseInternal::initialized() const { return impl() != nullptr; }

}  // namespace internal
}  // namespace database
}  // namespace firebase
