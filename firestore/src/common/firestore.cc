#include "firestore/src/include/firebase/firestore.h"

#include <cassert>
#include <map>

#include "app/meta/move.h"
#include "app/src/assert.h"
#include "app/src/cleanup_notifier.h"
#include "app/src/include/firebase/version.h"
#include "app/src/log.h"
#include "app/src/util.h"
#include "firestore/src/common/futures.h"

#if defined(__ANDROID__)
#include "firestore/src/android/firestore_android.h"
#elif defined(FIRESTORE_STUB_BUILD)
#include "firestore/src/stub/firestore_stub.h"
#else
#include "firestore/src/ios/firestore_ios.h"
#endif  // defined(__ANDROID__)

namespace firebase {
namespace firestore {

DEFINE_FIREBASE_VERSION_STRING(FirebaseFirestore);

namespace {

Mutex g_firestores_lock;  // NOLINT
std::map<App*, Firestore*>* g_firestores = nullptr;

// Ensures that the cache is initialized.
// Prerequisite: `g_firestores_lock` must be locked before calling this
// function.
std::map<App*, Firestore*>* FirestoreCache() {
  if (!g_firestores) {
    g_firestores = new std::map<App*, Firestore*>();
  }
  return g_firestores;
}

// Prerequisite: `g_firestores_lock` must be locked before calling this
// function.
Firestore* FindFirestoreInCache(App* app, InitResult* init_result_out) {
  auto* cache = FirestoreCache();

  auto found = cache->find(app);
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

}  // namespace

Firestore* Firestore::GetInstance(App* app, InitResult* init_result_out) {
  FIREBASE_ASSERT_MESSAGE(app != nullptr,
                          "Provided firebase::App must not be null.");

  MutexLock lock(g_firestores_lock);

  Firestore* from_cache = FindFirestoreInCache(app, init_result_out);
  if (from_cache) {
    return from_cache;
  }

  return AddFirestoreToCache(new Firestore(app), init_result_out);
}

Firestore* Firestore::GetInstance(InitResult* init_result_out) {
  App* app = App::GetInstance();
  FIREBASE_ASSERT_MESSAGE(app, "You must call firebase::App.Create first.");
  return Firestore::GetInstance(app, init_result_out);
}

Firestore* Firestore::CreateFirestore(App* app, FirestoreInternal* internal,
                                      InitResult* init_result_out) {
  FIREBASE_ASSERT_MESSAGE(app != nullptr,
                          "Provided firebase::App must not be null.");
  FIREBASE_ASSERT_MESSAGE(internal != nullptr,
                          "Provided FirestoreInternal must not be null.");

  MutexLock lock(g_firestores_lock);

  Firestore* from_cache = FindFirestoreInCache(app, init_result_out);
  FIREBASE_ASSERT_MESSAGE(from_cache == nullptr,
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

  // Once we remove STLPort support, change this back to `emplace`.
  FirestoreCache()->insert(std::make_pair(firestore->app(), firestore));
  return firestore;
}

Firestore::Firestore(::firebase::App* app)
    : Firestore{new FirestoreInternal{app}} {}

Firestore::Firestore(FirestoreInternal* internal)
    // TODO(wuandy): use make_unique once it is supported for our build here.
    : internal_(internal) {
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
  MutexLock lock(g_firestores_lock);

  if (!internal_) return;

  App* my_app = app();

  // Only need to unregister if internal_ is initialized.
  if (internal_->initialized()) {
    CleanupNotifier* app_notifier = CleanupNotifier::FindByOwner(my_app);
    assert(app_notifier);
    app_notifier->UnregisterObject(this);
  }

  // Force cleanup to happen first.
  internal_->cleanup().CleanupAll();
  delete internal_;
  internal_ = nullptr;
  // If a Firestore is explicitly deleted, remove it from our cache.
  FirestoreCache()->erase(my_app);
  // If it's the last one, delete the map.
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
  FIREBASE_ASSERT_MESSAGE(collection_path != nullptr,
                          "Provided collection path must not be null");
  if (!internal_) return {};
  return internal_->Collection(collection_path);
}

CollectionReference Firestore::Collection(
    const std::string& collection_path) const {
  return Collection(collection_path.c_str());
}

DocumentReference Firestore::Document(const char* document_path) const {
  if (!internal_) return {};
  return internal_->Document(document_path);
}

DocumentReference Firestore::Document(const std::string& document_path) const {
  return Document(document_path.c_str());
}

Query Firestore::CollectionGroup(const std::string& collection_id) const {
  if (!internal_) return {};
  return internal_->CollectionGroup(collection_id.c_str());
}

Settings Firestore::settings() const {
  if (!internal_) return {};
  return internal_->settings();
}

void Firestore::set_settings(const Settings& settings) {
  if (!internal_) return;
  internal_->set_settings(settings);
}

WriteBatch Firestore::batch() const {
  if (!internal_) return {};
  return internal_->batch();
}

Future<void> Firestore::RunTransaction(TransactionFunction* update) {
  if (!internal_) return FailedFuture<void>();
  return internal_->RunTransaction(update);
}

#if defined(FIREBASE_USE_STD_FUNCTION) || defined(DOXYGEN)
Future<void> Firestore::RunTransaction(
    std::function<Error(Transaction*, std::string*)> update) {
  FIREBASE_ASSERT_MESSAGE(update, "invalid update parameter is passed in.");
  if (!internal_) return FailedFuture<void>();
  return internal_->RunTransaction(firebase::Move(update));
}
#endif  // defined(FIREBASE_USE_STD_FUNCTION) || defined(DOXYGEN)

Future<void> Firestore::RunTransactionLastResult() {
  if (!internal_) return FailedFuture<void>();
  return internal_->RunTransactionLastResult();
}

Future<void> Firestore::DisableNetwork() {
  if (!internal_) return FailedFuture<void>();
  return internal_->DisableNetwork();
}

Future<void> Firestore::DisableNetworkLastResult() {
  if (!internal_) return FailedFuture<void>();
  return internal_->DisableNetworkLastResult();
}

Future<void> Firestore::EnableNetwork() {
  if (!internal_) return FailedFuture<void>();
  return internal_->EnableNetwork();
}

Future<void> Firestore::EnableNetworkLastResult() {
  if (!internal_) return FailedFuture<void>();
  return internal_->EnableNetworkLastResult();
}

Future<void> Firestore::Terminate() {
  if (!internal_) return FailedFuture<void>();
  FirestoreCache()->erase(app());
  return internal_->Terminate();
}

Future<void> Firestore::TerminateLastResult() {
  if (!internal_) return FailedFuture<void>();
  return internal_->TerminateLastResult();
}

Future<void> Firestore::WaitForPendingWrites() {
  if (!internal_) return FailedFuture<void>();
  return internal_->WaitForPendingWrites();
}

Future<void> Firestore::WaitForPendingWritesLastResult() {
  if (!internal_) return FailedFuture<void>();
  return internal_->WaitForPendingWritesLastResult();
}

Future<void> Firestore::ClearPersistence() {
  if (!internal_) return FailedFuture<void>();
  return internal_->ClearPersistence();
}

Future<void> Firestore::ClearPersistenceLastResult() {
  if (!internal_) return FailedFuture<void>();
  return internal_->ClearPersistenceLastResult();
}

}  // namespace firestore
}  // namespace firebase
