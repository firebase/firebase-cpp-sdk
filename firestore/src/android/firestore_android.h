// Copyright 2020 Google LLC

#ifndef FIREBASE_FIRESTORE_SRC_ANDROID_FIRESTORE_ANDROID_H_
#define FIREBASE_FIRESTORE_SRC_ANDROID_FIRESTORE_ANDROID_H_

#include <cstdint>
#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_set>

#include "app/src/app_android.h"
#include "app/src/cleanup_notifier.h"
#include "app/src/future_manager.h"
#include "app/src/include/firebase/app.h"
#include "firestore/src/android/lambda_event_listener.h"
#include "firestore/src/common/type_mapping.h"
#include "firestore/src/include/firebase/firestore/collection_reference.h"
#include "firestore/src/include/firebase/firestore/document_reference.h"
#include "firestore/src/include/firebase/firestore/load_bundle_task_progress.h"
#include "firestore/src/include/firebase/firestore/settings.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/jni_fwd.h"
#include "firestore/src/jni/object.h"
#include "firestore/src/jni/ownership.h"

namespace firebase::firestore {

class Firestore;
class ListenerRegistrationInternal;
class LoadBundleProgressCompletion;
class Transaction;
class WriteBatch;

template <typename EnumT>
class PromiseFactory;

extern const char kApiIdentifier[];

class FirestoreInternal {
 public:
  struct WeakTarget {
    explicit WeakTarget(FirestoreInternal* instance) : firestore(instance) {}
    std::recursive_mutex mutex;
    FirestoreInternal* firestore = nullptr;
  };

  enum class AsyncFn {
    kEnableNetwork = 0,
    kDisableNetwork,
    kRunTransaction,
    kTerminate,
    kWaitForPendingWrites,
    kClearPersistence,
    kGetNamedQuery,
    kLoadBundle,
    kCount,
  };

  FirestoreInternal(App* app, const std::string& database_id);
  ~FirestoreInternal();

  App* app() const { return app_; }
  bool initialized() const { return app_ != nullptr; }
  FutureManager& future_manager() { return future_manager_; }
  CleanupNotifier& cleanup() { return cleanup_; }
  const std::shared_ptr<WeakTarget>& weak_target() const {
    return weak_target_;
  }

  CollectionReference Collection(const char* collection_path) const;
  DocumentReference Document(const char* document_path) const;
  Query CollectionGroup(const char* collection_id) const;

  Settings settings() const;
  void set_settings(const Settings& settings);
  WriteBatch batch() const;

  Future<void> RunTransaction(
      std::function<Error(Transaction&, std::string&)> update,
      int32_t max_attempts);

  Future<void> DisableNetwork();
  Future<void> EnableNetwork();
  Future<void> Terminate();
  Future<void> WaitForPendingWrites();
  Future<void> ClearPersistence();

  ListenerRegistration AddSnapshotsInSyncListener(
      EventListener<void>* listener);
  ListenerRegistration AddSnapshotsInSyncListener(
      std::unique_ptr<EventListener<void>> listener);
  ListenerRegistration AddSnapshotsInSyncListener(
      std::function<void()> callback);
  ListenerRegistration AddSnapshotsInSyncListener(
      EventListener<void>* listener, bool passing_listener_ownership);

  void RegisterListenerRegistration(ListenerRegistrationInternal* registration);
  void UnregisterListenerRegistration(
      ListenerRegistrationInternal* registration);
  void ClearListeners();

  void RegisterBundleListener(LoadBundleProgressCompletion* listener);
  void UnregisterBundleListener(LoadBundleProgressCompletion* listener);
  void ClearBundleListeners(jni::Env& env);

  const std::string& database_name() const { return database_name_; }

  Future<LoadBundleTaskProgress> LoadBundle(const std::string& bundle);
  Future<LoadBundleTaskProgress> LoadBundle(
      const std::string& bundle,
      std::function<void(const LoadBundleTaskProgress&)> progress_callback);
  Future<Query> NamedQuery(const std::string& query_name);

  static jni::Env GetEnv();

  AggregateQuery NewAggregateQuery(jni::Env& env,
                                   const jni::Object& aggregate_query) const;
  AggregateQuerySnapshot NewAggregateQuerySnapshot(
      jni::Env& env, const jni::Object& aggregate_query_snapshot) const;
  CollectionReference NewCollectionReference(
      jni::Env& env, const jni::Object& reference) const;
  DocumentReference NewDocumentReference(jni::Env& env,
                                         const jni::Object& reference) const;
  DocumentSnapshot NewDocumentSnapshot(jni::Env& env,
                                       const jni::Object& snapshot) const;
  Query NewQuery(jni::Env& env, const jni::Object& query) const;
  QuerySnapshot NewQuerySnapshot(jni::Env& env,
                                 const jni::Object& snapshot) const;

  void set_firestore_public(Firestore* firestore_public) {
    firestore_public_ = firestore_public;
  }
  Firestore* firestore_public() { return firestore_public_; }
  const Firestore* firestore_public() const { return firestore_public_; }

  static FirestoreInternal* RecoverFirestore(jni::Env& env,
                                             const jni::Object& java_firestore);

  const jni::Global<jni::Object>& user_callback_executor() const {
    return user_callback_executor_;
  }

  static void SetClientLanguage(const std::string& language_token);

 private:
  friend class FirestoreIntegrationTest;

  FirestoreInternal* mutable_this() const {
    return const_cast<FirestoreInternal*>(this);
  }
  void ShutdownUserCallbackExecutor(jni::Env& env);

  static bool Initialize(App* app);
  static void ReleaseClassesLocked(jni::Env& env);
  static void Terminate(App* app);

  jni::Global<jni::Object> user_callback_executor_;
  App* app_ = nullptr;
  Firestore* firestore_public_ = nullptr;
  jni::Global<jni::Object> obj_;

  Mutex listener_registration_mutex_;
  std::unordered_set<ListenerRegistrationInternal*> listener_registrations_;

  Mutex bundle_listeners_mutex_;
  std::unordered_set<LoadBundleProgressCompletion*> bundle_listeners_;

  FutureManager future_manager_;
  std::unique_ptr<PromiseFactory<AsyncFn>> promises_;

  CleanupNotifier cleanup_;
  std::shared_ptr<WeakTarget> weak_target_;
  std::string database_name_;
};

class FirestoreInternalWeakReference {
 public:
  explicit FirestoreInternalWeakReference(FirestoreInternal* firestore)
      : target_(firestore ? firestore->weak_target() : nullptr) {}

  template <typename CallbackT>
  decltype(auto) Run(CallbackT&& callback) const {
    if (!target_) return std::forward<CallbackT>(callback)(nullptr);
    std::lock_guard lock(target_->mutex);
    return std::forward<CallbackT>(callback)(target_->firestore);
  }

  template <typename CallbackT>
  void RunIfValid(CallbackT&& callback) const {
    if (!target_) return;
    std::lock_guard lock(target_->mutex);
    if (target_->firestore) {
      std::forward<CallbackT>(callback)(*target_->firestore);
    }
  }

 private:
  std::shared_ptr<FirestoreInternal::WeakTarget> target_;
};

}  // namespace firebase::firestore

#endif  // FIREBASE_FIRESTORE_SRC_ANDROID_FIRESTORE_ANDROID_H_
