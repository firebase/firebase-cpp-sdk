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

#include "database/src/desktop/database_desktop.h"

#include <algorithm>
#include <cassert>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "app/src/app_common.h"
#include "app/src/assert.h"
#include "app/src/function_registry.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/future.h"
#include "app/src/reference_counted_future_impl.h"
#include "app/src/variant_util.h"
#include "database/src/desktop/core/indexed_variant.h"
#include "database/src/desktop/data_snapshot_desktop.h"
#include "database/src/desktop/database_reference_desktop.h"
#include "database/src/desktop/mutable_data_desktop.h"
#include "database/src/desktop/util_desktop.h"
#include "database/src/include/firebase/database/common.h"
#include "database/src/include/firebase/database/database_reference.h"

namespace firebase {

using callback::NewCallback;

namespace database {
namespace internal {

// Referenced by database/src/common/database.cc and database_reference.cc to
// synchronize DatabaseReference construction, copy/move, and cleanup.
Mutex g_database_reference_constructor_mutex;  // NOLINT

SingleValueListener::SingleValueListener(DatabaseInternal* database,
                                         const QuerySpec& query_spec,
                                         ReferenceCountedFutureImpl* future,
                                         SafeFutureHandle<DataSnapshot> handle)
    : database_ref_(
          database
              ? database->this_ref()
              : firebase::internal::SafeReference<DatabaseInternal>(nullptr)),
      query_spec_(query_spec),
      future_(future),
      handle_(handle),
      backing_future_(future ? MakeFuture(future, handle)
                             : Future<DataSnapshot>()),
      completed_(false) {}

SingleValueListener::~SingleValueListener() {
  if (!completed_ && future_ != nullptr &&
      backing_future_.status() == kFutureStatusPending) {
    completed_ = true;
    future_->Complete(handle_, kErrorUnknownError,
                      "The database is being destroyed.");
  }
  firebase::internal::SafeReferenceLock<DatabaseInternal> lock(&database_ref_);
  if (lock.GetReference() != nullptr) {
    lock.GetReference()->UnregisterValueListener(query_spec_, this);
  }
}

void SingleValueListener::OnValueChanged(const DataSnapshot& snapshot) {
  if (!completed_ && future_ != nullptr) {
    completed_ = true;
    future_->CompleteWithResult<DataSnapshot>(handle_, kErrorNone, "",
                                              snapshot);
  }
}

void SingleValueListener::OnCancelled(const Error& error_code,
                                      const char* error_message) {
  if (!completed_ && future_ != nullptr) {
    completed_ = true;
    future_->Complete(handle_, error_code, error_message);
  }
}

DatabaseInternal::DatabaseInternal(App* app)
    : DatabaseInternal(app, app ? app->options().database_url() : "") {
  constructor_url_.clear();
}

DatabaseInternal::DatabaseInternal(App* app, const char* url)
    : app_(app),
      database_url_(url ? url : ""),
      constructor_url_(url ? url : ""),
      persistence_enabled_(false),
      logger_(app ? app_common::FindAppLoggerByName(app->name()) : nullptr),
      safe_this_(this),
      repo_(nullptr) {
  assert(url);

  if (app_ != nullptr) {
    // Spin up the token auto-update thread in Auth.
    app_->function_registry()->CallFunction(
        ::firebase::internal::FnAuthStartTokenListener, app_, nullptr, nullptr);
  }
}

DatabaseInternal::~DatabaseInternal() {
  cleanup_.CleanupAll();
  safe_this_.ClearReference();
  if (app_ != nullptr) {
    app_->function_registry()->CallFunction(
        ::firebase::internal::FnAuthStopTokenListener, app_, nullptr, nullptr);
  }
  {
    MutexLock lock(repo_mutex_);
    repo_.reset(nullptr);
    repo_lifetime_objects_.clear();
  }
}

App* DatabaseInternal::GetApp() const { return app_; }

DatabaseReference DatabaseInternal::GetReference() {
  EnsureRepo();
  return DatabaseReference(new DatabaseReferenceInternal(this, Path()));
}

DatabaseReference DatabaseInternal::GetReference(const char* path) {
  if (path == nullptr) {
    logger_.LogError("Database::GetReference(): Path cannot be null.");
    return DatabaseReference();
  }
  EnsureRepo();
  return DatabaseReference(new DatabaseReferenceInternal(this, Path(path)));
}

DatabaseReference DatabaseInternal::GetReferenceFromUrl(const char* url) {
  if (url == nullptr) {
    logger_.LogError("Database::GetReferenceFromUrl(): Url cannot be null.");
    return DatabaseReference();
  }
  ParseUrl parser;
  auto result = parser.Parse(url);
  if (result != ParseUrl::kParseOk) {
    logger_.LogError("Url is not valid: %s", url);
    return DatabaseReference();
  }

  connection::HostInfo host_info(parser.hostname.c_str(), parser.ns.c_str(),
                                 parser.secure);

  if (host_info.ToString() != database_url()) {
    logger_.LogError(
        "The hostname of this url (%s) is different from the database url "
        "(%s)",
        url, database_url());
    return DatabaseReference();
  }

  EnsureRepo();
  return DatabaseReference(
      new DatabaseReferenceInternal(this, Path(parser.path)));
}

void DatabaseInternal::GoOffline() {
  Repo::ThisRef repo_ref = EnsureRepoRef();
  Repo::scheduler().Schedule(NewCallback(
      [](Repo::ThisRef ref) {
        Repo::ThisRefLock lock(&ref);
        if (lock.GetReference() != nullptr) {
          lock.GetReference()->connection()->Interrupt();
        }
      },
      repo_ref));
}

void DatabaseInternal::GoOnline() {
  Repo::ThisRef repo_ref = EnsureRepoRef();
  Repo::scheduler().Schedule(NewCallback(
      [](Repo::ThisRef ref) {
        Repo::ThisRefLock lock(&ref);
        if (lock.GetReference() != nullptr) {
          lock.GetReference()->connection()->Resume();
        }
      },
      repo_ref));
}

void DatabaseInternal::PurgeOutstandingWrites() {
  Repo::ThisRef repo_ref = EnsureRepoRef();
  Repo::scheduler().Schedule(NewCallback(
      [](Repo::ThisRef ref) {
        Repo::ThisRefLock lock(&ref);
        if (lock.GetReference() != nullptr) {
          lock.GetReference()->PurgeOutstandingWrites();
        }
      },
      repo_ref));
}

const char* DatabaseInternal::GetSdkVersion() const {
  static const char kSdkVersion[] = "Firebase Realtime Database 0.0.1";
  return kSdkVersion;
}

void DatabaseInternal::SetPersistenceEnabled(bool enabled) {
  MutexLock lock(repo_mutex_);
  // Only set persistence if the repo has not yet been initialized.
  if (!repo_) {
    persistence_enabled_ = enabled;
  }
}

void DatabaseInternal::set_log_level(LogLevel log_level) {
  logger_.SetLogLevel(log_level);
}

LogLevel DatabaseInternal::log_level() const { return logger_.GetLogLevel(); }

bool DatabaseInternal::RegisterValueListener(
    const QuerySpec& spec, ValueListener* listener,
    ValueListenerCleanupData cleanup_data) {
  MutexLock lock(listener_mutex_);
  if (value_listeners_by_query_.Register(spec, listener)) {
    auto key = std::make_pair(spec, listener);
    auto found = cleanup_value_listener_lookup_.find(key);
    if (found == cleanup_value_listener_lookup_.end()) {
      cleanup_value_listener_lookup_.insert(
          std::make_pair(std::move(key), std::move(cleanup_data)));
    }
    return true;
  }
  return false;
}

bool DatabaseInternal::UnregisterValueListener(const QuerySpec& spec,
                                               ValueListener* listener) {
  MutexLock lock(listener_mutex_);
  return UnregisterValueListenerLocked(spec, listener);
}

bool DatabaseInternal::UnregisterValueListenerLocked(const QuerySpec& spec,
                                                     ValueListener* listener) {
  RemoveActiveEventRegistrationsLocked(spec, static_cast<void*>(listener));
  if (value_listeners_by_query_.Unregister(spec, listener)) {
    cleanup_value_listener_lookup_.erase(std::make_pair(spec, listener));
    return true;
  }
  return false;
}

void DatabaseInternal::UnregisterAllValueListeners(const QuerySpec& spec) {
  MutexLock lock(listener_mutex_);
  std::vector<ValueListener*> listeners;
  if (value_listeners_by_query_.Get(spec, &listeners)) {
    for (ValueListener* listener : listeners) {
      UnregisterValueListenerLocked(spec, listener);
    }
  }
}

bool DatabaseInternal::RegisterChildListener(
    const QuerySpec& spec, ChildListener* listener,
    ChildListenerCleanupData cleanup_data) {
  MutexLock lock(listener_mutex_);
  if (child_listeners_by_query_.Register(spec, listener)) {
    auto key = std::make_pair(spec, listener);
    auto found = cleanup_child_listener_lookup_.find(key);
    if (found == cleanup_child_listener_lookup_.end()) {
      cleanup_child_listener_lookup_.insert(
          std::make_pair(std::move(key), std::move(cleanup_data)));
    }
    return true;
  }
  return false;
}

bool DatabaseInternal::UnregisterChildListener(const QuerySpec& spec,
                                               ChildListener* listener) {
  MutexLock lock(listener_mutex_);
  return UnregisterChildListenerLocked(spec, listener);
}

bool DatabaseInternal::UnregisterChildListenerLocked(const QuerySpec& spec,
                                                     ChildListener* listener) {
  RemoveActiveEventRegistrationsLocked(spec, static_cast<void*>(listener));
  if (child_listeners_by_query_.Unregister(spec, listener)) {
    cleanup_child_listener_lookup_.erase(std::make_pair(spec, listener));
    return true;
  }
  return false;
}

void DatabaseInternal::UnregisterAllChildListeners(const QuerySpec& spec) {
  MutexLock lock(listener_mutex_);
  std::vector<ChildListener*> listeners;
  if (child_listeners_by_query_.Get(spec, &listeners)) {
    for (ChildListener* listener : listeners) {
      UnregisterChildListenerLocked(spec, listener);
    }
  }
}

void DatabaseInternal::AddEventRegistration(
    const QuerySpec& query_spec, void* listener_ptr,
    EventRegistration* event_registration) {
  MutexLock lock(listener_mutex_);
  event_registration_lookup_[query_spec][listener_ptr].push_back(
      event_registration);
}

void DatabaseInternal::RemoveEventRegistration(
    const QuerySpec& query_spec, void* listener_ptr,
    EventRegistration* event_registration) {
  MutexLock lock(listener_mutex_);
  auto query_it = event_registration_lookup_.find(query_spec);
  if (query_it == event_registration_lookup_.end()) {
    return;
  }
  auto& listener_map = query_it->second;
  auto listener_it = listener_map.find(listener_ptr);
  if (listener_it == listener_map.end()) {
    return;
  }
  auto& vec = listener_it->second;
  vec.erase(std::remove(vec.begin(), vec.end(), event_registration), vec.end());
  if (vec.empty()) {
    listener_map.erase(listener_it);
    if (listener_map.empty()) {
      event_registration_lookup_.erase(query_it);
    }
  }
}

void DatabaseInternal::RemoveActiveEventRegistrations(
    const QuerySpec& query_spec, void* listener_ptr) {
  MutexLock lock(listener_mutex_);
  RemoveActiveEventRegistrationsLocked(query_spec, listener_ptr);
}

void DatabaseInternal::RemoveActiveEventRegistrationsLocked(
    const QuerySpec& query_spec, void* listener_ptr) {
  auto query_it = event_registration_lookup_.find(query_spec);
  if (query_it == event_registration_lookup_.end()) {
    return;
  }
  auto& listener_map = query_it->second;
  if (listener_ptr != nullptr) {
    auto listener_it = listener_map.find(listener_ptr);
    if (listener_it != listener_map.end()) {
      for (auto* registration : listener_it->second) {
        if (registration->status() == EventRegistration::kActive) {
          registration->set_status(EventRegistration::kRemoved);
        }
      }
      listener_map.erase(listener_it);
      if (listener_map.empty()) {
        event_registration_lookup_.erase(query_it);
      }
    }
  } else {
    for (auto& kv : listener_map) {
      for (auto* registration : kv.second) {
        if (registration->status() == EventRegistration::kActive) {
          registration->set_status(EventRegistration::kRemoved);
        }
      }
    }
    event_registration_lookup_.erase(query_it);
  }
}

Repo* DatabaseInternal::EnsureRepo() {
  MutexLock lock(repo_mutex_);
  if (!repo_) {
    repo_ = std::make_unique<Repo>(app_, this, database_url_.c_str(), &logger_,
                                   persistence_enabled_);
  }
  return repo_.get();
}

Repo::ThisRef DatabaseInternal::EnsureRepoRef() {
  MutexLock lock(repo_mutex_);
  if (!repo_) {
    repo_ = std::make_unique<Repo>(app_, this, database_url_.c_str(), &logger_,
                                   persistence_enabled_);
  }
  return repo_->this_ref();
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
