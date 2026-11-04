// Copyright 2020 Google LLC
#include "firestore/src/include/firebase/firestore.h"

#include <cassert>
#include <cstdint>
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
#endif

#ifdef __APPLE__
#include "TargetConditionals.h"
#endif

namespace firebase::firestore {

DEFINE_FIREBASE_VERSION_STRING(FirebaseFirestore);

namespace {

const char* GetPlatform() {
#if defined(__ANDROID__)
  return "gl-android/";
#elif defined(__APPLE__) && TARGET_OS_IOS
  return "gl-ios/";
#elif defined(__APPLE__) && TARGET_OS_TV
  return "gl-tvos/";
#elif defined(__APPLE__) && TARGET_OS_OSX
  return "gl-macos/";
#elif defined(_WIN32)
  return "gl-windows/";
#elif defined(__linux__)
  return "gl-linux/";
#else
  return "";
#endif
}

using FirestoreMap = std::map<std::pair<App*, std::string>, Firestore*>;
FirestoreMap::key_type MakeKey(App* app, std::string database_id) {
  return {app, std::move(database_id)};
}

Mutex* g_firestores_lock = new Mutex();
FirestoreMap* g_firestores = nullptr;

FirestoreMap* FirestoreCache() {
  if (!g_firestores) g_firestores = new FirestoreMap();
  return g_firestores;
}

Firestore* FindFirestoreInCache(App* app, const std::string& database_id,
                                InitResult* init_result_out) {
  auto* cache = FirestoreCache();
  if (auto found = cache->find(MakeKey(app, database_id)); found != cache->end()) {
    if (init_result_out) *init_result_out = kInitResultSuccess;
    return found->second;
  }
  return nullptr;
}

InitResult CheckInitialized(const FirestoreInternal& firestore) {
  return firestore.initialized() ? kInitResultSuccess : kInitResultFailedMissingDependency;
}

void ValidateApp(App* app) {
  if (!app) {
    SimpleThrowInvalidArgument(
        "firebase::App instance cannot be null. Use other Firestore::GetInstance() if you'd like to use the default app instance.");
  }
}

void ValidateDatabase(const char* database_id) {
  if (!database_id) {
    SimpleThrowInvalidArgument(
        "Provided database ID must not be null. Use other Firestore::GetInstance() if you'd like to use the default database ID.");
  }
}

}  // namespace

Firestore* Firestore::GetInstance(InitResult* init_result_out) {
  return Firestore::GetInstance(kDefaultDatabase, init_result_out);
}

Firestore* Firestore::GetInstance(App* app, InitResult* init_result_out) {
  return Firestore::GetInstance(app, kDefaultDatabase, init_result_out);
}

Firestore* Firestore::GetInstance(App* app, const char* db_name, InitResult* init_result_out) {
  ValidateApp(app);
  ValidateDatabase(db_name);

  Firestore* to_delete = nullptr;
  Firestore* result = nullptr;
  {
    MutexLock lock(*g_firestores_lock);
    if (Firestore* from_cache = FindFirestoreInCache(app, db_name, init_result_out)) {
      return from_cache;
    }
    Firestore* new_firestore = new Firestore(app, db_name);
    result = AddFirestoreToCache(new_firestore, init_result_out);
    if (!result) to_delete = new_firestore;
  }
  delete to_delete;
  return result;
}

Firestore* Firestore::GetInstance(const char* db_name, InitResult* init_result_out) {
  App* app = App::GetInstance();
  if (!app) {
    SimpleThrowInvalidArgument(
        "Failed to get firebase::App instance. Please call firebase::App::Create before using Firestore");
  }
  return Firestore::GetInstance(app, db_name, init_result_out);
}

Firestore* Firestore::CreateFirestore(App* app, FirestoreInternal* internal,
                                      InitResult* init_result_out) {
  ValidateApp(app);
  SIMPLE_HARD_ASSERT(internal != nullptr, "Provided FirestoreInternal must not be null.");

  Firestore* to_delete = nullptr;
  Firestore* result = nullptr;
  {
    MutexLock lock(*g_firestores_lock);
    Firestore* from_cache = FindFirestoreInCache(app, internal->database_name(), init_result_out);
    SIMPLE_HARD_ASSERT(from_cache == nullptr, "Firestore must not be created already");
    Firestore* new_firestore = new Firestore(internal);
    result = AddFirestoreToCache(new_firestore, init_result_out);
    if (!result) to_delete = new_firestore;
  }
  delete to_delete;
  return result;
}

Firestore* Firestore::AddFirestoreToCache(Firestore* firestore, InitResult* init_result_out) {
  InitResult init_result = CheckInitialized(*firestore->internal_);
  if (init_result_out) *init_result_out = init_result;
  if (init_result != kInitResultSuccess) return nullptr;
  FirestoreCache()->emplace(MakeKey(firestore->app(), firestore->internal_->database_name()),
                            firestore);
  return firestore;
}

Firestore::Firestore(::firebase::App* app, const std::string& database_id)
    : Firestore{new FirestoreInternal{app, database_id}} {}

Firestore::Firestore(FirestoreInternal* internal) : internal_(internal) {
  internal_->set_firestore_public(this);
  if (internal_->initialized()) {
    SetClientLanguage(std::string("gl-cpp/") + GetFullCompilerInfo());
    CleanupNotifier* app_notifier = CleanupNotifier::FindByOwner(app());
    assert(app_notifier);
    if (app_notifier) {
      app_notifier->RegisterObject(this, [](void* object) {
        auto* firestore = reinterpret_cast<Firestore*>(object);
        LogWarning(
            "Firestore object %p should be deleted before the App %p it depends upon.",
            static_cast<void*>(firestore), static_cast<void*>(firestore->app()));
        firestore->DeleteInternal();
      });
    }
  }
}

Firestore::~Firestore() { DeleteInternal(); }

void Firestore::DeleteInternal() {
  FirestoreInternal* internal_to_delete = nullptr;
  App* my_app = nullptr;
  {
    MutexLock lock(*g_firestores_lock);
    if (!internal_) return;
    internal_to_delete = std::exchange(internal_, nullptr);
    my_app = internal_to_delete->app();
    if (g_firestores) {
      FirestoreMap::key_type key = MakeKey(my_app, internal_to_delete->database_name());
      if (auto iter = g_firestores->find(key);
          iter != g_firestores->end() && iter->second == this) {
        g_firestores->erase(iter);
      }
      if (g_firestores->empty()) {
        delete std::exchange(g_firestores, nullptr);
      }
    }
  }

  if (internal_to_delete->initialized()) {
    if (CleanupNotifier* app_notifier = CleanupNotifier::FindByOwner(my_app)) {
      app_notifier->UnregisterObject(this);
    }
  }
  internal_to_delete->ClearListeners();
  internal_to_delete->cleanup().CleanupAll();
  delete internal_to_delete;
}

const App* Firestore::app() const { return internal_ ? internal_->app() : nullptr; }
App* Firestore::app() { return internal_ ? internal_->app() : nullptr; }

CollectionReference Firestore::Collection(const char* collection_path) const {
  if (!collection_path) SimpleThrowInvalidArgument("Collection path cannot be null.");
  return Collection(std::string(collection_path));
}

CollectionReference Firestore::Collection(const std::string& collection_path) const {
  if (collection_path.empty()) SimpleThrowInvalidArgument("Collection path cannot be empty.");
  return internal_ ? internal_->Collection(collection_path.c_str()) : CollectionReference{};
}

DocumentReference Firestore::Document(const char* document_path) const {
  if (!document_path) SimpleThrowInvalidArgument("Document path cannot be null.");
  return Document(std::string(document_path));
}

DocumentReference Firestore::Document(const std::string& document_path) const {
  if (document_path.empty()) SimpleThrowInvalidArgument("Document path cannot be empty.");
  return internal_ ? internal_->Document(document_path.c_str()) : DocumentReference{};
}

Query Firestore::CollectionGroup(const char* collection_id) const {
  if (!collection_id) SimpleThrowInvalidArgument("Collection ID cannot be null.");
  return CollectionGroup(std::string(collection_id));
}

Query Firestore::CollectionGroup(const std::string& collection_id) const {
  if (collection_id.empty()) SimpleThrowInvalidArgument("Collection ID cannot be empty.");
  return internal_ ? internal_->CollectionGroup(collection_id.c_str()) : Query{};
}

Settings Firestore::settings() const { return internal_ ? internal_->settings() : Settings{}; }

void Firestore::set_settings(Settings settings) {
  if (internal_) internal_->set_settings(std::move(settings));
}

WriteBatch Firestore::batch() const { return internal_ ? internal_->batch() : WriteBatch{}; }

Future<void> Firestore::RunTransaction(
    std::function<Error(Transaction&, std::string&)> update) {
  return RunTransaction({}, std::move(update));
}

Future<void> Firestore::RunTransaction(
    TransactionOptions options, std::function<Error(Transaction&, std::string&)> update) {
  if (!update) {
    SimpleThrowInvalidArgument("Transaction update callback cannot be an empty function.");
  }
  return internal_ ? internal_->RunTransaction(std::move(update), options.max_attempts())
                   : FailedFuture<void>();
}

Future<void> Firestore::DisableNetwork() {
  return internal_ ? internal_->DisableNetwork() : FailedFuture<void>();
}

Future<void> Firestore::EnableNetwork() {
  return internal_ ? internal_->EnableNetwork() : FailedFuture<void>();
}

Future<void> Firestore::Terminate() {
  if (!internal_) return FailedFuture<void>();
  {
    MutexLock lock(*g_firestores_lock);
    if (g_firestores) {
      FirestoreMap::key_type key = MakeKey(app(), internal_->database_name());
      if (auto iter = g_firestores->find(key);
          iter != g_firestores->end() && iter->second == this) {
        g_firestores->erase(iter);
      }
      if (g_firestores->empty()) {
        delete std::exchange(g_firestores, nullptr);
      }
    }
  }
  return internal_->Terminate();
}

Future<void> Firestore::WaitForPendingWrites() {
  return internal_ ? internal_->WaitForPendingWrites() : FailedFuture<void>();
}

Future<void> Firestore::ClearPersistence() {
  return internal_ ? internal_->ClearPersistence() : FailedFuture<void>();
}

ListenerRegistration Firestore::AddSnapshotsInSyncListener(std::function<void()> callback) {
  if (!callback) {
    SimpleThrowInvalidArgument("Snapshots in sync listener callback cannot be an empty function.");
  }
  return internal_ ? internal_->AddSnapshotsInSyncListener(std::move(callback))
                   : ListenerRegistration{};
}

void Firestore::SetClientLanguage(const std::string& language_token) {
  FirestoreInternal::SetClientLanguage(language_token + " " + GetPlatform());
}

Future<LoadBundleTaskProgress> Firestore::LoadBundle(const std::string& bundle) {
  return internal_ ? internal_->LoadBundle(bundle) : FailedFuture<LoadBundleTaskProgress>();
}

Future<LoadBundleTaskProgress> Firestore::LoadBundle(
    const std::string& bundle,
    std::function<void(const LoadBundleTaskProgress&)> progress_callback) {
  if (!progress_callback) {
    SimpleThrowInvalidArgument("Progress callback cannot be an empty function.");
  }
  return internal_ ? internal_->LoadBundle(bundle, std::move(progress_callback))
                   : FailedFuture<LoadBundleTaskProgress>();
}

Future<Query> Firestore::NamedQuery(const std::string& query_name) {
  return internal_ ? internal_->NamedQuery(query_name) : FailedFuture<Query>();
}

}  // namespace firebase::firestore
