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
#include "firestore/src/android/field_path_android.h"
#include "firestore/src/android/field_value_android.h"
#include "firestore/src/android/firebase_firestore_exception_android.h"
#include "firestore/src/android/firebase_firestore_settings_android.h"
#include "firestore/src/android/geo_point_android.h"
#include "firestore/src/android/lambda_event_listener.h"
#include "firestore/src/android/lambda_transaction_function.h"
#include "firestore/src/android/metadata_changes_android.h"
#include "firestore/src/android/promise_android.h"
#include "firestore/src/android/query_android.h"
#include "firestore/src/android/query_snapshot_android.h"
#include "firestore/src/android/server_timestamp_behavior_android.h"
#include "firestore/src/android/set_options_android.h"
#include "firestore/src/android/snapshot_metadata_android.h"
#include "firestore/src/android/source_android.h"
#include "firestore/src/android/timestamp_android.h"
#include "firestore/src/android/transaction_android.h"
#include "firestore/src/android/util_android.h"
#include "firestore/src/android/wrapper.h"
#include "firestore/src/android/write_batch_android.h"
#include "firestore/src/include/firebase/firestore.h"
#include "firestore/src/jni/env.h"
#include "firestore/src/jni/jni.h"
#include "firestore/src/jni/loader.h"

namespace firebase {
namespace firestore {

const char kApiIdentifier[] = "Firestore";

// clang-format off
#define FIREBASE_FIRESTORE_METHODS(X)                                   \
  X(Collection, "collection",                                           \
    "(Ljava/lang/String;)"                                              \
    "Lcom/google/firebase/firestore/CollectionReference;"),             \
  X(Document, "document",                                               \
    "(Ljava/lang/String;)"                                              \
    "Lcom/google/firebase/firestore/DocumentReference;"),               \
  X(CollectionGroup, "collectionGroup",                                 \
    "(Ljava/lang/String;)"                                              \
    "Lcom/google/firebase/firestore/Query;"),                           \
  X(GetSettings, "getFirestoreSettings",                                \
    "()Lcom/google/firebase/firestore/FirebaseFirestoreSettings;"),     \
  X(GetInstance, "getInstance",                                         \
    "(Lcom/google/firebase/FirebaseApp;)"                               \
    "Lcom/google/firebase/firestore/FirebaseFirestore;",                \
    util::kMethodTypeStatic),                                           \
  X(SetLoggingEnabled, "setLoggingEnabled",                             \
    "(Z)V", util::kMethodTypeStatic),                                   \
  X(SetSettings, "setFirestoreSettings",                                \
    "(Lcom/google/firebase/firestore/FirebaseFirestoreSettings;)V"),    \
  X(Batch, "batch",                                                     \
    "()Lcom/google/firebase/firestore/WriteBatch;"),                    \
  X(RunTransaction, "runTransaction",                                   \
    "(Lcom/google/firebase/firestore/Transaction$Function;)"            \
    "Lcom/google/android/gms/tasks/Task;"),                             \
  X(EnableNetwork, "enableNetwork",                                     \
    "()Lcom/google/android/gms/tasks/Task;"),                           \
  X(DisableNetwork, "disableNetwork",                                   \
    "()Lcom/google/android/gms/tasks/Task;"),                           \
  X(Terminate, "terminate",                                             \
    "()Lcom/google/android/gms/tasks/Task;"),                           \
  X(WaitForPendingWrites, "waitForPendingWrites",                       \
    "()Lcom/google/android/gms/tasks/Task;"),                           \
  X(ClearPersistence, "clearPersistence",                               \
    "()Lcom/google/android/gms/tasks/Task;"),                           \
  X(AddSnapshotsInSyncListener, "addSnapshotsInSyncListener",           \
    "(Ljava/util/concurrent/Executor;Ljava/lang/Runnable;)"             \
    "Lcom/google/firebase/firestore/ListenerRegistration;")

// clang-format on

METHOD_LOOKUP_DECLARATION(firebase_firestore, FIREBASE_FIRESTORE_METHODS)
METHOD_LOOKUP_DEFINITION(firebase_firestore,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/firestore/FirebaseFirestore",
                         FIREBASE_FIRESTORE_METHODS)

#define SILENT_REJECTION_EXECUTOR_METHODS(X) X(Constructor, "<init>", "()V")
METHOD_LOOKUP_DECLARATION(silent_rejection_executor,
                          SILENT_REJECTION_EXECUTOR_METHODS)
METHOD_LOOKUP_DEFINITION(silent_rejection_executor,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/firestore/internal/cpp/"
                         "SilentRejectionSingleThreadExecutor",
                         SILENT_REJECTION_EXECUTOR_METHODS)

Mutex FirestoreInternal::init_mutex_;  // NOLINT
int FirestoreInternal::initialize_count_ = 0;
jni::Loader* FirestoreInternal::loader_ = nullptr;

FirestoreInternal::FirestoreInternal(App* app) {
  FIREBASE_ASSERT(app != nullptr);
  if (!Initialize(app)) return;
  app_ = app;

  JNIEnv* env = app_->GetJNIEnv();
  jobject platform_app = app_->GetPlatformApp();
  jobject firestore_obj = env->CallStaticObjectMethod(
      firebase_firestore::GetClass(),
      firebase_firestore::GetMethodId(firebase_firestore::kGetInstance),
      platform_app);
  util::CheckAndClearJniExceptions(env);
  env->DeleteLocalRef(platform_app);
  FIREBASE_ASSERT(firestore_obj != nullptr);
  obj_ = env->NewGlobalRef(firestore_obj);
  env->DeleteLocalRef(firestore_obj);

  // Mainly for enabling TimestampsInSnapshotsEnabled. The rest comes from the
  // default in native SDK. The C++ implementation relies on that for reading
  // timestamp FieldValues correctly. TODO(zxu): Once it is set to true by
  // default, we may safely remove the calls below.
  set_settings(settings());

  jobject user_callback_executor_obj =
      env->NewObject(silent_rejection_executor::GetClass(),
                     silent_rejection_executor::GetMethodId(
                         silent_rejection_executor::kConstructor));

  CheckAndClearJniExceptions(env);
  FIREBASE_ASSERT(user_callback_executor_obj != nullptr);
  user_callback_executor_ = env->NewGlobalRef(user_callback_executor_obj);
  env->DeleteLocalRef(user_callback_executor_obj);

  future_manager_.AllocFutureApi(this, static_cast<int>(FirestoreFn::kCount));
}

/* static */
bool FirestoreInternal::Initialize(App* app) {
  MutexLock init_lock(init_mutex_);
  if (initialize_count_ == 0) {
    jni::Initialize(app->java_vm());

    JNIEnv* env = app->GetJNIEnv();
    jobject activity = app->activity();
    if (!(firebase_firestore::CacheMethodIds(env, activity) &&
          // Call Initialize on each Firestore internal class.
          BlobInternal::Initialize(app) &&
          DirectionInternal::Initialize(app) &&
          DocumentChangeInternal::Initialize(app) &&
          DocumentChangeTypeInternal::Initialize(app) &&
          DocumentReferenceInternal::Initialize(app) &&
          DocumentSnapshotInternal::Initialize(app) &&
          FieldPathConverter::Initialize(app) &&
          FieldValueInternal::Initialize(app) &&
          FirebaseFirestoreExceptionInternal::Initialize(app) &&
          FirebaseFirestoreSettingsInternal::Initialize(app) &&
          GeoPointInternal::Initialize(app) &&
          ListenerRegistrationInternal::Initialize(app) &&
          MetadataChangesInternal::Initialize(app) &&
          QueryInternal::Initialize(app) &&
          QuerySnapshotInternal::Initialize(app) &&
          ServerTimestampBehaviorInternal::Initialize(app) &&
          SetOptionsInternal::Initialize(app) &&
          SnapshotMetadataInternal::Initialize(app) &&
          SourceInternal::Initialize(app) &&
          TimestampInternal::Initialize(app) &&
          TransactionInternal::Initialize(app) && Wrapper::Initialize(app) &&
          WriteBatchInternal::Initialize(app) &&
          // Initialize those embedded Firestore internal classes.
          InitializeEmbeddedClasses(app))) {
      ReleaseClasses(app);
      return false;
    }

    util::CheckAndClearJniExceptions(env);

    jni::Loader loader(app);
    CollectionReferenceInternal::Initialize(loader);
    if (!loader.ok()) {
      ReleaseClasses(app);
      return false;
    }

    FIREBASE_DEV_ASSERT(loader_ == nullptr);
    loader_ = new jni::Loader(Move(loader));
  }
  initialize_count_++;
  return true;
}

/* static */
bool FirestoreInternal::InitializeEmbeddedClasses(App* app) {
  JNIEnv* env = app->GetJNIEnv();
  jobject activity = app->activity();
  // Terminate() handles tearing this down.
  // Load embedded classes.
  const std::vector<internal::EmbeddedFile> embedded_files =
      util::CacheEmbeddedFiles(
          env, activity,
          internal::EmbeddedFile::ToVector(
              ::firebase_firestore::firestore_resources_filename,
              ::firebase_firestore::firestore_resources_data,
              ::firebase_firestore::firestore_resources_size));
  return EventListenerInternal::InitializeEmbeddedClasses(app,
                                                          &embedded_files) &&
         TransactionInternal::InitializeEmbeddedClasses(app, &embedded_files) &&
         silent_rejection_executor::CacheClassFromFiles(env, activity,
                                                        &embedded_files) &&
         silent_rejection_executor::CacheMethodIds(env, activity);
}

/* static */
void FirestoreInternal::ReleaseClasses(App* app) {
  delete loader_;
  loader_ = nullptr;

  JNIEnv* env = app->GetJNIEnv();
  firebase_firestore::ReleaseClass(env);
  silent_rejection_executor::ReleaseClass(env);
  util::CheckAndClearJniExceptions(env);

  // Call Terminate on each Firestore internal class.
  BlobInternal::Terminate(app);
  DirectionInternal::Terminate(app);
  DocumentChangeInternal::Terminate(app);
  DocumentChangeTypeInternal::Terminate(app);
  DocumentReferenceInternal::Terminate(app);
  DocumentSnapshotInternal::Terminate(app);
  EventListenerInternal::Terminate(app);
  FieldPathConverter::Terminate(app);
  FieldValueInternal::Terminate(app);
  FirebaseFirestoreExceptionInternal::Terminate(app);
  FirebaseFirestoreSettingsInternal::Terminate(app);
  GeoPointInternal::Terminate(app);
  ListenerRegistrationInternal::Terminate(app);
  MetadataChangesInternal::Terminate(app);
  QueryInternal::Terminate(app);
  QuerySnapshotInternal::Terminate(app);
  ServerTimestampBehaviorInternal::Terminate(app);
  SetOptionsInternal::Terminate(app);
  SnapshotMetadataInternal::Terminate(app);
  SourceInternal::Terminate(app);
  TimestampInternal::Terminate(app);
  TransactionInternal::Terminate(app);
  Wrapper::Terminate(app);
  WriteBatchInternal::Terminate(app);
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

void FirestoreInternal::ShutdownUserCallbackExecutor() {
  JNIEnv* env = app_->GetJNIEnv();
  auto shutdown_method = env->GetMethodID(
      env->GetObjectClass(user_callback_executor_), "shutdown", "()V");
  env->CallVoidMethod(user_callback_executor_, shutdown_method);
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

  future_manager_.ReleaseFutureApi(this);

  ShutdownUserCallbackExecutor();

  JNIEnv* env = app_->GetJNIEnv();
  env->DeleteGlobalRef(user_callback_executor_);
  user_callback_executor_ = nullptr;
  env->DeleteGlobalRef(obj_);
  obj_ = nullptr;
  Terminate(app_);
  app_ = nullptr;

  util::CheckAndClearJniExceptions(env);
}

CollectionReference FirestoreInternal::Collection(
    const char* collection_path) const {
  JNIEnv* env = app_->GetJNIEnv();
  jstring path_string = env->NewStringUTF(collection_path);
  jobject collection_reference = env->CallObjectMethod(
      obj_, firebase_firestore::GetMethodId(firebase_firestore::kCollection),
      path_string);
  env->DeleteLocalRef(path_string);
  CheckAndClearJniExceptions(env);
  FIREBASE_ASSERT(collection_reference != nullptr);
  CollectionReferenceInternal* internal = new CollectionReferenceInternal{
      const_cast<FirestoreInternal*>(this), collection_reference};
  env->DeleteLocalRef(collection_reference);
  CheckAndClearJniExceptions(env);
  return CollectionReference{internal};
}

DocumentReference FirestoreInternal::Document(const char* document_path) const {
  JNIEnv* env = app_->GetJNIEnv();
  jstring path_string = env->NewStringUTF(document_path);
  jobject document_reference = env->CallObjectMethod(
      obj_, firebase_firestore::GetMethodId(firebase_firestore::kDocument),
      path_string);
  env->DeleteLocalRef(path_string);
  CheckAndClearJniExceptions(env);
  FIREBASE_ASSERT(document_reference != nullptr);
  DocumentReferenceInternal* internal = new DocumentReferenceInternal{
      const_cast<FirestoreInternal*>(this), document_reference};
  env->DeleteLocalRef(document_reference);
  CheckAndClearJniExceptions(env);
  return DocumentReference{internal};
}

Query FirestoreInternal::CollectionGroup(const char* collection_id) const {
  JNIEnv* env = app_->GetJNIEnv();
  jstring collection_id_string = env->NewStringUTF(collection_id);

  jobject query = env->CallObjectMethod(
      obj_,
      firebase_firestore::GetMethodId(firebase_firestore::kCollectionGroup),
      collection_id_string);
  env->DeleteLocalRef(collection_id_string);
  CheckAndClearJniExceptions(env);
  FIREBASE_ASSERT(query != nullptr);

  QueryInternal* internal =
      new QueryInternal{const_cast<FirestoreInternal*>(this), query};
  env->DeleteLocalRef(query);

  CheckAndClearJniExceptions(env);
  return Query{internal};
}

Settings FirestoreInternal::settings() const {
  JNIEnv* env = app_->GetJNIEnv();
  jobject settings = env->CallObjectMethod(
      obj_, firebase_firestore::GetMethodId(firebase_firestore::kGetSettings));
  FIREBASE_ASSERT(settings != nullptr);

  Settings result =
      FirebaseFirestoreSettingsInternal::JavaSettingToSetting(env, settings);
  env->DeleteLocalRef(settings);
  CheckAndClearJniExceptions(env);
  return result;
}

void FirestoreInternal::set_settings(Settings settings) {
  JNIEnv* env = app_->GetJNIEnv();
  jobject settings_jobj =
      FirebaseFirestoreSettingsInternal::SettingToJavaSetting(env, settings);
  env->CallVoidMethod(
      obj_, firebase_firestore::GetMethodId(firebase_firestore::kSetSettings),
      settings_jobj);
  env->DeleteLocalRef(settings_jobj);
  CheckAndClearJniExceptions(env);
}

WriteBatch FirestoreInternal::batch() const {
  JNIEnv* env = app_->GetJNIEnv();
  jobject write_batch = env->CallObjectMethod(
      obj_, firebase_firestore::GetMethodId(firebase_firestore::kBatch));
  FIREBASE_ASSERT(write_batch != nullptr);

  WriteBatchInternal* internal =
      new WriteBatchInternal{const_cast<FirestoreInternal*>(this), write_batch};
  env->DeleteLocalRef(write_batch);
  CheckAndClearJniExceptions(env);
  return WriteBatch{internal};
}

Future<void> FirestoreInternal::RunTransaction(TransactionFunction* update,
                                               bool is_lambda) {
  JNIEnv* env = app_->GetJNIEnv();
  jobject transaction_function =
      TransactionInternal::ToJavaObject(env, this, update);
  jobject task = env->CallObjectMethod(
      obj_,
      firebase_firestore::GetMethodId(firebase_firestore::kRunTransaction),
      transaction_function);
  env->DeleteLocalRef(transaction_function);
  CheckAndClearJniExceptions(env);

#if defined(FIREBASE_USE_STD_FUNCTION) || defined(DOXYGEN)
  auto* completion =
      static_cast<LambdaTransactionFunction*>(is_lambda ? update : nullptr);
  Promise<void, void, FirestoreFn> promise{ref_future(), this, completion};
#else   // defined(FIREBASE_USE_STD_FUNCTION) || defined(DOXYGEN)
  Promise<void, void, FirestoreFn> promise{ref_future(), this};
#endif  // defined(FIREBASE_USE_STD_FUNCTION) || defined(DOXYGEN)

  promise.RegisterForTask(FirestoreFn::kRunTransaction, task);
  env->DeleteLocalRef(task);
  CheckAndClearJniExceptions(env);
  return promise.GetFuture();
}

#if defined(FIREBASE_USE_STD_FUNCTION) || defined(DOXYGEN)
Future<void> FirestoreInternal::RunTransaction(
    std::function<Error(Transaction&, std::string&)> update) {
  LambdaTransactionFunction* lambda_update =
      new LambdaTransactionFunction(firebase::Move(update));
  return RunTransaction(lambda_update, /*is_lambda=*/true);
}
#endif  // defined(FIREBASE_USE_STD_FUNCTION) || defined(DOXYGEN)

Future<void> FirestoreInternal::DisableNetwork() {
  JNIEnv* env = app_->GetJNIEnv();
  jobject task = env->CallObjectMethod(
      obj_,
      firebase_firestore::GetMethodId(firebase_firestore::kDisableNetwork));
  CheckAndClearJniExceptions(env);

  Promise<void, void, FirestoreFn> promise{ref_future(), this};
  promise.RegisterForTask(FirestoreFn::kDisableNetwork, task);
  env->DeleteLocalRef(task);
  CheckAndClearJniExceptions(env);
  return promise.GetFuture();
}

Future<void> FirestoreInternal::EnableNetwork() {
  JNIEnv* env = app_->GetJNIEnv();
  jobject task = env->CallObjectMethod(
      obj_,
      firebase_firestore::GetMethodId(firebase_firestore::kEnableNetwork));
  CheckAndClearJniExceptions(env);

  Promise<void, void, FirestoreFn> promise{ref_future(), this};
  promise.RegisterForTask(FirestoreFn::kEnableNetwork, task);
  env->DeleteLocalRef(task);
  CheckAndClearJniExceptions(env);
  return promise.GetFuture();
}

Future<void> FirestoreInternal::Terminate() {
  JNIEnv* env = app_->GetJNIEnv();

  jobject task = env->CallObjectMethod(
      obj_, firebase_firestore::GetMethodId(firebase_firestore::kTerminate));
  CheckAndClearJniExceptions(env);

  Promise<void, void, FirestoreFn> promise{ref_future(), this};
  promise.RegisterForTask(FirestoreFn::kTerminate, task);
  env->DeleteLocalRef(task);
  CheckAndClearJniExceptions(env);

  return promise.GetFuture();
}

Future<void> FirestoreInternal::WaitForPendingWrites() {
  JNIEnv* env = app_->GetJNIEnv();
  jobject task = env->CallObjectMethod(
      obj_, firebase_firestore::GetMethodId(
                firebase_firestore::kWaitForPendingWrites));
  CheckAndClearJniExceptions(env);

  Promise<void, void, FirestoreFn> promise{ref_future(), this};
  promise.RegisterForTask(FirestoreFn::kWaitForPendingWrites, task);
  env->DeleteLocalRef(task);
  CheckAndClearJniExceptions(env);
  return promise.GetFuture();
}

Future<void> FirestoreInternal::ClearPersistence() {
  JNIEnv* env = app_->GetJNIEnv();
  jobject task = env->CallObjectMethod(
      obj_,
      firebase_firestore::GetMethodId(firebase_firestore::kClearPersistence));
  CheckAndClearJniExceptions(env);

  Promise<void, void, FirestoreFn> promise{ref_future(), this};
  promise.RegisterForTask(FirestoreFn::kClearPersistence, task);
  env->DeleteLocalRef(task);
  CheckAndClearJniExceptions(env);
  return promise.GetFuture();
}

ListenerRegistration FirestoreInternal::AddSnapshotsInSyncListener(
    EventListener<void>* listener, bool passing_listener_ownership) {
  JNIEnv* env = app_->GetJNIEnv();

  // Create listener.
  jobject java_runnable =
      EventListenerInternal::EventListenerToJavaRunnable(env, listener);

  // Register listener.
  jobject java_registration = env->CallObjectMethod(
      obj_,
      firebase_firestore::GetMethodId(
          firebase_firestore::kAddSnapshotsInSyncListener),
      user_callback_executor(), java_runnable);
  env->DeleteLocalRef(java_runnable);
  CheckAndClearJniExceptions(env);

  // Wrapping
  ListenerRegistrationInternal* registration = new ListenerRegistrationInternal{
      this, listener, passing_listener_ownership, java_registration};
  env->DeleteLocalRef(java_registration);
  return ListenerRegistration{registration};
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

DocumentReference FirestoreInternal::NewDocumentReference(
    jni::Env& env, const jni::Object& reference) {
  if (!env.ok() || !reference) return {};

  return DocumentReference(
      new DocumentReferenceInternal(this, reference.get()));
}

/* static */
void Firestore::set_log_level(LogLevel log_level) {
  // "Verbose" and "debug" map to logging enabled.
  // "Info", "warning", "error", and "assert" map to logging disabled.
  bool logging_enabled = log_level < LogLevel::kLogLevelInfo;
  JNIEnv* env = firebase::util::GetJNIEnvFromApp();
  env->CallStaticVoidMethod(
      firebase_firestore::GetClass(),
      firebase_firestore::GetMethodId(firebase_firestore::kSetLoggingEnabled),
      logging_enabled);
  CheckAndClearJniExceptions(env);
}

}  // namespace firestore
}  // namespace firebase
