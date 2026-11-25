/*
 * Copyright 2020 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "firestore/src/include/firebase/firestore.h"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <memory>
#include <utility>

#include "app/src/app_state.h"
#include "app/src/include/firebase/version.h"
#include "app/src/log.h"
#include "app/src/util.h"
#include "firestore/src/common/compiler_info.h"
#include "firestore/src/common/exception_common.h"
#include "firestore/src/common/futures.h"
#include "firestore/src/common/hard_assert_common.h"

#if defined(__ANDROID__)
#include "firestore/src/android/firestore_android.h"
#else
#include "firestore/src/main/firestore_main.h"
#endif  // defined(__ANDROID__)

#ifdef __APPLE__
#include "TargetConditionals.h"
#endif  // __APPLE__

namespace firebase {
namespace firestore {

DEFINE_FIREBASE_VERSION_STRING(FirebaseFirestore);

namespace {

const char kFirestoreServiceTag = 0;

const char* GetPlatform() {
#if defined(__ANDROID__)
  return "gl-android/";
#elif TARGET_OS_IOS
  return "gl-ios/";
#elif TARGET_OS_OSX
  return "gl-macos/";
#elif defined(_WIN32)
  return "gl-windows/";
#elif defined(__linux__)
  return "gl-linux/";
#else
  return "";
#endif
}

InitResult CheckInitialized(const FirestoreInternal& firestore) {
  if (!firestore.initialized()) {
    return kInitResultFailedMissingDependency;
  }

  return kInitResultSuccess;
}

void ValidateDatabase(const char* database_id) {
  if (!database_id) {
    SimpleThrowInvalidArgument(
        "Provided database ID must not be null. Use "
        "Firestore::GetInstance(app) if you'd like to use the default "
        "database ID.");
  }
}

}  // namespace

Firestore Firestore::GetInstance(const App& app, InitResult* init_result_out) {
  return Firestore::GetInstance(app, kDefaultDatabase, init_result_out);
}

Firestore Firestore::GetInstance(const App& app,
                                 const char* db_name,
                                 InitResult* init_result_out) {
  auto state = ::firebase::internal::AppState::FromApp(app);
  if (!state) {
    LogError("Firestore::GetInstance(): The app must be valid.");
    if (init_result_out) {
      *init_result_out = kInitResultFailedMissingDependency;
    }
    return Firestore{};
  }
  ValidateDatabase(db_name);

  InitResult init_result = kInitResultSuccess;
  auto [owner_state, internal] =
      state->GetOrCreateDetachableService<FirestoreInternal>(
          &kFirestoreServiceTag, db_name,
          [&](::firebase::internal::AppState& app_state)
              -> std::shared_ptr<FirestoreInternal> {
            auto created =
                std::make_shared<FirestoreInternal>(app_state, db_name);
            init_result = CheckInitialized(*created);
            if (init_result != kInitResultSuccess) {
              return nullptr;
            }
            // Note: because Firestore libraries are currently distributed in
            // a precompiled form, `GetFullCompilerInfo` will reflect the
            // compiler used to produce the binaries. Unfortunately, there is no
            // clear way to avoid that without breaking ODR.
            SetClientLanguage(std::string("gl-cpp/") + GetFullCompilerInfo());
            return created;
          });
  if (init_result_out) {
    *init_result_out = init_result;
  }
  if (!internal) {
    return Firestore{};
  }
  return Firestore(std::move(owner_state), std::move(internal));
}

Firestore::Firestore(std::shared_ptr<::firebase::internal::AppState> app_state,
                     std::shared_ptr<FirestoreInternal> internal)
    : app_state_(std::move(app_state)), internal_(std::move(internal)) {}

Firestore::~Firestore() = default;

Firestore& Firestore::operator=(const Firestore& src) {
  if (this == &src) return *this;
  // Pin the previous AppState until any previous `internal_` is released so
  // `~FirestoreInternal()` never dereferences a destroyed `AppState`.
  auto old_app_state = std::move(app_state_);
  internal_ = src.internal_;
  app_state_ = src.app_state_;
  return *this;
}

Firestore& Firestore::operator=(Firestore&& src) noexcept {
  if (this == &src) return *this;
  auto old_app_state = std::move(app_state_);
  internal_ = std::move(src.internal_);
  app_state_ = std::move(src.app_state_);
  return *this;
}

App Firestore::app() const {
  if (!app_state_) return {};
  return app_state_->TryMakeAppHandle();
}

CollectionReference Firestore::Collection(const char* collection_path) const {
  if (!collection_path) {
    SimpleThrowInvalidArgument("Collection path cannot be null.");
  }
  if (collection_path[0] == '\0') {
    SimpleThrowInvalidArgument("Collection path cannot be empty.");
  }

  if (!internal_) return {};
  return internal_->Collection(collection_path);
}

CollectionReference Firestore::Collection(
    const std::string& collection_path) const {
  return Collection(collection_path.c_str());
}

DocumentReference Firestore::Document(const char* document_path) const {
  if (!document_path) {
    SimpleThrowInvalidArgument("Document path cannot be null.");
  }
  if (document_path[0] == '\0') {
    SimpleThrowInvalidArgument("Document path cannot be empty.");
  }

  if (!internal_) return {};
  return internal_->Document(document_path);
}

DocumentReference Firestore::Document(const std::string& document_path) const {
  return Document(document_path.c_str());
}

Query Firestore::CollectionGroup(const char* collection_id) const {
  if (!collection_id) {
    SimpleThrowInvalidArgument("Collection ID cannot be null.");
  }
  if (collection_id[0] == '\0') {
    SimpleThrowInvalidArgument("Collection ID cannot be empty.");
  }

  if (!internal_) return {};
  return internal_->CollectionGroup(collection_id);
}

Query Firestore::CollectionGroup(const std::string& collection_id) const {
  if (!internal_) return {};
  return internal_->CollectionGroup(collection_id.c_str());
}

Settings Firestore::settings() const {
  if (!internal_) return {};
  return internal_->settings();
}

void Firestore::set_settings(Settings settings) {
  if (!internal_) return;
  internal_->set_settings(std::move(settings));
}

WriteBatch Firestore::batch() const {
  if (!internal_) return {};
  return internal_->batch();
}

Future<void> Firestore::RunTransaction(
    std::function<Error(Transaction&, std::string&)> update) {
  return RunTransaction({}, std::move(update));
}

Future<void> Firestore::RunTransaction(
    TransactionOptions options,
    std::function<Error(Transaction&, std::string&)> update) {
  if (!update) {
    SimpleThrowInvalidArgument(
        "Transaction update callback cannot be an empty function.");
  }

  if (!internal_) return FailedFuture<void>();
  return internal_->RunTransaction(std::move(update), options.max_attempts());
}

Future<void> Firestore::DisableNetwork() {
  if (!internal_) return FailedFuture<void>();
  return internal_->DisableNetwork();
}

Future<void> Firestore::EnableNetwork() {
  if (!internal_) return FailedFuture<void>();
  return internal_->EnableNetwork();
}

Future<void> Firestore::Terminate() {
  if (!internal_) return FailedFuture<void>();
  if (app_state_) {
    app_state_->DetachService(&kFirestoreServiceTag, internal_->database_name(),
                              internal_.get());
  }
  return internal_->Terminate();
}

Future<void> Firestore::WaitForPendingWrites() {
  if (!internal_) return FailedFuture<void>();
  return internal_->WaitForPendingWrites();
}

Future<void> Firestore::ClearPersistence() {
  if (!internal_) return FailedFuture<void>();
  return internal_->ClearPersistence();
}

ListenerRegistration Firestore::AddSnapshotsInSyncListener(
    std::function<void()> callback) {
  if (!callback) {
    SimpleThrowInvalidArgument(
        "Snapshots in sync listener callback cannot be an empty function.");
  }

  if (!internal_) return {};
  return internal_->AddSnapshotsInSyncListener(std::move(callback));
}

void Firestore::SetClientLanguage(const std::string& language_token) {
  // TODO(b/135633112): this is a temporary measure until the Firestore backend
  // rolls out Firebase platform logging.
  // Note: this implementation lumps together the language and platform tokens,
  // relying on the fact that `SetClientLanguage` doesn't validate or parse its
  // input in any way. This is deemed acceptable because reporting the platform
  // this way is a temporary measure.
  FirestoreInternal::SetClientLanguage(language_token + " " + GetPlatform());
}

Future<LoadBundleTaskProgress> Firestore::LoadBundle(
    const std::string& bundle) {
  if (!internal_) return FailedFuture<LoadBundleTaskProgress>();
  return internal_->LoadBundle(bundle);
}

Future<LoadBundleTaskProgress> Firestore::LoadBundle(
    const std::string& bundle,
    std::function<void(const LoadBundleTaskProgress&)> progress_callback) {
  if (!progress_callback) {
    SimpleThrowInvalidArgument(
        "Progress callback cannot be an empty function.");
  }

  if (!internal_) return FailedFuture<LoadBundleTaskProgress>();
  return internal_->LoadBundle(bundle, std::move(progress_callback));
}

Future<Query> Firestore::NamedQuery(const std::string& query_name) {
  if (!internal_) return FailedFuture<Query>();
  return internal_->NamedQuery(query_name);
}

}  // namespace firestore
}  // namespace firebase
