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

#include "database/src/include/firebase/database.h"

#include <assert.h>

#include <map>
#include <string>

#include "app/src/app_common.h"
#include "app/src/assert.h"
#include "app/src/cleanup_notifier.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/internal/platform.h"
#include "app/src/include/firebase/version.h"
#include "app/src/util.h"

// DatabaseInternal is defined in these 3 files, one implementation for each OS.
#if FIREBASE_PLATFORM_ANDROID
#include "database/src/android/database_android.h"
#elif FIREBASE_PLATFORM_IOS
#include "database/src/ios/database_ios.h"
#elif defined(FIREBASE_TARGET_DESKTOP)
#include "database/src/desktop/database_desktop.h"
#else
#include "database/src/stub/database_stub.h"
#endif  // FIREBASE_PLATFORM_ANDROID, FIREBASE_PLATFORM_IOS,
        // defined(FIREBASE_TARGET_DESKTOP)

// Register the module initializer.
FIREBASE_APP_REGISTER_CALLBACKS(
    database,
    {
      FIREBASE_UTIL_RETURN_FAILURE_IF_GOOGLE_PLAY_UNAVAILABLE(*app);
      return ::firebase::kInitResultSuccess;
    },
    {
        // Nothing to tear down.
    });

namespace firebase {
namespace database {

DEFINE_FIREBASE_VERSION_STRING(FirebaseDatabase);

using internal::DatabaseInternal;

using DatabaseMap = std::map<std::pair<std::string, std::string>, Database*>;
Mutex g_databases_lock;  // NOLINT
static DatabaseMap* g_databases = nullptr;

namespace {
DatabaseMap::key_type MakeKey(App* app, const std::string& url) {
  return std::make_pair(std::string(app->name()), url);
}
}  // namespace

Database* Database::GetInstance(App* app, const char* url,
                                InitResult* init_result_out) {
  if (!app) {
    LogError("Database::GetInstance(): The app must not be null.");
    return nullptr;
  }
  MutexLock lock(g_databases_lock);
  if (!g_databases) {
    g_databases = new DatabaseMap();
  }

  DatabaseMap::key_type key = MakeKey(app, std::string(url ? url : ""));
  DatabaseMap::iterator it = g_databases->find(key);
  if (it != g_databases->end()) {
    if (init_result_out != nullptr) *init_result_out = kInitResultSuccess;
    return it->second;
  }
  FIREBASE_UTIL_RETURN_NULL_IF_GOOGLE_PLAY_UNAVAILABLE(*app, init_result_out);

  Database* database = url ? new Database(app, new DatabaseInternal(app, url))
                           : new Database(app, new DatabaseInternal(app));
  if (!database->internal_->initialized()) {
    if (init_result_out) *init_result_out = kInitResultFailedMissingDependency;
    delete database;
    return nullptr;
  }
  g_databases->insert(std::make_pair(key, database));
  if (init_result_out) *init_result_out = kInitResultSuccess;
  return database;
}

Database* Database::GetInstance(App* app, InitResult* init_result_out) {
  return GetInstance(app, nullptr, init_result_out);
}

Database::Database(::firebase::App* app, internal::DatabaseInternal* internal)
    : internal_(internal) {
  if (internal_->initialized()) {
    CleanupNotifier* app_notifier = CleanupNotifier::FindByOwner(app);
    assert(app_notifier);
    app_notifier->RegisterObject(this, [](void* object) {
      Database* database = reinterpret_cast<Database*>(object);
      Logger* logger = app_common::FindAppLoggerByName(database->app()->name());
      logger->LogWarning(
          "Database object 0x%08x should be deleted before the App 0x%08x it "
          "depends upon.",
          static_cast<int>(reinterpret_cast<intptr_t>(database)),
          static_cast<int>(reinterpret_cast<intptr_t>(database->app())));
      database->DeleteInternal();
    });
  }
}

void Database::DeleteInternal() {
  MutexLock lock(g_databases_lock);

  if (!internal_) return;

  App* my_app = app();
  std::string url = internal_->constructor_url();

  // Only need to unregister if internal_ is initialized
  if (internal_->initialized()) {
    CleanupNotifier* app_notifier = CleanupNotifier::FindByOwner(my_app);
    assert(app_notifier);
    app_notifier->UnregisterObject(this);
  }

  {
    MutexLock db_ref_lock(internal::g_database_reference_constructor_mutex);
    // Force cleanup to happen first.
    internal_->cleanup().CleanupAll();
  }
  delete internal_;
  internal_ = nullptr;
  // If a Database is explicitly deleted, remove it from our cache.
  g_databases->erase(MakeKey(my_app, url));
  // If it's the last one, delete the map.
  if (g_databases->empty()) {
    delete g_databases;
    g_databases = nullptr;
  }
}

Database::~Database() { DeleteInternal(); }

Database::Database(const Database& src) {
  FIREBASE_ASSERT_MESSAGE(false,
                          "Can't copy a firebase::database::Database object.");
}

Database& Database::operator=(const Database& src) {
  FIREBASE_ASSERT_MESSAGE(false,
                          "Can't copy a firebase::database::Database object.");
  return *this;
}

App* Database::app() const { return internal_ ? internal_->GetApp() : nullptr; }

const char* Database::url() const {
  return internal_ ? internal_->constructor_url().c_str() : "";
}

DatabaseReference Database::GetReference() const {
  return internal_ ? internal_->GetReference() : DatabaseReference();
}

DatabaseReference Database::GetReference(const char* path) const {
  return internal_ && path ? internal_->GetReference(path)
                           : DatabaseReference();
}

DatabaseReference Database::GetReferenceFromUrl(const char* url) const {
  return internal_ && url ? internal_->GetReferenceFromUrl(url)
                          : DatabaseReference();
}

void Database::GoOffline() {
  if (internal_) internal_->GoOffline();
}

void Database::GoOnline() {
  if (internal_) internal_->GoOnline();
}

void Database::PurgeOutstandingWrites() {
  if (internal_) internal_->PurgeOutstandingWrites();
}

void Database::set_persistence_enabled(bool enabled) {
  if (internal_) internal_->SetPersistenceEnabled(enabled);
}

void Database::set_log_level(LogLevel log_level) {
  if (internal_) internal_->set_log_level(log_level);
}

LogLevel Database::log_level() const {
  return internal_ ? internal_->log_level() : kLogLevelInfo;
}

}  // namespace database
}  // namespace firebase
