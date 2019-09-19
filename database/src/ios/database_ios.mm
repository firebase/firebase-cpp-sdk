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
    : app_(app), logger_(app_common::FindAppLoggerByName(app->name())) {
  @try {
    impl_.reset(new FIRDatabasePointer([FIRDatabase databaseForApp:app->GetPlatformApp()]));
    query_lock_.reset(new NSRecursiveLockPointer([[NSRecursiveLock alloc] init]));
  } @catch (NSException* exception) {
    logger_.LogError(
        [[NSString stringWithFormat:@"Database::GetInstance(): %@", exception] UTF8String]);
    impl_ = MakeUnique<FIRDatabasePointer>(nil);
  }
}

DatabaseInternal::DatabaseInternal(App* app, const char* url)
    : app_(app), constructor_url_(url), logger_(app_common::FindAppLoggerByName(app->name())) {
  @try {
    impl_.reset(new FIRDatabasePointer([FIRDatabase databaseForApp:app->GetPlatformApp()
                                                               URL:@(url)]));
    query_lock_.reset(new NSRecursiveLockPointer([[NSRecursiveLock alloc] init]));
  } @catch (NSException* exception) {
    logger_.LogError(
        [[NSString stringWithFormat:@"Database::GetInstance(%s): %@", url, exception] UTF8String]);
    impl_ = MakeUnique<FIRDatabasePointer>(nil);
  }
}

DatabaseInternal::~DatabaseInternal() {
  cleanup_.CleanupAll();
  // If there are any pending listeners, delete their pointers.
  {
    MutexLock lock(listener_mutex_);
    while (single_value_listeners_.begin() != single_value_listeners_.end()) {
      auto it = single_value_listeners_.begin();
      auto* listener = *it;
      single_value_listeners_.erase(it);
      delete listener;
    }
  }
  impl_ = MakeUnique<FIRDatabasePointer>(nil);
}

App* DatabaseInternal::GetApp() { return app_; }

DatabaseReference DatabaseInternal::GetReference() const {
  return DatabaseReference(
      new DatabaseReferenceInternal(const_cast<DatabaseInternal*>(this),
                                    MakeUnique<FIRDatabaseReferencePointer>([impl() reference])));
}

DatabaseReference DatabaseInternal::GetReference(const char* path) const {
  return DatabaseReference(new DatabaseReferenceInternal(
      const_cast<DatabaseInternal*>(this),
      MakeUnique<FIRDatabaseReferencePointer>([impl() referenceWithPath:@(path)])));
}

DatabaseReference DatabaseInternal::GetReferenceFromUrl(const char* url) const {
  return DatabaseReference(new DatabaseReferenceInternal(
      const_cast<DatabaseInternal*>(this),
      MakeUnique<FIRDatabaseReferencePointer>([impl() referenceFromURL:@(url)])));
}

void DatabaseInternal::GoOffline() { [impl() goOffline]; }

void DatabaseInternal::GoOnline() { [impl() goOnline]; }

void DatabaseInternal::PurgeOutstandingWrites() { [impl() purgeOutstandingWrites]; }

static std::string* g_sdk_version = nullptr;
const char* DatabaseInternal::GetSdkVersion() {
  if (g_sdk_version == nullptr) {
    g_sdk_version = new std::string([[FIRDatabase sdkVersion] UTF8String]);
  }
  return g_sdk_version->c_str();
}

void DatabaseInternal::SetPersistenceEnabled(bool enabled) {
  @try {
    impl().persistenceEnabled = enabled;
  } @catch (NSException* e) {
    LogError("SetPersistenceEnabled error: %s", e.reason.UTF8String);
  }
}

void DatabaseInternal::set_log_level(LogLevel log_level) {
  // iOS FIRDatabase only supports logging or not logging.  Since the default logging level (Info)
  // should be quiet except for notices that can be resolved by the developer fixing their code,
  // logging is enabled for only kLogLevelVerbose & kLogLevelDebug.
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

bool DatabaseInternal::RegisterValueListener(const internal::QuerySpec& spec,
                                             ValueListener* listener,
                                             FIRCPPDatabaseQueryCallbackState* callback_state) {
  MutexLock lock(listener_mutex_);
  if (value_listeners_by_query_.Register(spec, listener)) {
    auto found = cleanup_value_listener_lookup_.find(listener);
    if (found == cleanup_value_listener_lookup_.end()) {
      cleanup_value_listener_lookup_.insert(
          std::make_pair(listener, FIRCPPDatabaseQueryCallbackStatePointer(callback_state)));
    }
    return true;
  }
  return false;
}

bool DatabaseInternal::UnregisterValueListener(const internal::QuerySpec& spec,
                                               ValueListener* listener,
                                               FIRDatabaseQuery* query_impl) {
  MutexLock lock(listener_mutex_);
  if (value_listeners_by_query_.Unregister(spec, listener)) {
    auto found = cleanup_value_listener_lookup_.find(listener);
    if (found != cleanup_value_listener_lookup_.end()) {
      [found->second.get() removeAllObservers];
      cleanup_value_listener_lookup_.erase(found);
    }
    return true;
  }
  return false;
}

void DatabaseInternal::UnregisterAllValueListeners(const internal::QuerySpec& spec,
                                                   FIRDatabaseQuery* query_impl) {
  std::vector<ValueListener*> listeners;
  if (value_listeners_by_query_.Get(spec, &listeners)) {
    for (int i = 0; i < listeners.size(); i++) {
      UnregisterValueListener(spec, listeners[i], query_impl);
    }
  }
}

bool DatabaseInternal::RegisterChildListener(
    const internal::QuerySpec& spec, ChildListener* listener,
    FIRCPPDatabaseQueryCallbackState* _Nonnull callback_state) {
  MutexLock lock(listener_mutex_);
  if (child_listeners_by_query_.Register(spec, listener)) {
    auto found = cleanup_child_listener_lookup_.find(listener);
    if (found == cleanup_child_listener_lookup_.end()) {
      cleanup_child_listener_lookup_.insert(
          std::make_pair(listener, FIRCPPDatabaseQueryCallbackStatePointer(callback_state)));
    }
    return true;
  }
  return false;
}

bool DatabaseInternal::UnregisterChildListener(const internal::QuerySpec& spec,
                                               ChildListener* listener,
                                               FIRDatabaseQuery* query_impl) {
  MutexLock lock(listener_mutex_);
  if (child_listeners_by_query_.Unregister(spec, listener)) {
    auto found = cleanup_child_listener_lookup_.find(listener);
    if (found != cleanup_child_listener_lookup_.end()) {
      [found->second.get() removeAllObservers];
      cleanup_child_listener_lookup_.erase(found);
    }
    return true;
  }
  return false;
}

void DatabaseInternal::UnregisterAllChildListeners(const internal::QuerySpec& spec,
                                                   FIRDatabaseQuery* query_impl) {
  std::vector<ChildListener*> listeners;
  if (child_listeners_by_query_.Get(spec, &listeners)) {
    for (int i = 0; i < listeners.size(); i++) {
      UnregisterChildListener(spec, listeners[i], query_impl);
    }
  }
}

bool DatabaseInternal::initialized() const { return impl() != nullptr; }

}  // namespace internal
}  // namespace database
}  // namespace firebase
