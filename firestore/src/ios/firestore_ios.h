#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_IOS_FIRESTORE_IOS_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_IOS_FIRESTORE_IOS_H_

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>  // NOLINT(build/c++11)
#include <unordered_set>

#include "app/src/cleanup_notifier.h"
#include "app/src/future_manager.h"
#include "app/src/include/firebase/app.h"
#include "firestore/src/include/firebase/firestore/collection_reference.h"
#include "firestore/src/include/firebase/firestore/document_reference.h"
#include "firestore/src/include/firebase/firestore/settings.h"
#include "firestore/src/ios/promise_factory_ios.h"
#include "Firestore/core/src/api/firestore.h"
#include "Firestore/core/src/auth/credentials_provider.h"
#include "Firestore/core/src/model/database_id.h"

namespace firebase {
namespace firestore {

class Firestore;
class ListenerRegistrationInternal;
class Transaction;
class TransactionFunction;
class WriteBatch;

namespace util {
class Executor;
}

class FirestoreInternal {
 public:
  // Note: call `set_firestore_public` immediately after construction.
  explicit FirestoreInternal(App* app);
  ~FirestoreInternal();

  FirestoreInternal(const FirestoreInternal&) = delete;
  FirestoreInternal& operator=(const FirestoreInternal&) = delete;

  App* app() const { return app_; }

  // Whether this object was successfully initialized by the constructor.
  bool initialized() const { return app_ != nullptr; }

  // Manages all Future objects returned from Firestore API.
  FutureManager& future_manager() { return future_manager_; }

  // When this is deleted, it will clean up all DatabaseReferences,
  // DataSnapshots, and other such objects.
  CleanupNotifier& cleanup() { return cleanup_; }

  CollectionReference Collection(const char* collection_path) const;

  DocumentReference Document(const char* document_path) const;

  Query CollectionGroup(const char* collection_id) const;

  Settings settings() const;
  void set_settings(Settings settings);

  WriteBatch batch() const;

  Future<void> RunTransaction(
      std::function<Error(Transaction&, std::string&)> update);
  Future<void> RunTransaction(TransactionFunction* update);

  Future<void> DisableNetwork();

  Future<void> EnableNetwork();

  Future<void> Terminate();

  Future<void> WaitForPendingWrites();

  Future<void> ClearPersistence();

  ListenerRegistration AddSnapshotsInSyncListener(
      EventListener<void>* listener);
  ListenerRegistration AddSnapshotsInSyncListener(
      std::function<void()> callback);

  const model::DatabaseId& database_id() const {
    return firestore_core_->database_id();
  }

  // Manages the ListenerRegistrationInternal objects.
  void RegisterListenerRegistration(ListenerRegistrationInternal* registration);
  void UnregisterListenerRegistration(
      ListenerRegistrationInternal* registration);
  void ClearListeners();

  void set_firestore_public(Firestore* firestore_public) {
    firestore_public_ = firestore_public;
  }

  Firestore* firestore_public() { return firestore_public_; }
  const Firestore* firestore_public() const { return firestore_public_; }

  const std::shared_ptr<api::Firestore>& firestore_core() const {
    return firestore_core_;
  }

  static void SetClientLanguage(const std::string& language_token);

 private:
  friend class TestFriend;

  enum class AsyncApi {
    kEnableNetwork = 0,
    kDisableNetwork,
    kRunTransaction,
    kTerminate,
    kWaitForPendingWrites,
    kClearPersistence,
    kCount,
  };

  FirestoreInternal(App* app,
                    std::unique_ptr<auth::CredentialsProvider> credentials);

  std::shared_ptr<api::Firestore> CreateFirestore(
      App* app, std::unique_ptr<auth::CredentialsProvider> credentials);

  // Gets the reference-counted Future implementation of this instance, which
  // can be used to create a Future.
  ReferenceCountedFutureImpl* ref_future() {
    return future_manager_.GetFutureApi(this);
  }

  void ApplyDefaultSettings();

  App* app_ = nullptr;
  Firestore* firestore_public_ = nullptr;
  std::shared_ptr<api::Firestore> firestore_core_;

  CleanupNotifier cleanup_;

  FutureManager future_manager_;
  PromiseFactory<AsyncApi> promise_factory_{&cleanup_, &future_manager_};

  // TODO(b/136119216): revamp this mechanism on both iOS and Android.
  std::mutex listeners_mutex_;
  std::unordered_set<ListenerRegistrationInternal*> listeners_;

  std::shared_ptr<util::Executor> transaction_executor_;
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_IOS_FIRESTORE_IOS_H_
