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

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "app/src/app_state.h"
#include "app/src/assert.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/internal/platform.h"
#include "app/src/include/firebase/version.h"
#include "app/src/log.h"
#include "app/src/util.h"

// DatabaseInternal is defined in these 3 files, one implementation for each OS.
#if FIREBASE_PLATFORM_ANDROID
#include "database/src/android/database_android.h"
#elif FIREBASE_PLATFORM_IOS || FIREBASE_PLATFORM_TVOS
#include "database/src/ios/database_ios.h"
#elif defined(FIREBASE_TARGET_DESKTOP)
#include "database/src/desktop/database_desktop.h"
#else
#include "database/src/stub/database_stub.h"
#endif  // FIREBASE_PLATFORM_ANDROID, FIREBASE_PLATFORM_IOS,
        // FIREBASE_PLATFORM_TVOS, defined(FIREBASE_TARGET_DESKTOP)

// Register the module initializer.
FIREBASE_APP_REGISTER_CALLBACKS(
    database, { return ::firebase::kInitResultSuccess; }, false);

namespace firebase {
namespace database {

DEFINE_FIREBASE_VERSION_STRING(FirebaseDatabase);

using internal::DatabaseInternal;

namespace {
constexpr int kDatabaseServiceTag = 0;
}  // namespace

Database::Database(std::shared_ptr<internal::DatabaseInternal> internal)
    : internal_(std::move(internal)) {}

Database Database::GetInstance(const App& app, const char* url,
                               InitResult* init_result_out) {
  auto state = ::firebase::internal::AppState::FromApp(app);
  if (!state) {
    LogError("Database::GetInstance(): The app must be valid.");
    if (init_result_out != nullptr) {
      *init_result_out = kInitResultFailedMissingDependency;
    }
    return Database();
  }

  std::string url_key = url ? url : "";
  auto internal = state->GetOrCreateService<DatabaseInternal>(
      &kDatabaseServiceTag, url_key,
      [&app, url](::firebase::internal::AppState& app_state)
          -> std::shared_ptr<DatabaseInternal> {
        auto inst =
            url ? std::make_shared<DatabaseInternal>(app_state, app, url)
                : std::make_shared<DatabaseInternal>(app_state, app);
        if (!inst->initialized()) {
          return nullptr;
        }
        return inst;
      });

  if (!internal) {
    if (init_result_out != nullptr) {
      *init_result_out = kInitResultFailedMissingDependency;
    }
    return Database();
  }

  if (init_result_out != nullptr) {
    *init_result_out = kInitResultSuccess;
  }
  return Database(std::move(internal));
}

Database Database::GetInstance(const App& app, InitResult* init_result_out) {
  return GetInstance(app, nullptr, init_result_out);
}

App Database::app() const { return internal_ ? internal_->GetApp() : App(); }

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
