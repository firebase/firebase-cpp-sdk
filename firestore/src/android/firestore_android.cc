// Copyright 2016 Google Inc. All Rights Reserved.

#include "firestore/src/android/firestore_android.h"

#include "app/meta/move.h"
#include "app/src/assert.h"
#include "app/src/embedded_file.h"
#include "app/src/include/firebase/future.h"
#include "app/src/reference_counted_future_impl.h"
#include "app/src/util_android.h"
#include "firestore/firestore_resources.h"
#include "firestore/src/android/blob_android.h"
#include "firestore/src/android/collection_reference_android.h"
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
#include "firestore/src/android/lambda_event_listener.h"
#include "firestore/src/android/lambda_transaction_function.h"
#include "firestore/src/android/listener_registration_android.h"
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
#include "firestore/src/android/util_android.h"
#include "firestore/src/android/wrapper.h"
#include "firestore/src/android/write_batch_android.h"
#include "firestore/src/include/firebase/firestore.h"
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

namespace firebase {
namespace firestore {
namespace {

using jni::Constructor;
using jni::Env;
using jni::Loader;
using jni::Local;
using jni::Method;
using jni::Object;
using jni::StaticMethod;
using jni::String;

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
Method<Object> kRunTransaction(
    "runTransaction",
    "(Lcom/google/firebase/firestore/Transaction$Function;)"
    "Lcom/google/android/gms/tasks/Task;");
Method<Object> kEnableNetwork("enableNetwork",
                              "()Lcom/google/android/gms/tasks/Task;");
Method<Object> kDisableNetwork("disableNetwork",
                               "()Lcom/google/android/gms/tasks/Task;");
Method<Object> kTerminate("terminate", "()Lcom/google/android/gms/tasks/Task;");
Method<Object> kWaitForPendingWrites("waitForPendingWrites",
                                     "()Lcom/google/android/gms/tasks/Task;");
Method<Object> kClearPersistence("clearPersistence",
                                 "()Lcom/google/android/gms/tasks/Task;");
Method<Object> kAddSnapshotsInSyncListener(
    "addSnapshotsInSyncListener",
    "(Ljava/util/concurrent/Executor;Ljava/lang/Runnable;)"
    "Lcom/google/firebase/firestore/ListenerRegistration;");

void InitializeFirestore(Loader& loader) {
  loader.LoadClass(kFirestoreClassName, kCollection, kDocument,
                   kCollectionGroup, kGetSettings, kGetInstance,
                   kSetLoggingEnabled, kSetClientLanguage, kSetSettings, kBatch,
                   kRunTransaction, kEnableNetwork, kDisableNetwork, kTerminate,
                   kWaitForPendingWrites, kClearPersistence,
                   kAddSnapshotsInSyncListener);
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

}  // namespace

const char kApiIdentifier[] = "Firestore";

Mutex FirestoreInternal::init_mutex_;  // NOLINT
int FirestoreInternal::initialize_count_ = 0;
Loader* FirestoreInternal::loader_ = nullptr;

FirestoreInternal::FirestoreInternal(App* app) {
  FIREBASE_ASSERT(app != nullptr);
  if (!Initialize(app)) return;
  app_ = app;

  Env env = GetEnv();
  Local<Object> platform_app(env.get(), app_->GetPlatformApp());
  Local<Object> java_firestore = env.Call(kGetInstance, platform_app);
  FIREBASE_ASSERT(java_firestore.get() != nullptr);
  obj_ = java_firestore;

  // Mainly for enabling TimestampsInSnapshotsEnabled. The rest comes from the
  // default in native SDK. The C++ implementation relies on that for reading
  // timestamp FieldValues correctly. TODO(zxu): Once it is set to true by
  // default, we may safely remove the calls below.
  set_settings(settings());

  Local<Object> java_user_callback_executor = env.New(kNewUserCallbackExecutor);

  FIREBASE_ASSERT(java_user_callback_executor.get() != nullptr);
  user_callback_executor_ = java_user_callback_executor;

  future_manager_.AllocFutureApi(this, static_cast<int>(AsyncFn::kCount));
}

/* static */
bool FirestoreInternal::Initialize(App* app) {
  MutexLock init_lock(init_mutex_);
  if (initialize_count_ == 0) {
    jni::Initialize(app->java_vm());

    Loader loader(app);
    loader.AddEmbeddedFile(::firebase_firestore::firestore_resources_filename,
                           ::firebase_firestore::firestore_resources_data,
                           ::firebase_firestore::firestore_resources_size);
    loader.CacheEmbeddedFiles();

    if (!FieldValueInternal::Initialize(app)) {
      ReleaseClasses(app);
      return false;
    }

    jni::Object::Initialize(loader);

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
    InitializeUserCallbackExecutor(loader);

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
    GeoPointInternal::Initialize(loader);
    ListenerRegistrationInternal::Initialize(loader);
    MetadataChangesInternal::Initialize(loader);
    QueryInternal::Initialize(loader);
    QuerySnapshotInternal::Initialize(loader);
    ServerTimestampBehaviorInternal::Initialize(loader);
    SetOptionsInternal::Initialize(loader);
    SettingsInternal::Initialize(loader);
    SnapshotMetadataInternal::Initialize(loader);
    SourceInternal::Initialize(loader);
    TimestampInternal::Initialize(loader);
    TransactionInternal::Initialize(loader);
    WriteBatchInternal::Initialize(loader);
    if (!loader.ok()) {
      ReleaseClasses(app);
      return false;
    }

    FIREBASE_DEV_ASSERT(loader_ == nullptr);
    loader_ = new Loader(Move(loader));
  }
  initialize_count_++;
  return true;
}

/* static */
void FirestoreInternal::ReleaseClasses(App* app) {
  delete loader_;
  loader_ = nullptr;

  // Call Terminate on each Firestore internal class.
  FieldValueInternal::Terminate(app);
}

/* static */
void FirestoreInternal::Terminate(App* app) {
  MutexLock init_lock(init_mutex_);
  FIREBASE_ASSERT(initialize_count_ > 0);
  initialize_count_--;
  if (initialize_count_ == 0) {
    ReleaseClasses(app);
  }
}

void FirestoreInternal::ShutdownUserCallbackExecutor(Env& env) {
  env.Call(user_callback_executor_, kExecutorShutdown);
}

FirestoreInternal::~FirestoreInternal() {
  // If initialization failed, there is nothing to clean up.
  if (app_ == nullptr) return;

  {
    MutexLock lock(listener_registration_mutex_);
    // TODO(varconst): investigate why not all listener registrations are
    // cleared.
    // FIREBASE_ASSERT(listener_registrations_.empty());
    for (auto* reg : listener_registrations_) {
      delete reg;
    }
    listener_registrations_.clear();
  }

  Env env = GetEnv();
  ShutdownUserCallbackExecutor(env);

  future_manager_.ReleaseFutureApi(this);

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
  return WriteBatch(new WriteBatchInternal(mutable_this(), result.get()));
}

Future<void> FirestoreInternal::RunTransaction(TransactionFunction* update,
                                               bool is_lambda) {
  Env env = GetEnv();
  Local<Object> transaction_function =
      TransactionInternal::Create(env, this, update);
  Local<Object> task = env.Call(obj_, kRunTransaction, transaction_function);

  if (!env.ok()) return {};

#if defined(FIREBASE_USE_STD_FUNCTION) || defined(DOXYGEN)
  auto* completion =
      static_cast<LambdaTransactionFunction*>(is_lambda ? update : nullptr);
  Promise<void, void, AsyncFn> promise(ref_future(), this, completion);
#else  // defined(FIREBASE_USE_STD_FUNCTION) || defined(DOXYGEN)
  Promise<void, void, AsyncFn> promise(ref_future(), this);
#endif  // defined(FIREBASE_USE_STD_FUNCTION) || defined(DOXYGEN)

  promise.RegisterForTask(env, AsyncFn::kRunTransaction, task);
  return promise.GetFuture();
}

#if defined(FIREBASE_USE_STD_FUNCTION) || defined(DOXYGEN)
Future<void> FirestoreInternal::RunTransaction(
    std::function<Error(Transaction&, std::string&)> update) {
  auto* lambda_update = new LambdaTransactionFunction(Move(update));
  return RunTransaction(lambda_update, /*is_lambda=*/true);
}
#endif  // defined(FIREBASE_USE_STD_FUNCTION) || defined(DOXYGEN)

Future<void> FirestoreInternal::DisableNetwork() {
  Env env = GetEnv();
  Local<Object> task = env.Call(obj_, kDisableNetwork);
  return NewFuture(env, AsyncFn::kDisableNetwork, task);
}

Future<void> FirestoreInternal::EnableNetwork() {
  Env env = GetEnv();
  Local<Object> task = env.Call(obj_, kEnableNetwork);
  return NewFuture(env, AsyncFn::kEnableNetwork, task);
}

Future<void> FirestoreInternal::Terminate() {
  Env env = GetEnv();
  Local<Object> task = env.Call(obj_, kTerminate);
  return NewFuture(env, AsyncFn::kTerminate, task);
}

Future<void> FirestoreInternal::WaitForPendingWrites() {
  Env env = GetEnv();
  Local<Object> task = env.Call(obj_, kWaitForPendingWrites);
  return NewFuture(env, AsyncFn::kWaitForPendingWrites, task);
}

Future<void> FirestoreInternal::ClearPersistence() {
  Env env = GetEnv();
  Local<Object> task = env.Call(obj_, kClearPersistence);
  return NewFuture(env, AsyncFn::kClearPersistence, task);
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

#if defined(FIREBASE_USE_STD_FUNCTION)

ListenerRegistration FirestoreInternal::AddSnapshotsInSyncListener(
    std::function<void()> callback) {
  auto* listener = new LambdaEventListener<void>(firebase::Move(callback));
  return AddSnapshotsInSyncListener(listener,
                                    /*passing_listener_ownership=*/true);
}

#endif  // defined(FIREBASE_USE_STD_FUNCTION)

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

jni::Env FirestoreInternal::GetEnv() {
  jni::Env env;
  env.SetUnhandledExceptionHandler(GlobalUnhandledExceptionHandler, nullptr);
  return env;
}

CollectionReference FirestoreInternal::NewCollectionReference(
    jni::Env& env, const jni::Object& reference) const {
  if (!env.ok() || !reference) return {};

  return CollectionReference(
      new CollectionReferenceInternal(mutable_this(), reference.get()));
}

DocumentReference FirestoreInternal::NewDocumentReference(
    jni::Env& env, const jni::Object& reference) const {
  if (!env.ok() || !reference) return {};

  return DocumentReference(
      new DocumentReferenceInternal(mutable_this(), reference.get()));
}

DocumentSnapshot FirestoreInternal::NewDocumentSnapshot(
    jni::Env& env, const jni::Object& snapshot) const {
  if (!env.ok() || !snapshot) return {};

  return DocumentSnapshot(
      new DocumentSnapshotInternal(mutable_this(), snapshot.get()));
}

Query FirestoreInternal::NewQuery(jni::Env& env,
                                  const jni::Object& query) const {
  if (!env.ok() || !query) return {};
  return Query(new QueryInternal(mutable_this(), query.get()));
}

QuerySnapshot FirestoreInternal::NewQuerySnapshot(
    jni::Env& env, const jni::Object& snapshot) const {
  if (!env.ok() || !snapshot) return {};
  return QuerySnapshot(
      new QuerySnapshotInternal(mutable_this(), snapshot.get()));
}

/* static */
void Firestore::set_log_level(LogLevel log_level) {
  // "Verbose" and "debug" map to logging enabled.
  // "Info", "warning", "error", and "assert" map to logging disabled.
  bool logging_enabled = log_level < LogLevel::kLogLevelInfo;

  Env env = FirestoreInternal::GetEnv();
  env.Call(kSetLoggingEnabled, logging_enabled);
}

void FirestoreInternal::SetClientLanguage(const std::string& language_token) {
  Env env = FirestoreInternal::GetEnv();
  Local<String> java_language_token = env.NewStringUtf(language_token);
  env.Call(kSetClientLanguage, java_language_token);
}

}  // namespace firestore
}  // namespace firebase
