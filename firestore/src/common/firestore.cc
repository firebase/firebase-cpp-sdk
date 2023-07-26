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
#include <cstring>
#include <map>
#include <utility>

#include "app/src/cleanup_notifier.h"
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

// Use the combination of the App address and database ID to identify a
// firestore instance in cache.
using FirestoreMap = std::map<std::pair<App*, std::string>, Firestore*>;

FirestoreMap::key_type MakeKey(App* app, std::string database_id) {
  return std::make_pair(app, std::move(database_id));
}

Mutex* g_firestores_lock = new Mutex();
FirestoreMap* g_firestores = nullptr;

// Ensures that the cache is initialized.
// Prerequisite: `g_firestores_lock` must be locked before calling this
// function.
FirestoreMap* FirestoreCache() {
  if (!g_firestores) {
    g_firestores = new FirestoreMap();
  }
  return g_firestores;
}

// Prerequisite: `g_firestores_lock` must be locked before calling this
// function.
Firestore* FindFirestoreInCache(App* app,
                                const std::string& database_id,
                                InitResult* init_result_out) {
  auto* cache = FirestoreCache();

  FirestoreMap::key_type key = MakeKey(app, database_id);
  FirestoreMap::iterator found = g_firestores->find(key);

  if (found != cache->end()) {
    if (init_result_out) *init_result_out = kInitResultSuccess;
    return found->second;
  }

  return nullptr;
}

InitResult CheckInitialized(const FirestoreInternal& firestore) {
  if (!firestore.initialized()) {
    return kInitResultFailedMissingDependency;
  }

  return kInitResultSuccess;
}

void ValidateApp(App* app) {
  if (!app) {
    SimpleThrowInvalidArgument(
        "firebase::App instance cannot be null. Use other "
        "Firestore::GetInstance() if you'd like to use the default "
        "app instance.");
  }
}

void ValidateDatabase(const char* database_id) {
  if (!database_id) {
    SimpleThrowInvalidArgument(
        "Provided database ID must not be null. Use other "
        "Firestore::GetInstance() if you'd like to use the default "
        "database ID.");
  }
}

}  // namespace

Firestore* Firestore::GetInstance(InitResult* init_result_out) {
  App* app = App::GetInstance();
  if (!app) {
    SimpleThrowInvalidArgument(
        "Failed to get firebase::App instance. Please call "
        "firebase::App::Create before using Firestore");
  }
  return Firestore::GetInstance(app, kDefaultDatabase, init_result_out);
}

Firestore* Firestore::GetInstance(App* app, InitResult* init_result_out) {
  return Firestore::GetInstance(app, kDefaultDatabase, init_result_out);
}

Firestore* Firestore::GetInstance(const char* db_name,
                                  InitResult* init_result_out) {
  App* app = App::GetInstance();
  if (!app) {
    SimpleThrowInvalidArgument(
        "Failed to get firebase::App instance. Please call "
        "firebase::App::Create before using Firestore");
  }
  return Firestore::GetInstance(app, db_name, init_result_out);
}

Firestore* Firestore::GetInstance(App* app,
                                  const char* db_name,
                                  InitResult* init_result_out) {
  ValidateApp(app);
  ValidateDatabase(db_name);

  MutexLock lock(*g_firestores_lock);
  Firestore* from_cache = FindFirestoreInCache(app, db_name, init_result_out);
  if (from_cache) {
    return from_cache;
  }

  return AddFirestoreToCache(new Firestore(app, db_name), init_result_out);
}

Firestore* Firestore::CreateFirestore(App* app,
                                      FirestoreInternal* internal,
                                      InitResult* init_result_out) {
  ValidateApp(app);
  SIMPLE_HARD_ASSERT(internal != nullptr,
                     "Provided FirestoreInternal must not be null.");

  MutexLock lock(*g_firestores_lock);

  const std::string& database_id = internal->database_name();
  Firestore* from_cache =
      FindFirestoreInCache(app, database_id, init_result_out);
  SIMPLE_HARD_ASSERT(from_cache == nullptr,
                     "Firestore must not be created already");

  return AddFirestoreToCache(new Firestore(internal), init_result_out);
}

Firestore* Firestore::AddFirestoreToCache(Firestore* firestore,
                                          InitResult* init_result_out) {
  InitResult init_result = CheckInitialized(*firestore->internal_);
  if (init_result_out) {
    *init_result_out = init_result;
  }
  if (init_result != kInitResultSuccess) {
    delete firestore;
    return nullptr;
  }

  FirestoreMap::key_type key =
      MakeKey(firestore->app(), firestore->internal_->database_name());
  FirestoreCache()->emplace(std::move(key), firestore);
  return firestore;
}

Firestore::Firestore(::firebase::App* app, const std::string& database_id)
    : Firestore{new FirestoreInternal{app, database_id}} {}

Firestore::Firestore(FirestoreInternal* internal)
    // TODO(wuandy): use make_unique once it is supported for our build here.
    : internal_(internal) {
  internal_->set_firestore_public(this);

  // Note: because Firestore libraries are currently distributed in
  // a precompiled form, `GetFullCompilerInfo` will reflect the compiler used to
  // produce the binaries. Unfortunately, there is no clear way to avoid that
  // without breaking ODR.
  SetClientLanguage(std::string("gl-cpp/") + GetFullCompilerInfo());

  if (internal_->initialized()) {
    CleanupNotifier* app_notifier = CleanupNotifier::FindByOwner(app());
    assert(app_notifier);
    app_notifier->RegisterObject(this, [](void* object) {
      Firestore* firestore = reinterpret_cast<Firestore*>(object);
      LogWarning(
          "Firestore object 0x%08x should be deleted before the App 0x%08x it "
          "depends upon.",
          static_cast<int>(reinterpret_cast<intptr_t>(firestore)),
          static_cast<int>(reinterpret_cast<intptr_t>(firestore->app())));
      firestore->DeleteInternal();
    });
  }
}

Firestore::~Firestore() { DeleteInternal(); }

void Firestore::DeleteInternal() {
  MutexLock lock(*g_firestores_lock);

  if (!internal_) return;

  App* my_app = app();
  // Store the database id before deleting the firestore instance.
  const std::string database_id = internal_->database_name();

  // Only need to unregister if internal_ is initialized.
  if (internal_->initialized()) {
    CleanupNotifier* app_notifier = CleanupNotifier::FindByOwner(my_app);
    assert(app_notifier);
    app_notifier->UnregisterObject(this);
  }

  // Make sure to clear the listeners _before_ triggering cleanup. This avoids
  // a potential deadlock that can happen if the Firestore instance is destroyed
  // in parallel with or shortly after a snapshot listener's invocation:
  // - the thread on which cleanup is being executed holds the cleanup lock and
  // tries to mute listeners, which requires the listeners' lock;
  // - in parallel on the user callbacks' thread which holds the listeners'
  // lock, one of the user callbacks is being destroyed, which leads to an
  // attempt to unregister an object from cleanup, requiring the cleanup lock.
  internal_->ClearListeners();

  // Force cleanup to happen first.
  internal_->cleanup().CleanupAll();
  delete internal_;
  internal_ = nullptr;
  // If a Firestore is explicitly deleted, remove it from our cache.

  FirestoreMap::key_type key = MakeKey(my_app, database_id);
  FirestoreCache()->erase(key);
  if (g_firestores->empty()) {
    delete g_firestores;
    g_firestores = nullptr;
  }
}

const App* Firestore::app() const {
  if (!internal_) return {};
  return internal_->app();
}

App* Firestore::app() {
  if (!internal_) return {};
  return internal_->app();
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
  FirestoreMap::key_type key = MakeKey(app(), internal_->database_name());

  FirestoreCache()->erase(key);
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
