/*
 * Copyright 2016 Google LLC
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

#include "firestore/src/android/firestore_android.h"

#include <cstdint>
#include <utility>
#include <vector>

#include "app/src/assert.h"
#include "app/src/embedded_file.h"
#include "app/src/include/firebase/future.h"
#include "app/src/reference_counted_future_impl.h"
#include "app/src/util_android.h"
#include "firebase/firestore/firestore_version.h"
#include "firestore/firestore_resources.h"
#include "firestore/src/android/aggregate_query_android.h"
#include "firestore/src/android/aggregate_query_snapshot_android.h"
#include "firestore/src/android/aggregate_source_android.h"
#include "firestore/src/android/blob_android.h"
#include "firestore/src/android/collection_reference_android.h"
#include "firestore/src/android/converter_android.h"
#include "firestore/src/android/direction_android.h"
#include "firestore/src/android/document_change_android.h"
#include "firestore/src/android/document_change_type_android.h"
#include "firestore/src/android/document_reference_android.h"
#include "firestore/src/android/document_snapshot_android.h"
#include "firestore/src/android/event_listener_android.h"
#include "firestore/src/android/exception_android.h"
#include "firestore/src/android/field_path_android.h"
#include "firestore/src/android/field_value_android.h"
#include "firestore/src/android/filter_android.h"
#include "firestore/src/android/geo_point_android.h"
#include "firestore/src/android/jni_runnable_android.h"
#include "firestore/src/android/lambda_event_listener.h"
#include "firestore/src/android/lambda_transaction_function.h"
#include "firestore/src/android/listener_registration_android.h"
#include "firestore/src/android/load_bundle_task_android.h"
#include "firestore/src/android/load_bundle_task_progress_android.h"
#include "firestore/src/android/metadata_changes_android.h"
#include "firestore/src/android/promise_android.h"
#include "firestore/src/android/query_android.h"
#include "firestore/src/android/query_snapshot_android.h"
#include "firestore/src/android/server_timestamp_behavior_android.h"
#include "firestore/src/android/set_options_android.h"
#include "firestore/src/android/settings_android.h"
#include "firestore/src/android/snapshot_metadata_android.h"
#include "firestore/src/android/source_android.h"
#include "firestore/src/android/timestamp_android.h"
#include "firestore/src/android/transaction_android.h"
#include "firestore/src/android/transaction_options_android.h"
#include "firestore/src/android/transaction_options_builder_android.h"
#include "firestore/src/android/wrapper.h"
#include "firestore/src/android/write_batch_android.h"
#include "firestore/src/common/hard_assert_common.h"
#include "firestore/src/include/firebase/firestore.h"
#include "firestore/src/jni/arena_ref.h"
#include "firestore/src/jni/array.h"
#include "firestore/src/jni/array_list.h"
#include "firestore/src/jni/boolean.h"
#include "firestore/src/jni/collection.h"
#include "firestore/src/jni/double.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/hash_map.h"
#include "firestore/src/jni/integer.h"
#include "firestore/src/jni/iterator.h"
#include "firestore/src/jni/jni.h"
#include "firestore/src/jni/list.h"
#include "firestore/src/jni/loader.h"
#include "firestore/src/jni/long.h"
#include "firestore/src/jni/map.h"
#include "firestore/src/jni/task.h"

namespace firebase {
namespace firestore {

class LoadBundleProgressCompletion
    : public EventListener<LoadBundleTaskProgress>,
      public Promise<LoadBundleTaskProgress,
                     LoadBundleTaskProgressInternal,
                     FirestoreInternal::AsyncFn>::Completion {
 public:
  LoadBundleProgressCompletion(
      FirestoreInternal* firestore,
      std::function<void(const LoadBundleTaskProgress&)> callback)
      : firestore_ref_(firestore), callback_(std::move(callback)) {}

  void SetJavaListener(const jni::Object& java_listener) {
    std::lock_guard<std::mutex> lock(mutex_);
    java_listener_ = java_listener;
  }

  void DiscardJavaListener(jni::Env& env) {
    jni::Global<jni::Object> listener;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      listener = std::move(java_listener_);
    }
    if (listener) {
      jni::ExceptionClearGuard guard(env);
      EventListenerInternal::DiscardPointers(env, listener);
      listener.clear();
    }
  }

  void OnEvent(const LoadBundleTaskProgress& progress,
               Error,
               const std::string&) override {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      last_docs_ = progress.documents_loaded();
      last_total_docs_ = progress.total_documents();
      last_bytes_ = progress.bytes_loaded();
      last_total_bytes_ = progress.total_bytes();
      if (progress.state() != LoadBundleTaskProgress::State::kInProgress) {
        saw_terminal_state_ = true;
      }
    }
    if (callback_) {
      callback_(progress);
    }
  }

  void CompleteWith(Error error_code,
                    const char*,
                    LoadBundleTaskProgress*) override {
    firestore_ref_.RunIfValid([this](FirestoreInternal& firestore) {
      firestore.UnregisterBundleListener(this);
    });

    jni::Env env;
    DiscardJavaListener(env);

    if (error_code != Error::kErrorOk && callback_) {
      bool should_notify_error = false;
      LoadBundleTaskProgress error_progress;
      {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!saw_terminal_state_) {
          saw_terminal_state_ = true;
          should_notify_error = true;
          error_progress = LoadBundleTaskProgress(
              last_docs_, last_total_docs_, last_bytes_, last_total_bytes_,
              LoadBundleTaskProgress::State::kError);
        }
      }
      if (should_notify_error) {
        callback_(error_progress);
      }
    }

    delete this;
  }

 private:
  FirestoreInternalWeakReference firestore_ref_;
  std::function<void(const LoadBundleTaskProgress&)> callback_;
  std::mutex mutex_;
  jni::Global<jni::Object> java_listener_;
  int32_t last_docs_ = 0;
  int32_t last_total_docs_ = 0;
  int64_t last_bytes_ = 0;
  int64_t last_total_bytes_ = 0;
  bool saw_terminal_state_ = false;
};

namespace {

using jni::Array;
using jni::Constructor;
using jni::Env;
using jni::ExceptionClearGuard;
using jni::Global;
using jni::HashMap;
using jni::Loader;
using jni::Local;
using jni::Long;
using jni::Method;
using jni::Object;
using jni::StaticMethod;
using jni::String;
using jni::Task;

constexpr char kFirestoreClassName[] =
    PROGUARD_KEEP_CLASS "com/google/firebase/firestore/FirebaseFirestore";

Method<Object> kCollection(
    "collection",
    "(Ljava/lang/String;)"
    "Lcom/google/firebase/firestore/CollectionReference;");
Method<Object> kDocument("document",
                         "(Ljava/lang/String;)"
                         "Lcom/google/firebase/firestore/DocumentReference;");
Method<Object> kCollectionGroup("collectionGroup",
                                "(Ljava/lang/String;)"
                                "Lcom/google/firebase/firestore/Query;");
Method<SettingsInternal> kGetSettings(
    "getFirestoreSettings",
    "()Lcom/google/firebase/firestore/FirebaseFirestoreSettings;");
StaticMethod<Object> kGetInstance(
    "getInstance",
    "(Lcom/google/firebase/FirebaseApp;Ljava/lang/String;)"
    "Lcom/google/firebase/firestore/FirebaseFirestore;");
StaticMethod<void> kSetLoggingEnabled("setLoggingEnabled", "(Z)V");
StaticMethod<void> kSetClientLanguage("setClientLanguage",
                                      "(Ljava/lang/String;)V");
Method<void> kSetSettings(
    "setFirestoreSettings",
    "(Lcom/google/firebase/firestore/FirebaseFirestoreSettings;)V");
Method<Object> kBatch("batch", "()Lcom/google/firebase/firestore/WriteBatch;");
Method<Task> kRunTransaction(
    "runTransaction",
    "(Lcom/google/firebase/firestore/TransactionOptions;"
    "Lcom/google/firebase/firestore/Transaction$Function;)"
    "Lcom/google/android/gms/tasks/Task;");
Method<Task> kEnableNetwork("enableNetwork",
                            "()Lcom/google/android/gms/tasks/Task;");
Method<Task> kDisableNetwork("disableNetwork",
                             "()Lcom/google/android/gms/tasks/Task;");
Method<Task> kTerminate("terminate", "()Lcom/google/android/gms/tasks/Task;");
Method<Task> kWaitForPendingWrites("waitForPendingWrites",
                                   "()Lcom/google/android/gms/tasks/Task;");
Method<Task> kClearPersistence("clearPersistence",
                               "()Lcom/google/android/gms/tasks/Task;");
Method<Object> kAddSnapshotsInSyncListener(
    "addSnapshotsInSyncListener",
    "(Ljava/util/concurrent/Executor;Ljava/lang/Runnable;)"
    "Lcom/google/firebase/firestore/ListenerRegistration;");
Method<Task> kGetNamedQuery("getNamedQuery",
                            "(Ljava/lang/String;)"
                            "Lcom/google/android/gms/tasks/Task;");
Method<LoadBundleTaskInternal> kLoadBundle(
    "loadBundle",
    "([B)"
    "Lcom/google/firebase/firestore/LoadBundleTask;");

void InitializeFirestore(Loader& loader) {
  loader.LoadClass(kFirestoreClassName, kCollection, kDocument,
                   kCollectionGroup, kGetSettings, kGetInstance,
                   kSetLoggingEnabled, kSetClientLanguage, kSetSettings, kBatch,
                   kRunTransaction, kEnableNetwork, kDisableNetwork, kTerminate,
                   kWaitForPendingWrites, kClearPersistence,
                   kAddSnapshotsInSyncListener, kGetNamedQuery, kLoadBundle);
}

constexpr char kUserCallbackExecutorClassName[] = PROGUARD_KEEP_CLASS
    "com/google/firebase/firestore/internal/cpp/"
    "SilentRejectionSingleThreadExecutor";
Constructor<Object> kNewUserCallbackExecutor("()V");
Method<void> kExecutorShutdown("shutdown", "()V");

void InitializeUserCallbackExecutor(Loader& loader) {
  loader.LoadClass(kUserCallbackExecutorClassName, kNewUserCallbackExecutor,
                   kExecutorShutdown);
}

constexpr char kFirestoreTasksClassName[] = PROGUARD_KEEP_CLASS
    "com/google/firebase/firestore/internal/cpp/FirestoreTasks";
StaticMethod<void> kAwaitCompletion("awaitCompletion",
                                    "(Lcom/google/android/gms/tasks/Task;)V");
StaticMethod<Task> kFailTaskWhenResultIsNull(
    "failTaskWhenResultIsNull",
    "(Lcom/google/android/gms/tasks/Task;Ljava/lang/String;)"
    "Lcom/google/android/gms/tasks/Task;");

void InitializeFirestoreTasks(Loader& loader) {
  loader.LoadClass(kFirestoreTasksClassName, kAwaitCompletion,
                   kFailTaskWhenResultIsNull);
}

/**
 * A map of Java Firestore instance to C++ Firestore instance.
 */
class JavaFirestoreMap {
 public:
  FirestoreInternal* Get(Env& env, const Object& java_firestore) {
    if (!java_firestore) {
      return nullptr;
    }
    MutexLock lock(mutex_);
    auto& map = GetMapLocked(env);
    if (!env.ok() || !map) {
      return nullptr;
    }
    Local<Long> boxed_ptr = map.Get(env, java_firestore).CastTo<Long>();
    if (!env.ok() || !boxed_ptr) {
      return nullptr;
    }
    return reinterpret_cast<FirestoreInternal*>(boxed_ptr.LongValue(env));
  }

  void Put(Env& env,
           const Object& java_firestore,
           FirestoreInternal* internal) {
    static_assert(sizeof(uintptr_t) <= sizeof(int64_t),
                  "pointers must fit in Java long");
    if (!java_firestore) {
      return;
    }
    MutexLock lock(mutex_);
    auto& map = GetMapLocked(env);
    if (!env.ok() || !map) {
      return;
    }
    auto boxed_ptr = Long::Create(env, reinterpret_cast<int64_t>(internal));
    if (!env.ok() || !boxed_ptr) {
      return;
    }
    map.Put(env, java_firestore, boxed_ptr);
  }

  void Remove(Env& env, const Object& java_firestore) {
    if (!java_firestore) {
      return;
    }
    MutexLock lock(mutex_);
    auto& map = GetMapLocked(env);
    if (!env.ok() || !map) {
      return;
    }
    map.Remove(env, java_firestore);
  }

 private:
  // Ensures that the backing map is initialized.
  // Prerequisite: `mutex_` must be locked before calling this method.
  HashMap& GetMapLocked(Env& env) {
    if (!firestores_) {
      Local<HashMap> created = HashMap::Create(env);
      if (env.ok() && created) {
        firestores_ = created;
      }
    }
    return firestores_;
  }

  Mutex mutex_;
  Global<HashMap> firestores_;
};

// init_mutex protects all the globals below.
Mutex init_mutex;  // NOLINT
int initialize_count = 0;
Loader* global_loader = nullptr;

JavaFirestoreMap* java_firestores = nullptr;

// The initial value for setLoggingEnabled.
enum class InitialLogState {
  kUnset,
  kSetEnabled,
  kSetDisabled,
} initial_log_state = InitialLogState::kUnset;

Local<LoadBundleTaskInternal> CreateLoadBundleTask(Env& env,
                                                   Object& firestore,
                                                   const std::string& bundle) {
  Local<Array<uint8_t>> java_bytes = env.NewArray<uint8_t>(bundle.size());
  if (!env.ok() || !java_bytes) {
    env.ExceptionClear();
    java_bytes = env.NewArray<uint8_t>(0);
  } else if (!bundle.empty()) {
    env.SetArrayRegion(java_bytes, 0, bundle.size(),
                       reinterpret_cast<const uint8_t*>(bundle.data()));
  }
  if (!env.ok() || !java_bytes) {
    return {};
  }
  return env.Call(firestore, kLoadBundle, java_bytes);
}

}  // namespace

const char kApiIdentifier[] = "Firestore";

FirestoreInternal::FirestoreInternal(App* app, const std::string& database_id) {
  FIREBASE_ASSERT(app != nullptr);
  if (!Initialize(app)) return;

  Env env = GetEnv();
  Local<Object> platform_app(env.get(), app->GetPlatformApp());
  Local<String> java_database_id = env.NewStringUtf(database_id);
  Local<Object> java_firestore =
      env.Call(kGetInstance, platform_app, java_database_id);
  Local<Object> java_user_callback_executor = env.New(kNewUserCallbackExecutor);
  if (!env.ok() || !java_firestore || !java_user_callback_executor) {
    Terminate(app);
    return;
  }

  app_ = app;
  database_name_ = database_id;
  obj_ = java_firestore;
  user_callback_executor_ = java_user_callback_executor;

  {
    MutexLock init_lock(init_mutex);
    if (java_firestores != nullptr) {
      java_firestores->Put(env, java_firestore, this);
    }
  }

  weak_target_ = std::make_shared<WeakTarget>(this);
  cleanup_.RegisterObject(weak_target_.get(), [](void* obj) {
    auto* target = static_cast<WeakTarget*>(obj);
    std::lock_guard<std::recursive_mutex> lock(target->mutex);
    target->firestore = nullptr;
  });

  promises_ = std::make_unique<PromiseFactory<AsyncFn>>(this);
  App::RegisterLibrary("fire-fst", kFirestoreVersionString, env.get());
}

/* static */
bool FirestoreInternal::Initialize(App* app) {
  MutexLock init_lock(init_mutex);
  if (initialize_count == 0) {
    jni::Initialize(app->java_vm());

    FIREBASE_DEV_ASSERT(java_firestores == nullptr);
    java_firestores = new JavaFirestoreMap();

    Env env = GetEnv();
    Loader loader(app);
    loader.AddEmbeddedFile(::firebase_firestore::firestore_resources_filename,
                           ::firebase_firestore::firestore_resources_data,
                           ::firebase_firestore::firestore_resources_size);
    loader.CacheEmbeddedFiles();

    jni::Object::Initialize(loader);
    jni::String::Initialize(env, loader);
    jni::ArrayList::Initialize(loader);
    jni::Boolean::Initialize(loader);
    jni::Collection::Initialize(loader);
    jni::Double::Initialize(loader);
    jni::Integer::Initialize(loader);
    jni::Iterator::Initialize(loader);
    jni::HashMap::Initialize(loader);
    jni::List::Initialize(loader);
    jni::Long::Initialize(loader);
    jni::Map::Initialize(loader);
    jni::ArenaRef::Initialize(loader);

    InitializeFirestore(loader);
    InitializeFirestoreTasks(loader);
    InitializeUserCallbackExecutor(loader);

    AggregateQueryInternal::Initialize(loader);
    AggregateQuerySnapshotInternal::Initialize(loader);
    AggregateSourceInternal::Initialize(loader);
    BlobInternal::Initialize(loader);
    CollectionReferenceInternal::Initialize(loader);
    DirectionInternal::Initialize(loader);
    DocumentChangeInternal::Initialize(loader);
    DocumentChangeTypeInternal::Initialize(loader);
    DocumentReferenceInternal::Initialize(loader);
    DocumentSnapshotInternal::Initialize(loader);
    EventListenerInternal::Initialize(loader);
    ExceptionInternal::Initialize(loader);
    FieldPathConverter::Initialize(loader);
    FieldValueInternal::Initialize(loader);
    FilterInternal::Initialize(loader);
    GeoPointInternal::Initialize(loader);
    JniRunnableBase::Initialize(loader);
    ListenerRegistrationInternal::Initialize(loader);
    MetadataChangesInternal::Initialize(loader);
    QueryInternal::Initialize(loader);
    QuerySnapshotInternal::Initialize(loader);
    ServerTimestampBehaviorInternal::Initialize(loader);
    SetOptionsInternal::Initialize(loader);
    SettingsInternal::Initialize(loader);
    SnapshotMetadataInternal::Initialize(loader);
    SourceInternal::Initialize(loader);
    Task::Initialize(loader);
    TimestampInternal::Initialize(loader);
    TransactionInternal::Initialize(loader);
    TransactionOptionsBuilderInternal::Initialize(loader);
    TransactionOptionsInternal::Initialize(loader);
    WriteBatchInternal::Initialize(loader);
    LoadBundleTaskInternal::Initialize(loader);
    LoadBundleTaskProgressInternal::Initialize(loader);
    if (!loader.ok()) {
      ReleaseClassesLocked(env);
      delete java_firestores;
      java_firestores = nullptr;
      return false;
    }

    FIREBASE_DEV_ASSERT(global_loader == nullptr);
    global_loader = new Loader(std::move(loader));

    if (initial_log_state != InitialLogState::kUnset) {
      ExceptionClearGuard exception_clear_guard(env);
      bool enabled = initial_log_state == InitialLogState::kSetEnabled;
      env.Call(kSetLoggingEnabled, enabled);
    }
  }
  initialize_count++;
  return true;
}

/* static */
void FirestoreInternal::ReleaseClassesLocked(Env& env) {
  // Assumes `init_mutex` is held.
  String::Terminate(env);

  delete global_loader;
  global_loader = nullptr;
}

/* static */
void FirestoreInternal::Terminate(App* app) {
  FIREBASE_ASSERT(app != nullptr);
  MutexLock init_lock(init_mutex);
  FIREBASE_ASSERT(initialize_count > 0);
  initialize_count--;
  if (initialize_count == 0) {
    Env env(app->GetJNIEnv());
    ExceptionClearGuard exception_clear_guard(env);
    ::firebase::util::CancelCallbacks(env.get(), kApiIdentifier);
    ReleaseClassesLocked(env);

    delete java_firestores;
    java_firestores = nullptr;
  }
}

void FirestoreInternal::ShutdownUserCallbackExecutor(Env& env) {
  if (user_callback_executor_) {
    ExceptionClearGuard exception_clear_guard(env);
    env.Call(user_callback_executor_, kExecutorShutdown);
    user_callback_executor_.clear();
  }
}

FirestoreInternal::~FirestoreInternal() {
  // If initialization failed, there is nothing to clean up.
  if (app_ == nullptr) return;

  ClearListeners();

  Env env = GetEnv();
  ExceptionClearGuard exception_clear_guard(env);

  ClearBundleListeners(env);

  // Call `terminate()` on the Java `FirebaseFirestore` object and wait for it
  // to complete to guarantee that the next instance of `FirestoreInternal` will
  // be backed by a new Java `FirebaseFirestore` instance.
  Local<Task> terminate_task = env.Call(obj_, kTerminate);
  if (env.ok() && terminate_task) {
    env.Call(kAwaitCompletion, terminate_task);
  }
  env.ExceptionClear();

  ShutdownUserCallbackExecutor(env);

  if (weak_target_) {
    cleanup_.UnregisterObject(weak_target_.get());
    std::lock_guard<std::recursive_mutex> lock(weak_target_->mutex);
    weak_target_->firestore = nullptr;
  }

  promises_.reset(nullptr);

  {
    MutexLock init_lock(init_mutex);
    if (java_firestores != nullptr) {
      java_firestores->Remove(env, obj_);
    }
  }
  obj_.clear();

  Terminate(app_);
  app_ = nullptr;
}

CollectionReference FirestoreInternal::Collection(
    const char* collection_path) const {
  if (collection_path == nullptr || collection_path[0] == '\0') {
    return {};
  }
  Env env = GetEnv();
  Local<String> java_path = env.NewStringUtf(collection_path);
  Local<Object> result = env.Call(obj_, kCollection, java_path);
  return NewCollectionReference(env, result);
}

DocumentReference FirestoreInternal::Document(const char* document_path) const {
  if (document_path == nullptr || document_path[0] == '\0') {
    return {};
  }
  Env env = GetEnv();
  Local<String> java_path = env.NewStringUtf(document_path);
  Local<Object> result = env.Call(obj_, kDocument, java_path);
  return NewDocumentReference(env, result);
}

Query FirestoreInternal::CollectionGroup(const char* collection_id) const {
  if (collection_id == nullptr || collection_id[0] == '\0') {
    return {};
  }
  Env env = GetEnv();
  Local<String> java_collection_id = env.NewStringUtf(collection_id);
  Local<Object> query = env.Call(obj_, kCollectionGroup, java_collection_id);
  return NewQuery(env, query);
}

Settings FirestoreInternal::settings() const {
  Env env = GetEnv();
  Local<SettingsInternal> settings = env.Call(obj_, kGetSettings);

  if (!env.ok() || !settings) return {};
  return settings.ToPublic(env);
}

void FirestoreInternal::set_settings(const Settings& settings) {
  Env env = GetEnv();
  auto java_settings = SettingsInternal::Create(env, settings);
  if (!env.ok() || !java_settings) return;
  env.Call(obj_, kSetSettings, java_settings);
}

WriteBatch FirestoreInternal::batch() const {
  Env env = GetEnv();
  Local<Object> result = env.Call(obj_, kBatch);

  if (!env.ok() || !result) return {};
  return WriteBatch(new WriteBatchInternal(mutable_this(), result));
}

Future<void> FirestoreInternal::RunTransaction(
    std::function<Error(Transaction&, std::string&)> update,
    int32_t max_attempts) {
  SIMPLE_HARD_ASSERT(max_attempts > 0);

  auto* lambda_update = new LambdaTransactionFunction(std::move(update));
  Env env = GetEnv();
  Local<Object> transaction_function =
      TransactionInternal::Create(env, this, lambda_update);

  Local<TransactionOptionsBuilderInternal> options_builder =
      TransactionOptionsBuilderInternal::Create(env);
  options_builder.SetMaxAttempts(env, max_attempts);
  Local<TransactionOptionsInternal> options = options_builder.Build(env);
  options_builder.clear();

  Local<Task> task =
      env.Call(obj_, kRunTransaction, options, transaction_function);

  if (!env.ok() || !task) {
    delete lambda_update;
    return {};
  }

  return promises_->NewFuture<void>(env, AsyncFn::kRunTransaction, task,
                                    lambda_update);
}

Future<void> FirestoreInternal::DisableNetwork() {
  Env env = GetEnv();
  Local<Task> task = env.Call(obj_, kDisableNetwork);
  return promises_->NewFuture<void>(env, AsyncFn::kDisableNetwork, task);
}

Future<void> FirestoreInternal::EnableNetwork() {
  Env env = GetEnv();
  Local<Task> task = env.Call(obj_, kEnableNetwork);
  return promises_->NewFuture<void>(env, AsyncFn::kEnableNetwork, task);
}

Future<void> FirestoreInternal::Terminate() {
  Env env = GetEnv();
  Local<Task> task = env.Call(obj_, kTerminate);
  return promises_->NewFuture<void>(env, AsyncFn::kTerminate, task);
}

Future<void> FirestoreInternal::WaitForPendingWrites() {
  Env env = GetEnv();
  Local<Task> task = env.Call(obj_, kWaitForPendingWrites);
  return promises_->NewFuture<void>(env, AsyncFn::kWaitForPendingWrites, task);
}

Future<void> FirestoreInternal::ClearPersistence() {
  Env env = GetEnv();
  Local<Task> task = env.Call(obj_, kClearPersistence);
  return promises_->NewFuture<void>(env, AsyncFn::kClearPersistence, task);
}

ListenerRegistration FirestoreInternal::AddSnapshotsInSyncListener(
    EventListener<void>* listener, bool passing_listener_ownership) {
  Env env = GetEnv();
  Local<Object> java_runnable = EventListenerInternal::Create(env, listener);

  Local<Object> java_registration =
      env.Call(obj_, kAddSnapshotsInSyncListener, user_callback_executor(),
               java_runnable);

  if (!env.ok() || !java_registration) {
    if (passing_listener_ownership) {
      delete listener;
    }
    return {};
  }
  return ListenerRegistration(new ListenerRegistrationInternal(
      this, listener, passing_listener_ownership, java_registration,
      java_runnable));
}

ListenerRegistration FirestoreInternal::AddSnapshotsInSyncListener(
    std::function<void()> callback) {
  auto* listener = new LambdaEventListener<void>(std::move(callback));
  return AddSnapshotsInSyncListener(listener,
                                    /*passing_listener_ownership=*/true);
}

void FirestoreInternal::RegisterListenerRegistration(
    ListenerRegistrationInternal* registration) {
  MutexLock lock(listener_registration_mutex_);
  listener_registrations_.insert(registration);
}

void FirestoreInternal::UnregisterListenerRegistration(
    ListenerRegistrationInternal* registration) {
  ListenerRegistrationInternal* to_delete = nullptr;
  {
    MutexLock lock(listener_registration_mutex_);
    auto iter = listener_registrations_.find(registration);
    if (iter != listener_registrations_.end()) {
      to_delete = *iter;
      listener_registrations_.erase(iter);
    }
  }
  delete to_delete;
}

void FirestoreInternal::ClearListeners() {
  std::unordered_set<ListenerRegistrationInternal*> registrations;
  {
    MutexLock lock(listener_registration_mutex_);
    registrations.swap(listener_registrations_);
  }
  for (auto* reg : registrations) {
    delete reg;
  }
}

void FirestoreInternal::RegisterBundleListener(
    LoadBundleProgressCompletion* listener) {
  MutexLock lock(bundle_listeners_mutex_);
  bundle_listeners_.insert(listener);
}

void FirestoreInternal::UnregisterBundleListener(
    LoadBundleProgressCompletion* listener) {
  MutexLock lock(bundle_listeners_mutex_);
  bundle_listeners_.erase(listener);
}

void FirestoreInternal::ClearBundleListeners(Env& env) {
  std::vector<LoadBundleProgressCompletion*> listeners;
  {
    MutexLock lock(bundle_listeners_mutex_);
    listeners.assign(bundle_listeners_.begin(), bundle_listeners_.end());
    bundle_listeners_.clear();
  }
  for (auto* listener : listeners) {
    listener->DiscardJavaListener(env);
  }
}

Future<LoadBundleTaskProgress> FirestoreInternal::LoadBundle(
    const std::string& bundle) {
  Env env = GetEnv();
  Local<LoadBundleTaskInternal> task = CreateLoadBundleTask(env, obj_, bundle);
  return promises_->NewFuture<LoadBundleTaskProgress>(env, AsyncFn::kLoadBundle,
                                                      task);
}

Future<LoadBundleTaskProgress> FirestoreInternal::LoadBundle(
    const std::string& bundle,
    std::function<void(const LoadBundleTaskProgress&)> progress_callback) {
  FIREBASE_ASSERT(progress_callback);
  Env env = GetEnv();
  Local<LoadBundleTaskInternal> task = CreateLoadBundleTask(env, obj_, bundle);
  if (!env.ok() || !task) {
    return {};
  }

  auto* completion =
      new LoadBundleProgressCompletion(this, std::move(progress_callback));
  Local<Object> progress_listener =
      EventListenerInternal::Create(env, this, completion);
  if (!env.ok() || !progress_listener) {
    delete completion;
    return {};
  }
  completion->SetJavaListener(progress_listener);
  RegisterBundleListener(completion);
  task.AddProgressListener(env, user_callback_executor(), progress_listener);

  return promises_->NewFuture<LoadBundleTaskProgress>(
      env, AsyncFn::kLoadBundle, task, completion);
}

Future<Query> FirestoreInternal::NamedQuery(const std::string& query_name) {
  Env env = GetEnv();
  Local<String> name = env.NewStringUtf(query_name);
  Local<Task> task = env.Call(obj_, kGetNamedQuery, name);
  Local<Task> null_checked_task =
      env.Call(kFailTaskWhenResultIsNull, task,
               env.NewStringUtf("Failed to find named query."));
  return promises_->NewFuture<Query>(env, AsyncFn::kGetNamedQuery,
                                     null_checked_task);
}

Env FirestoreInternal::GetEnv() {
  Env env;
  env.SetUnhandledExceptionHandler(GlobalUnhandledExceptionHandler, nullptr);
  return env;
}

AggregateQuery FirestoreInternal::NewAggregateQuery(
    Env& env, const jni::Object& aggregate_query) const {
  return MakePublic<AggregateQuery>(env, mutable_this(), aggregate_query);
}

AggregateQuerySnapshot FirestoreInternal::NewAggregateQuerySnapshot(
    Env& env, const jni::Object& aggregate_query_snapshot) const {
  return MakePublic<AggregateQuerySnapshot>(env, mutable_this(),
                                            aggregate_query_snapshot);
}

CollectionReference FirestoreInternal::NewCollectionReference(
    Env& env, const jni::Object& reference) const {
  return MakePublic<CollectionReference>(env, mutable_this(), reference);
}

DocumentReference FirestoreInternal::NewDocumentReference(
    Env& env, const jni::Object& reference) const {
  return MakePublic<DocumentReference>(env, mutable_this(), reference);
}

DocumentSnapshot FirestoreInternal::NewDocumentSnapshot(
    Env& env, const jni::Object& snapshot) const {
  return MakePublic<DocumentSnapshot>(env, mutable_this(), snapshot);
}

Query FirestoreInternal::NewQuery(Env& env, const jni::Object& query) const {
  return MakePublic<Query>(env, mutable_this(), query);
}

QuerySnapshot FirestoreInternal::NewQuerySnapshot(
    Env& env, const jni::Object& snapshot) const {
  return MakePublic<QuerySnapshot>(env, mutable_this(), snapshot);
}

/* static */
void Firestore::set_log_level(LogLevel log_level) {
  // "Verbose" and "debug" map to logging enabled.
  // "Info", "warning", "error", and "assert" map to logging disabled.
  bool logging_enabled = log_level < LogLevel::kLogLevelInfo;

  MutexLock lock(init_mutex);

  // Set the initial_log_state on every invocation, just in case Firestore
  // is terminated for long enough to unload the Firestore classes within
  // the JVM.
  initial_log_state = logging_enabled ? InitialLogState::kSetEnabled
                                      : InitialLogState::kSetDisabled;

  if (initialize_count < 1) {
    // Avoid invoking Java methods before Firestore has been initialized.
    return;
  }

  Env env = FirestoreInternal::GetEnv();
  ExceptionClearGuard exception_clear_guard(env);
  env.Call(kSetLoggingEnabled, logging_enabled);
}

FirestoreInternal* FirestoreInternal::RecoverFirestore(
    Env& env, const Object& java_firestore) {
  MutexLock lock(init_mutex);
  if (java_firestores == nullptr || !java_firestore) {
    return nullptr;
  }
  return java_firestores->Get(env, java_firestore);
}

void FirestoreInternal::SetClientLanguage(const std::string& language_token) {
  MutexLock lock(init_mutex);
  if (initialize_count < 1) {
    return;
  }
  Env env = FirestoreInternal::GetEnv();
  ExceptionClearGuard exception_clear_guard(env);
  Local<String> java_language_token = env.NewStringUtf(language_token);
  env.Call(kSetClientLanguage, java_language_token);
}

}  // namespace firestore
}  // namespace firebase
