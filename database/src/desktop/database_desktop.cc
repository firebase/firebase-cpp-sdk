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

#include <memory>
#include <queue>
#include <stack>

#include "app/memory/shared_ptr.h"
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

Mutex g_database_reference_constructor_mutex;  // NOLINT

SingleValueListener::SingleValueListener(DatabaseInternal* database,
                                         const QuerySpec& query_spec,
                                         ReferenceCountedFutureImpl* future,
                                         SafeFutureHandle<DataSnapshot> handle)
    : database_(database),
      query_spec_(query_spec),
      future_(future),
      handle_(handle) {}

SingleValueListener::~SingleValueListener() {
  database_->RemoveSingleValueListener(this);
}

void SingleValueListener::OnValueChanged(const DataSnapshot& snapshot) {
  future_->CompleteWithResult<DataSnapshot>(handle_, kErrorNone, "", snapshot);
  delete this;
}

void SingleValueListener::OnCancelled(const Error& error_code,
                                      const char* error_message) {
  future_->Complete(handle_, error_code, error_message);
  delete this;
}

DatabaseInternal::DatabaseInternal(App* app)
    : DatabaseInternal(app, app ? app->options().database_url() : "") {
  constructor_url_ = std::string();
}

DatabaseInternal::DatabaseInternal(App* app, const char* url)
    : app_(app),
      future_manager_(),
      cleanup_(),
      database_url_(url),
      constructor_url_(url),
      logger_(app_common::FindAppLoggerByName(app->name())),
      repo_(nullptr) {
  assert(app);
  assert(url);

  // Spin up the token auto-update thread in Auth.
  app_->function_registry()->CallFunction(
      ::firebase::internal::FnAuthStartTokenListener, app_, nullptr, nullptr);
}

DatabaseInternal::~DatabaseInternal() {
  cleanup_.CleanupAll();

  // If initialization failed, there is nothing to clean up.
  if (app_ == nullptr) return;

  // If there are any pending listeners, delete their pointers.
  {
    MutexLock lock(listener_mutex_);
    for (SingleValueListener** listener_holder : single_value_listeners_) {
      delete *listener_holder;
      *listener_holder = nullptr;
    }
    single_value_listeners_.clear();
  }

  app_->function_registry()->CallFunction(
      ::firebase::internal::FnAuthStopTokenListener, app_, nullptr, nullptr);
}

App* DatabaseInternal::GetApp() { return app_; }

DatabaseReference DatabaseInternal::GetReference() {
  EnsureRepo();
  return DatabaseReference(new DatabaseReferenceInternal(
      const_cast<DatabaseInternal*>(this), Path()));
}

DatabaseReference DatabaseInternal::GetReference(const char* path) {
  EnsureRepo();
  return DatabaseReference(new DatabaseReferenceInternal(
      const_cast<DatabaseInternal*>(this), Path(path)));
}

DatabaseReference DatabaseInternal::GetReferenceFromUrl(const char* url) {
  EnsureRepo();
  ParseUrl parser;
  auto result = parser.Parse(url);
  if (parser.Parse(url) != ParseUrl::kParseOk) {
    logger_.LogError("Url is not valid: %s", url);
  }

  if (result != ParseUrl::kParseOk) {
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

  return DatabaseReference(new DatabaseReferenceInternal(
      const_cast<DatabaseInternal*>(this), Path(parser.path)));
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

static std::string* g_sdk_version = nullptr;
const char* DatabaseInternal::GetSdkVersion() {
  if (g_sdk_version == nullptr) {
    g_sdk_version = new std::string("Firebase Realtime Database 0.0.1");
  }
  return g_sdk_version->c_str();
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

void DatabaseInternal::EnsureRepo() {
  MutexLock lock(repo_mutex_);
  if (!repo_) {
    repo_ = MakeUnique<Repo>(app_, this, database_url_.c_str(), &logger_,
                             persistence_enabled_);
  }
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
