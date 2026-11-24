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

#include <cassert>
#include <memory>
#include <queue>
#include <stack>

#include "app/src/assert.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/future.h"
#include "app/src/reference_counted_future_impl.h"
#include "app/src/service_interfaces.h"
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

SingleValueListener::SingleValueListener(DatabaseInternal* database,
                                         const QuerySpec& query_spec,
                                         ReferenceCountedFutureImpl* future,
                                         SafeFutureHandle<DataSnapshot> handle)
    : database_(database),
      query_spec_(query_spec),
      future_(future),
      handle_(handle) {}

SingleValueListener::~SingleValueListener() {}

void SingleValueListener::OnValueChanged(const DataSnapshot& snapshot) {
  future_->CompleteWithResult<DataSnapshot>(handle_, kErrorNone, "", snapshot);
}

void SingleValueListener::OnCancelled(const Error& error_code,
                                      const char* error_message) {
  future_->Complete(handle_, error_code, error_message);
}

DatabaseInternal::DatabaseInternal(::firebase::internal::AppState& app_state,
                                   const App& app)
    : DatabaseInternal(
          app_state, app,
          app.options().database_url() ? app.options().database_url() : "") {
  constructor_url_ = std::string();
}

DatabaseInternal::DatabaseInternal(::firebase::internal::AppState& app_state,
                                   const App& app, const char* url)
    : app_state_(app_state),
      future_manager_(),
      database_url_(url ? url : ""),
      constructor_url_(url ? url : ""),
      logger_(&app_state.logger()),
      repo_(nullptr) {
  (void)app;
  // Spin up the token auto-update thread in Auth if Auth is active.
  if (auto* auth = app_state_.auth_token_provider()) {
    auth->StartTokenListener();
  }
}

DatabaseInternal::~DatabaseInternal() {
  // Destroy Repo first so its scheduler/connection stop before token listeners
  // and future_manager_ are torn down.
  repo_.reset();

  if (auto* auth = app_state_.auth_token_provider()) {
    auth->StopTokenListener();
  }
}

App DatabaseInternal::GetApp() const { return app_state_.TryMakeAppHandle(); }

std::shared_ptr<DatabaseInternal> DatabaseInternal::MakeSharedHandle() const {
  auto state = app_state_.weak_from_this().lock();
  if (!state) return {};
  return std::shared_ptr<DatabaseInternal>(
      std::move(state), const_cast<DatabaseInternal*>(this));
}

DatabaseReference DatabaseInternal::GetReference() {
  auto self = MakeSharedHandle();
  if (!self) return DatabaseReference();
  EnsureRepo();
  return DatabaseReference(
      new DatabaseReferenceInternal(std::move(self), Path()));
}

DatabaseReference DatabaseInternal::GetReference(const char* path) {
  auto self = MakeSharedHandle();
  if (!self) return DatabaseReference();
  EnsureRepo();
  return DatabaseReference(
      new DatabaseReferenceInternal(std::move(self), Path(path)));
}

DatabaseReference DatabaseInternal::GetReferenceFromUrl(const char* url) {
  auto self = MakeSharedHandle();
  if (!self) return DatabaseReference();
  EnsureRepo();
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

  return DatabaseReference(
      new DatabaseReferenceInternal(std::move(self), Path(parser.path)));
}

void DatabaseInternal::GoOffline() {
  EnsureRepo();
  Repo::scheduler().Schedule(NewCallback(
      [](Repo::ThisRef ref) {
        Repo::ThisRefLock lock(&ref);
        if (lock.GetReference() != nullptr) {
          lock.GetReference()->connection()->Interrupt();
        }
      },
      repo_->this_ref()));
}

void DatabaseInternal::GoOnline() {
  EnsureRepo();
  Repo::scheduler().Schedule(NewCallback(
      [](Repo::ThisRef ref) {
        Repo::ThisRefLock lock(&ref);
        if (lock.GetReference() != nullptr) {
          lock.GetReference()->connection()->Resume();
        }
      },
      repo_->this_ref()));
}

void DatabaseInternal::PurgeOutstandingWrites() {
  EnsureRepo();
  Repo::scheduler().Schedule(NewCallback(
      [](Repo::ThisRef ref) {
        Repo::ThisRefLock lock(&ref);
        if (lock.GetReference() != nullptr) {
          lock.GetReference()->PurgeOutstandingWrites();
        }
      },
      repo_->this_ref()));
}

const char* DatabaseInternal::GetSdkVersion() {
  return "Firebase Realtime Database 0.0.1";
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
    const internal::QuerySpec& spec, ValueListener* listener,
    ValueListenerCleanupData cleanup_data) {
  MutexLock lock(listener_mutex_);
  if (value_listeners_by_query_.Register(spec, listener)) {
    auto found = cleanup_value_listener_lookup_.find(listener);
    if (found == cleanup_value_listener_lookup_.end()) {
      cleanup_value_listener_lookup_.insert(
          std::make_pair(listener, std::move(cleanup_data)));
    }
    return true;
  }
  return false;
}

bool DatabaseInternal::UnregisterValueListener(const internal::QuerySpec& spec,
                                               ValueListener* listener) {
  EventRegistration* registration =
      ActiveEventRegistration(spec, (void*)listener);
  if (registration) {
    registration->set_status(EventRegistration::kRemoved);
  }

  MutexLock lock(listener_mutex_);
  if (value_listeners_by_query_.Unregister(spec, listener)) {
    auto found = cleanup_value_listener_lookup_.find(listener);
    if (found != cleanup_value_listener_lookup_.end()) {
      // ValueListenerCleanupData& cleanup_data = found->second;
      cleanup_value_listener_lookup_.erase(found);
    }
    return true;
  }
  return false;
}

void DatabaseInternal::UnregisterAllValueListeners(
    const internal::QuerySpec& spec) {
  std::vector<ValueListener*> listeners;
  if (value_listeners_by_query_.Get(spec, &listeners)) {
    for (ValueListener* listener : listeners) {
      UnregisterValueListener(spec, listener);
    }
  }
}

bool DatabaseInternal::RegisterChildListener(
    const internal::QuerySpec& spec, ChildListener* listener,
    ChildListenerCleanupData cleanup_data) {
  MutexLock lock(listener_mutex_);
  if (child_listeners_by_query_.Register(spec, listener)) {
    auto found = cleanup_child_listener_lookup_.find(listener);
    if (found == cleanup_child_listener_lookup_.end()) {
      cleanup_child_listener_lookup_.insert(
          std::make_pair(listener, std::move(cleanup_data)));
    }
    return true;
  }
  return false;
}

bool DatabaseInternal::UnregisterChildListener(const internal::QuerySpec& spec,
                                               ChildListener* listener) {
  MutexLock lock(listener_mutex_);
  if (child_listeners_by_query_.Unregister(spec, listener)) {
    auto found = cleanup_child_listener_lookup_.find(listener);
    if (found != cleanup_child_listener_lookup_.end()) {
      cleanup_child_listener_lookup_.erase(found);
    }
    return true;
  }
  return false;
}

void DatabaseInternal::UnregisterAllChildListeners(
    const internal::QuerySpec& spec) {
  std::vector<ChildListener*> listeners;
  if (child_listeners_by_query_.Get(spec, &listeners)) {
    for (ChildListener* listener : listeners) {
      UnregisterChildListener(spec, listener);
    }
  }
}

void DatabaseInternal::AddEventRegistration(
    const QuerySpec& query_spec, void* listener_ptr,
    EventRegistration* event_registration) {
  event_registration_lookup_[query_spec][listener_ptr].push_back(
      event_registration);
}

EventRegistration* DatabaseInternal::ActiveEventRegistration(
    const QuerySpec& query_spec, void* listener_ptr) {
  auto* listener_registration_map =
      MapGet(&event_registration_lookup_, query_spec);
  if (!listener_registration_map) {
    return nullptr;
  }
  auto* registration_vector = MapGet(listener_registration_map, listener_ptr);
  if (!registration_vector) {
    return nullptr;
  }
  for (auto* registration : *registration_vector) {
    if (registration->status() == EventRegistration::kActive) {
      return registration;
    }
  }
  return nullptr;
}

void DatabaseInternal::EnsureRepo() {
  MutexLock lock(repo_mutex_);
  if (!repo_) {
    repo_ = std::make_unique<Repo>(app_state_, this, database_url_.c_str(),
                                   &logger_, persistence_enabled_);
  }
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
