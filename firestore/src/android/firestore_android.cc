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

#include "app/meta/move.h"
#include "app/src/assert.h"
#include "app/src/embedded_file.h"
#include "app/src/include/firebase/future.h"
#include "app/src/reference_counted_future_impl.h"
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
#include "firestore/src/jni/set.h"
#include "firestore/src/jni/task.h"

namespace firebase {
namespace firestore {
namespace {

using jni::Array;
using jni::Constructor;
using jni::Env;
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
    "(Lcom/google/firebase/FirebaseApp;)"
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
    MutexLock lock(mutex_);
    auto& map = GetMapLocked(env);
    Local<Long> boxed_ptr = map.Get(env, java_firestore).CastTo<Long>();
    if (!boxed_ptr) {
      return nullptr;
    }
    return reinterpret_cast<FirestoreInternal*>(boxed_ptr.LongValue(env));
  }

  void Put(Env& env,
           const Object& java_firestore,
           FirestoreInternal* internal) {
    static_assert(sizeof(uintptr_t) <= sizeof(int64_t),
                  "pointers must fit in Java long");

    MutexLock lock(mutex_);
    auto& map = GetMapLocked(env);
    auto boxed_ptr = Long::Create(env, reinterpret_cast<int64_t>(internal));
    map.Put(env, java_firestore, boxed_ptr);
  }

  void Remove(Env& env, const Object& java_firestore) {
    MutexLock lock(mutex_);
    auto& map = GetMapLocked(env);
    map.Remove(env, java_firestore);
  }

 private:
  // Ensures that the backing map is initialized.
  // Prerequisite: `mutex_` must be locked before calling this method.
  HashMap& GetMapLocked(Env& env) {
    if (!firestores_) {
      firestores_ = HashMap::Create(env);
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
  env.SetArrayRegion(java_bytes, 0, bundle.size(),
                     reinterpret_cast<const uint8_t*>(bundle.c_str()));
  if (!env.ok()) {
    java_bytes = env.NewArray<uint8_t>(0);
  }
  return env.Call(firestore, kLoadBundle, java_bytes);
}

}  // namespace

const char kApiIdentifier[] = "Firestore";

FirestoreInternal::FirestoreInternal(App* app) {
  FIREBASE_ASSERT(app != nullptr);
  if (!Initialize(app)) return;
  app_ = app;

  Env env = GetEnv();
  Local<Object> platform_app(env.get(), app_->GetPlatformApp());
  Local<Object> java_firestore = env.Call(kGetInstance, platform_app);
  FIREBASE_ASSERT(java_firestore.get() != nullptr);
  obj_ = java_firestore;

  java_firestores->Put(env, java_firestore, this);

  // Mainly for enabling TimestampsInSnapshotsEnabled. The rest comes from the
  // default in native SDK. The C++ implementation relies on that for reading
  // timestamp FieldValues correctly. TODO(zxu): Once it is set to true by
  // default, we may safely remove the calls below.
  set_settings(settings());

  Local<Object> java_user_callback_executor = env.New(kNewUserCallbackExecutor);

  FIREBASE_ASSERT(java_user_callback_executor.get() != nullptr);
  user_callback_executor_ = java_user_callback_executor;

  promises_ = std::make_unique<PromiseFactory<AsyncFn>>(this);
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
      return false;
    }

    FIREBASE_DEV_ASSERT(global_loader == nullptr);
    global_loader = new Loader(Move(loader));

    if (initial_log_state != InitialLogState::kUnset) {
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
  MutexLock init_lock(init_mutex);
  FIREBASE_ASSERT(initialize_count > 0);
  initialize_count--;
  if (initialize_count == 0) {
    Env env(app->GetJNIEnv());
    ReleaseClassesLocked(env);

    delete java_firestores;
    java_firestores = nullptr;
  }
}

void FirestoreInternal::ShutdownUserCallbackExecutor(Env& env) {
  env.Call(user_callback_executor_, kExecutorShutdown);
}

FirestoreInternal::~FirestoreInternal() {
  // If initialization failed, there is nothing to clean up.
  if (app_ == nullptr) return;

  ClearListeners();

  Env env = GetEnv();

  // Call `terminate()` on the Java `FirebaseFirestore` object and wait for it
  // to complete to guarantee that the next instance of `FirestoreInternal` will
  // be backed by a new Java `FirebaseFirestore` instance.
  Local<Task> terminate_task = env.Call(obj_, kTerminate);
  env.Call(kAwaitCompletion, terminate_task);

  ShutdownUserCallbackExecutor(env);

  promises_.reset(nullptr);

  java_firestores->Remove(env, obj_);

  Terminate(app_);
  app_ = nullptr;
}

CollectionReference FirestoreInternal::Collection(
    const char* collection_path) const {
  Env env = GetEnv();
  Local<String> java_path = env.NewStringUtf(collection_path);
  Local<Object> result = env.Call(obj_, kCollection, java_path);
  return NewCollectionReference(env, result);
}

DocumentReference FirestoreInternal::Document(const char* document_path) const {
  Env env = GetEnv();
  Local<String> java_path = env.NewStringUtf(document_path);
  Local<Object> result = env.Call(obj_, kDocument, java_path);
  return NewDocumentReference(env, result);
}

Query FirestoreInternal::CollectionGroup(const char* collection_id) const {
  Env env = GetEnv();
  Local<String> java_collection_id = env.NewStringUtf(collection_id);
  Local<Object> query = env.Call(obj_, kCollectionGroup, java_collection_id);
  return NewQuery(env, query);
}

Settings FirestoreInternal::settings() const {
  Env env = GetEnv();
  Local<SettingsInternal> settings = env.Call(obj_, kGetSettings);

  if (!env.ok()) return {};
  return settings.ToPublic(env);
}

void FirestoreInternal::set_settings(Settings settings) {
  Env env = GetEnv();
  auto java_settings = SettingsInternal::Create(env, settings);
  env.Call(obj_, kSetSettings, java_settings);
}

WriteBatch FirestoreInternal::batch() const {
  Env env = GetEnv();
  Local<Object> result = env.Call(obj_, kBatch);

  if (!env.ok()) return {};
  return WriteBatch(new WriteBatchInternal(mutable_this(), result));
}

Future<void> FirestoreInternal::RunTransaction(
    std::function<Error(Transaction&, std::string&)> update,
    int32_t max_attempts) {
  SIMPLE_HARD_ASSERT(max_attempts > 0);

  auto* lambda_update = new LambdaTransactionFunction(Move(update));
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

  if (!env.ok()) return {};

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

  if (!env.ok() || !java_registration) return {};
  return ListenerRegistration(new ListenerRegistrationInternal(
      this, listener, passing_listener_ownership, java_registration));
}

ListenerRegistration FirestoreInternal::AddSnapshotsInSyncListener(
    std::function<void()> callback) {
  auto* listener = new LambdaEventListener<void>(firebase::Move(callback));
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
  MutexLock lock(listener_registration_mutex_);
  auto iter = listener_registrations_.find(registration);
  if (iter != listener_registrations_.end()) {
    delete *iter;
    listener_registrations_.erase(iter);
  }
}

void FirestoreInternal::ClearListeners() {
  MutexLock lock(listener_registration_mutex_);

  for (auto* reg : listener_registrations_) {
    delete reg;
  }
  listener_registrations_.clear();
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
  Env env = GetEnv();
  Local<LoadBundleTaskInternal> task = CreateLoadBundleTask(env, obj_, bundle);

  LambdaEventListener<LoadBundleTaskProgress> listener(
      // TODO(C++14): Move the callback into the lambda.
      [progress_callback](const LoadBundleTaskProgress& p, Error,
                          const std::string&) {
        // Error is always `kErrorOk` and error message is always empty for
        // progress listeners.
        progress_callback(p);
      });

  EventListener<LoadBundleTaskProgress>* listener_ptr = nullptr;
  // TODO(b/187420421): The listener is owned by the firestore instance, longer
  // than ideal. This is to support the unlikely case when user try to delete
  // Firestore instance in the listener. Once the referred bug is fixed, it can
  // be managed through a `shared_ptr`.
  {
    MutexLock lock(bundle_listeners_mutex_);
    bundle_listeners_.push_back(std::move(listener));
    listener_ptr = &(bundle_listeners_.back());
  }

  Local<Object> progress_listener =
      EventListenerInternal::Create(env, this, listener_ptr);
  task.AddProgressListener(env, user_callback_executor(), progress_listener);

  return promises_->NewFuture<LoadBundleTaskProgress>(env, AsyncFn::kLoadBundle,
                                                      task);
}

Future<Query> FirestoreInternal::NamedQuery(const std::string& query_name) {
  Env env = GetEnv();
  Local<String> name = env.NewStringUtf(query_name.c_str());
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

  {
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
  }

  Env env = FirestoreInternal::GetEnv();
  env.Call(kSetLoggingEnabled, logging_enabled);
}

FirestoreInternal* FirestoreInternal::RecoverFirestore(
    Env& env, const Object& java_firestore) {
  return java_firestores->Get(env, java_firestore);
}

void FirestoreInternal::SetClientLanguage(const std::string& language_token) {
  Env env = FirestoreInternal::GetEnv();
  Local<String> java_language_token = env.NewStringUtf(language_token);
  env.Call(kSetClientLanguage, java_language_token);
}

}  // namespace firestore
}  // namespace firebase
