// Copyright 2016 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "database/src/android/database_android.h"

#include <assert.h>
#include <jni.h>

#include "app/src/app_android.h"
#include "app/src/app_common.h"
#include "app/src/embedded_file.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/include/firebase/log.h"
#include "app/src/jni/declaration.h"
#include "app/src/jni/env.h"
#include "app/src/jni/ownership.h"
#include "app/src/reference_counted_future_impl.h"
#include "app/src/util.h"
#include "app/src/util_android.h"
#include "database/database_resources.h"
#include "database/src/android/data_snapshot_android.h"
#include "database/src/android/database_reference_android.h"
#include "database/src/android/disconnection_android.h"
#include "database/src/android/mutable_data_android.h"
#include "database/src/android/query_android.h"
#include "database/src/include/firebase/database/transaction.h"

namespace firebase {
namespace database {
namespace internal {

Mutex g_database_reference_constructor_mutex;  // NOLINT
const char kApiIdentifier[] = "Database";

// clang-format off
#define LOGGER_LEVEL_METHODS(X)                                         \
  X(ValueOf, "valueOf",                                                 \
    "(Ljava/lang/String;)Lcom/google/firebase/database/Logger$Level;",  \
    jni::kMethodTypeStatic)

#define FIREBASE_DATABASE_METHODS(X)                                    \
  X(GetInstance, "getInstance",                                         \
    "(Lcom/google/firebase/FirebaseApp;)"                               \
    "Lcom/google/firebase/database/FirebaseDatabase;",                  \
    jni::kMethodTypeStatic)                                             \
  X(GetInstanceFromUrl, "getInstance",                                  \
    "(Lcom/google/firebase/FirebaseApp;Ljava/lang/String;)"             \
    "Lcom/google/firebase/database/FirebaseDatabase;",                  \
    jni::kMethodTypeStatic)                                             \
  X(GetApp, "getApp", "()Lcom/google/firebase/FirebaseApp;")            \
  X(GetRootReference, "getReference",                                   \
    "()Lcom/google/firebase/database/DatabaseReference;")               \
  X(GetReferenceFromPath, "getReference",                               \
    "(Ljava/lang/String;)Lcom/google/firebase/database/"                \
    "DatabaseReference;")                                               \
  X(GetReferenceFromUrl, "getReferenceFromUrl",                         \
    "(Ljava/lang/String;)"                                              \
    "Lcom/google/firebase/database/DatabaseReference;")                 \
  X(PurgeOutstandingWrites, "purgeOutstandingWrites", "()V")            \
  X(GoOnline, "goOnline", "()V")                                        \
  X(GoOffline, "goOffline", "()V")                                      \
  X(SetLogLevel, "setLogLevel",                                         \
    "(Lcom/google/firebase/database/Logger$Level;)V")                   \
  X(SetPersistenceEnabled, "setPersistenceEnabled", "(Z)V")             \
  X(GetSdkVersion, "getSdkVersion", "()Ljava/lang/String;",             \
    jni::kMethodTypeStatic)

#define DATABASE_ERROR_METHODS(X)                                              \
    X(GetCode, "getCode", "()I")                                               \
    X(GetMessage, "getMessage", "()Ljava/lang/String;")                        \
    X(FromException, "fromException",                                          \
      "(Ljava/lang/Throwable;)Lcom/google/firebase/database/DatabaseError;",   \
      jni::kMethodTypeStatic, jni::kMethodOptional)

#define DATABASE_ERROR_FIELDS(X)                                              \
    X(Disconnected, "DISCONNECTED", "I", jni::kFieldTypeStatic)               \
    X(ExpiredToken, "EXPIRED_TOKEN", "I", jni::kFieldTypeStatic)              \
    X(InvalidToken, "INVALID_TOKEN", "I", jni::kFieldTypeStatic)              \
    X(MaxRetries, "MAX_RETRIES", "I", jni::kFieldTypeStatic)                  \
    X(NetworkError, "NETWORK_ERROR", "I", jni::kFieldTypeStatic)              \
    X(OperationFailed, "OPERATION_FAILED", "I", jni::kFieldTypeStatic)        \
    X(OverriddenBySet, "OVERRIDDEN_BY_SET", "I", jni::kFieldTypeStatic)       \
    X(PermissionDenied, "PERMISSION_DENIED", "I", jni::kFieldTypeStatic)      \
    X(Unavailable, "UNAVAILABLE", "I", jni::kFieldTypeStatic)                 \
    X(UnknownError, "UNKNOWN_ERROR", "I", jni::kFieldTypeStatic)              \
    X(UserCodeException, "USER_CODE_EXCEPTION", "I", jni::kFieldTypeStatic)   \
    X(WriteCanceled, "WRITE_CANCELED", "I", jni::kFieldTypeStatic)

#define CPP_TRANSACTION_HANDLER_METHODS(X)                                    \
    X(Constructor, "<init>", "(JJ)V")                                         \
    X(DiscardPointers, "discardPointers", "()J")

#define CPP_EVENT_LISTENER_METHODS(X)                                    \
    X(DiscardPointers, "discardPointers", "()V")

#define CPP_VALUE_EVENT_LISTENER_METHODS(X)                                    \
    X(Constructor, "<init>", "(JJ)V")

#define CPP_CHILD_EVENT_LISTENER_METHODS(X)                                    \
    X(Constructor, "<init>", "(JJ)V")
// clang-format on

JNI_METHOD_LOOKUP_DECLARATION(
    logger_level, LOGGER_LEVEL_METHODS, FIELD_LOOKUP_NONE,
    PROGUARD_KEEP_CLASS "com/google/firebase/database/Logger$Level")
JNI_METHOD_LOOKUP_DECLARATION(
    firebase_database, FIREBASE_DATABASE_METHODS, FIELD_LOOKUP_NONE,
    PROGUARD_KEEP_CLASS "com/google/firebase/database/FirebaseDatabase")
JNI_METHOD_LOOKUP_DECLARATION(
    database_error, DATABASE_ERROR_METHODS, DATABASE_ERROR_FIELDS,
    PROGUARD_KEEP_CLASS "com/google/firebase/database/DatabaseError")
JNI_METHOD_LOOKUP_DECLARATION(
    cpp_transaction_handler, CPP_TRANSACTION_HANDLER_METHODS, FIELD_LOOKUP_NONE,
    "com/google/firebase/database/internal/cpp/CppTransactionHandler")
JNI_METHOD_LOOKUP_DECLARATION(
    cpp_event_listener, CPP_EVENT_LISTENER_METHODS, FIELD_LOOKUP_NONE,
    "com/google/firebase/database/internal/cpp/CppEventListener")
JNI_METHOD_LOOKUP_DECLARATION(
    cpp_value_event_listener, CPP_VALUE_EVENT_LISTENER_METHODS,
    FIELD_LOOKUP_NONE,
    "com/google/firebase/database/internal/cpp/CppValueEventListener")
JNI_METHOD_LOOKUP_DECLARATION(
    cpp_child_event_listener, CPP_CHILD_EVENT_LISTENER_METHODS,
    FIELD_LOOKUP_NONE,
    "com/google/firebase/database/internal/cpp/CppChildEventListener")

Mutex DatabaseInternal::init_mutex_;  // NOLINT
int DatabaseInternal::initialize_count_ = 0;
std::map<jint, Error>* DatabaseInternal::java_error_to_cpp_ = nullptr;

DatabaseInternal::DatabaseInternal(App* app)
    : app_(nullptr),
      obj_(nullptr),
      logger_(app_common::FindAppLoggerByName(app->name())),
      safe_this_(this) {
  if (!Initialize(app)) return;
  app_ = app;
  jni_task_id_ = CreateApiIdentifier(kApiIdentifier, this);
  JNIEnv* env = app_->GetJNIEnv();
  jobject platform_app = app->GetPlatformApp();
  jobject database_obj = env->CallStaticObjectMethod(
      firebase_database::GetClass(),
      firebase_database::GetMethodId(firebase_database::kGetInstance),
      platform_app);
  if (platform_app != nullptr) env->DeleteLocalRef(platform_app);
  if (::firebase::jni::CheckAndClearException(env) || database_obj == nullptr) {
    if (database_obj != nullptr) env->DeleteLocalRef(database_obj);
    logger_.LogWarning("Could not create default Database");
    Terminate(app_);
    app_ = nullptr;
  } else {
    obj_ = env->NewGlobalRef(database_obj);
    env->DeleteLocalRef(database_obj);
  }
}

DatabaseInternal::DatabaseInternal(App* app, const char* url)
    : app_(nullptr),
      obj_(nullptr),
      constructor_url_(url ? url : ""),
      logger_(app_common::FindAppLoggerByName(app->name())),
      safe_this_(this) {
  if (!Initialize(app)) return;
  app_ = app;
  jni_task_id_ = CreateApiIdentifier(kApiIdentifier, this);
  JNIEnv* env = app_->GetJNIEnv();
  jobject url_string = env->NewStringUTF(constructor_url_.c_str());
  jobject platform_app = app->GetPlatformApp();
  jobject database_obj = env->CallStaticObjectMethod(
      firebase_database::GetClass(),
      firebase_database::GetMethodId(firebase_database::kGetInstanceFromUrl),
      platform_app, url_string);
  if (platform_app != nullptr) env->DeleteLocalRef(platform_app);
  if (::firebase::jni::CheckAndClearException(env) || database_obj == nullptr) {
    if (database_obj != nullptr) env->DeleteLocalRef(database_obj);
    logger_.LogWarning("Could not create Database with URL '%s' .",
                       constructor_url_.c_str());
    Terminate(app_);
    app_ = nullptr;
  } else {
    obj_ = env->NewGlobalRef(database_obj);
    env->DeleteLocalRef(database_obj);
  }
  if (url_string != nullptr) env->DeleteLocalRef(url_string);
}

static const struct {
  database_error::Field java_error_field;
  Error error_code;
} g_error_codes[] = {
    {database_error::kDisconnected, kErrorDisconnected},
    {database_error::kExpiredToken, kErrorExpiredToken},
    {database_error::kInvalidToken, kErrorInvalidToken},
    {database_error::kMaxRetries, kErrorMaxRetries},
    {database_error::kNetworkError, kErrorNetworkError},
    {database_error::kOperationFailed, kErrorOperationFailed},
    {database_error::kOverriddenBySet, kErrorOverriddenBySet},
    {database_error::kPermissionDenied, kErrorPermissionDenied},
    {database_error::kUnavailable, kErrorUnavailable},
    {database_error::kUnknownError, kErrorUnknownError},
    {database_error::kUserCodeException, kErrorUnknownError},
    {database_error::kWriteCanceled, kErrorWriteCanceled},
    {database_error::kFieldCount, kErrorNone},
};

const char* kCppLogLevelToLoggerLevelName[] = {
    "DEBUG", "DEBUG", "INFO", "WARN", "ERROR", "NONE",
};

bool DatabaseInternal::Initialize(App* app) {
  MutexLock init_lock(init_mutex_);
  if (initialize_count_ == 0) {
    JNIEnv* env = app->GetJNIEnv();
    jobject activity = app->activity();
    if (!(firebase_database::CacheMethodIds(env, activity) &&
          logger_level::CacheMethodIds(env, activity) &&
          database_error::CacheMethodIds(env, activity) &&
          database_error::CacheFieldIds(env, activity) &&
          DatabaseReferenceInternal::Initialize(app) &&
          QueryInternal::Initialize(app) &&
          DataSnapshotInternal::Initialize(app) &&
          MutableDataInternal::Initialize(app) &&
          DisconnectionHandlerInternal::Initialize(app) &&
          InitializeEmbeddedClasses(app))) {
      ReleaseClasses(app);
      return false;
    }
    java_error_to_cpp_ = new std::map<jint, Error>();
    for (int i = 0;
         g_error_codes[i].java_error_field != database_error::kFieldCount;
         i++) {
      jint java_error = env->GetStaticIntField(
          database_error::GetClass(),
          database_error::GetFieldId(g_error_codes[i].java_error_field));
      java_error_to_cpp_->insert(
          std::make_pair(java_error, g_error_codes[i].error_code));
    }
    ::firebase::jni::CheckAndClearException(env);
  }
  initialize_count_++;
  return true;
}

bool DatabaseInternal::InitializeEmbeddedClasses(App* app) {
  static const JNINativeMethod kCppTransactionHandler[] = {
      {"nativeDoTransaction",
       "(JJLcom/google/firebase/database/MutableData;)"
       "Lcom/google/firebase/database/MutableData;",
       reinterpret_cast<void*>(
           &internal::Callbacks::TransactionHandlerDoTransaction)},
      {"nativeOnComplete",
       "(JJLcom/google/firebase/database/DatabaseError;Z"
       "Lcom/google/firebase/database/DataSnapshot;)V",
       reinterpret_cast<void*>(
           &internal::Callbacks::TransactionHandlerOnComplete)}};
  static const JNINativeMethod kCppValueEventListenerNatives[] = {
      {"nativeOnDataChange", "(JJLcom/google/firebase/database/DataSnapshot;)V",
       reinterpret_cast<void*>(
           &internal::Callbacks::ValueListenerNativeOnDataChange)},
      {"nativeOnCancelled", "(JJLcom/google/firebase/database/DatabaseError;)V",
       reinterpret_cast<void*>(
           &internal::Callbacks::ValueListenerNativeOnCancelled)}};
  static const JNINativeMethod kCppChildEventListenerNatives[] = {
      {"nativeOnCancelled", "(JJLcom/google/firebase/database/DatabaseError;)V",
       reinterpret_cast<void*>(
           &internal::Callbacks::ChildListenerNativeOnCancelled)},
      {"nativeOnChildAdded",
       "(JJLcom/google/firebase/database/DataSnapshot;Ljava/lang/String;)V",
       reinterpret_cast<void*>(
           &internal::Callbacks::ChildListenerNativeOnChildAdded)},
      {"nativeOnChildChanged",
       "(JJLcom/google/firebase/database/DataSnapshot;Ljava/lang/String;)V",
       reinterpret_cast<void*>(
           &internal::Callbacks::ChildListenerNativeOnChildChanged)},
      {"nativeOnChildMoved",
       "(JJLcom/google/firebase/database/DataSnapshot;Ljava/lang/String;)V",
       reinterpret_cast<void*>(
           &internal::Callbacks::ChildListenerNativeOnChildMoved)},
      {"nativeOnChildRemoved",
       "(JJLcom/google/firebase/database/DataSnapshot;)V",
       reinterpret_cast<void*>(
           &internal::Callbacks::ChildListenerNativeOnChildRemoved)},
  };

  JNIEnv* env = app->GetJNIEnv();
  const std::vector<firebase::internal::EmbeddedFile> embedded_files =
      util::CacheEmbeddedFiles(
          env, app->activity(),
          firebase::internal::EmbeddedFile::ToVector(
              firebase_database_resources::database_resources_filename,
              firebase_database_resources::database_resources_data,
              firebase_database_resources::database_resources_size));
  if (!(cpp_transaction_handler::CacheClassFromFiles(env, app->activity(),
                                                     &embedded_files) &&
        cpp_transaction_handler::CacheMethodIds(env, app->activity()) &&
        cpp_transaction_handler::RegisterNatives(
            env, kCppTransactionHandler,
            FIREBASE_ARRAYSIZE(kCppTransactionHandler)) &&
        cpp_event_listener::CacheClassFromFiles(env, app->activity(),
                                                &embedded_files) &&
        cpp_event_listener::CacheMethodIds(env, app->activity()) &&
        cpp_value_event_listener::CacheClassFromFiles(env, app->activity(),
                                                      &embedded_files) &&
        cpp_value_event_listener::CacheMethodIds(env, app->activity()) &&
        cpp_value_event_listener::RegisterNatives(
            env, kCppValueEventListenerNatives,
            FIREBASE_ARRAYSIZE(kCppValueEventListenerNatives)) &&
        cpp_child_event_listener::CacheClassFromFiles(env, app->activity(),
                                                      &embedded_files) &&
        cpp_child_event_listener::CacheMethodIds(env, app->activity()) &&
        cpp_child_event_listener::RegisterNatives(
            env, kCppChildEventListenerNatives,
            FIREBASE_ARRAYSIZE(kCppChildEventListenerNatives)))) {
    ::firebase::jni::CheckAndClearException(env);
    return false;
  }
  return true;
}

void DatabaseInternal::ReleaseClasses(App* app) {
  JNIEnv* env = app->GetJNIEnv();
  firebase_database::ReleaseClass(env);
  logger_level::ReleaseClass(env);
  database_error::ReleaseClass(env);
  DatabaseReferenceInternal::Terminate(app);
  QueryInternal::Terminate(app);
  DataSnapshotInternal::Terminate(app);
  MutableDataInternal::Terminate(app);
  DisconnectionHandlerInternal::Terminate(app);
  cpp_value_event_listener::ReleaseClass(env);
  cpp_child_event_listener::ReleaseClass(env);
  cpp_event_listener::ReleaseClass(env);
  cpp_transaction_handler::ReleaseClass(env);
}

void DatabaseInternal::Terminate(App* app) {
  MutexLock init_lock(init_mutex_);
  assert(initialize_count_ > 0);
  if (--initialize_count_ == 0) {
    ReleaseClasses(app);
    delete java_error_to_cpp_;
    java_error_to_cpp_ = nullptr;
  }
}

DatabaseInternal::~DatabaseInternal() {
  safe_this_.ClearReference();
  if (app_ == nullptr) return;
  cleanup_.CleanupAll();
  JNIEnv* env = app_->GetJNIEnv();
  util::CancelCallbacks(env, jni_task_id_.c_str());
  std::map<ValueListener*, jobject> value_listeners;
  std::map<ChildListener*, jobject> child_listeners;
  std::set<jobject> single_value_listeners;
  {
    MutexLock lock(listener_mutex_);
    value_listeners.swap(java_value_listener_lookup_);
    child_listeners.swap(java_child_listener_lookup_);
    single_value_listeners.swap(java_single_value_listeners_);
    single_value_listeners_.clear();
  }
  for (auto& kv : value_listeners) {
    ClearJavaEventListener(kv.second);
    env->DeleteGlobalRef(kv.second);
  }
  for (auto& kv : child_listeners) {
    ClearJavaEventListener(kv.second);
    env->DeleteGlobalRef(kv.second);
  }
  for (jobject listener : single_value_listeners) {
    ClearJavaEventListener(listener);
    env->DeleteGlobalRef(listener);
  }
  std::set<jobject> transaction_handlers;
  {
    MutexLock lock(transaction_mutex_);
    transaction_handlers.swap(java_transaction_handlers_);
  }
  for (jobject handler : transaction_handlers) {
    jlong transaction_ptr = env->CallLongMethod(
        handler, cpp_transaction_handler::GetMethodId(
                     cpp_transaction_handler::kDiscardPointers));
    ::firebase::jni::CheckAndClearException(env);
    delete reinterpret_cast<TransactionData*>(transaction_ptr);
    env->DeleteGlobalRef(handler);
  }
  env->DeleteGlobalRef(obj_);
  obj_ = nullptr;
  Terminate(app_);
  app_ = nullptr;
  ::firebase::jni::CheckAndClearException(env);
}

DatabaseReference DatabaseInternal::GetReference() const {
  JNIEnv* env = app_->GetJNIEnv();
  jobject ref_obj = env->CallObjectMethod(
      obj_,
      firebase_database::GetMethodId(firebase_database::kGetRootReference));
  if (::firebase::jni::CheckAndClearException(env) || ref_obj == nullptr) {
    if (ref_obj != nullptr) env->DeleteLocalRef(ref_obj);
    return DatabaseReference(nullptr);
  }
  auto* internal = new DatabaseReferenceInternal(
      const_cast<DatabaseInternal*>(this), ref_obj);
  env->DeleteLocalRef(ref_obj);
  return DatabaseReference(internal);
}

DatabaseReference DatabaseInternal::GetReference(const char* path) const {
  FIREBASE_ASSERT_RETURN(DatabaseReference(nullptr), path != nullptr);
  JNIEnv* env = app_->GetJNIEnv();
  jobject path_string = env->NewStringUTF(path);
  if (::firebase::jni::CheckAndClearException(env) || path_string == nullptr) {
    if (path_string != nullptr) env->DeleteLocalRef(path_string);
    return DatabaseReference(nullptr);
  }
  jobject ref_obj = env->CallObjectMethod(
      obj_,
      firebase_database::GetMethodId(firebase_database::kGetReferenceFromPath),
      path_string);
  env->DeleteLocalRef(path_string);
  if (::firebase::jni::CheckAndClearException(env) || ref_obj == nullptr) {
    if (ref_obj != nullptr) env->DeleteLocalRef(ref_obj);
    logger_.LogWarning("Database::GetReference(): Invalid path specified: %s",
                       path);
    return DatabaseReference(nullptr);
  }
  auto* internal = new DatabaseReferenceInternal(
      const_cast<DatabaseInternal*>(this), ref_obj);
  env->DeleteLocalRef(ref_obj);
  return DatabaseReference(internal);
}

DatabaseReference DatabaseInternal::GetReferenceFromUrl(const char* url) const {
  FIREBASE_ASSERT_RETURN(DatabaseReference(nullptr), url != nullptr);
  JNIEnv* env = app_->GetJNIEnv();
  jobject url_string = env->NewStringUTF(url);
  if (::firebase::jni::CheckAndClearException(env) || url_string == nullptr) {
    if (url_string != nullptr) env->DeleteLocalRef(url_string);
    return DatabaseReference(nullptr);
  }
  jobject ref_obj = env->CallObjectMethod(
      obj_,
      firebase_database::GetMethodId(firebase_database::kGetReferenceFromUrl),
      url_string);
  env->DeleteLocalRef(url_string);
  if (::firebase::jni::CheckAndClearException(env) || ref_obj == nullptr) {
    if (ref_obj != nullptr) env->DeleteLocalRef(ref_obj);
    logger_.LogWarning(
        "Database::GetReferenceFromUrl(): URL '%s' does not match the "
        "Database URL.",
        url);
    return DatabaseReference(nullptr);
  }
  auto* internal = new DatabaseReferenceInternal(
      const_cast<DatabaseInternal*>(this), ref_obj);
  env->DeleteLocalRef(ref_obj);
  return DatabaseReference(internal);
}

void DatabaseInternal::GoOffline() const {
  JNIEnv* env = app_->GetJNIEnv();
  env->CallVoidMethod(
      obj_, firebase_database::GetMethodId(firebase_database::kGoOffline));
  ::firebase::jni::CheckAndClearException(env);
}

void DatabaseInternal::GoOnline() const {
  JNIEnv* env = app_->GetJNIEnv();
  env->CallVoidMethod(
      obj_, firebase_database::GetMethodId(firebase_database::kGoOnline));
  ::firebase::jni::CheckAndClearException(env);
}

void DatabaseInternal::PurgeOutstandingWrites() const {
  JNIEnv* env = app_->GetJNIEnv();
  env->CallVoidMethod(obj_, firebase_database::GetMethodId(
                                firebase_database::kPurgeOutstandingWrites));
  ::firebase::jni::CheckAndClearException(env);
}

void DatabaseInternal::SetPersistenceEnabled(bool enabled) const {
  JNIEnv* env = app_->GetJNIEnv();
  env->CallVoidMethod(
      obj_,
      firebase_database::GetMethodId(firebase_database::kSetPersistenceEnabled),
      enabled);
  ::firebase::jni::CheckAndClearException(env);
}

void DatabaseInternal::set_log_level(LogLevel log_level) {
  FIREBASE_ASSERT_RETURN_VOID(
      log_level >= 0 &&
      static_cast<size_t>(log_level) <
          FIREBASE_ARRAYSIZE(kCppLogLevelToLoggerLevelName));
  JNIEnv* env = app_->GetJNIEnv();
  jstring enum_name =
      env->NewStringUTF(kCppLogLevelToLoggerLevelName[log_level]);
  if (!::firebase::jni::CheckAndClearException(env) && enum_name != nullptr) {
    jobject log_level_enum_obj = env->CallStaticObjectMethod(
        logger_level::GetClass(),
        logger_level::GetMethodId(logger_level::kValueOf), enum_name);
    if (!::firebase::jni::CheckAndClearException(env) &&
        log_level_enum_obj != nullptr) {
      env->CallVoidMethod(
          obj_, firebase_database::GetMethodId(firebase_database::kSetLogLevel),
          log_level_enum_obj);
      if (!::firebase::jni::CheckAndClearException(env)) {
        logger_.SetLogLevel(log_level);
      }
      env->DeleteLocalRef(log_level_enum_obj);
    }
  }
  if (enum_name != nullptr) env->DeleteLocalRef(enum_name);
}

Error DatabaseInternal::ErrorFromResultAndErrorCode(
    util::FutureResult result_code, jint error_code) const {
  switch (result_code) {
    case util::kFutureResultSuccess:
      return kErrorNone;
    case util::kFutureResultCancelled:
      return kErrorWriteCanceled;
    case util::kFutureResultFailure:
      return ErrorFromJavaErrorCode(error_code);
  }
  return kErrorUnknownError;
}

Error DatabaseInternal::ErrorFromJavaErrorCode(jint error_code) const {
  auto found = java_error_to_cpp_->find(error_code);
  return found != java_error_to_cpp_->end() ? found->second
                                            : kErrorUnknownError;
}

Error DatabaseInternal::ErrorFromJavaDatabaseError(
    jobject error, std::string* error_message) const {
  if (error == nullptr) return kErrorUnknownError;
  JNIEnv* env = app_->GetJNIEnv();
  if (error_message != nullptr) {
    jobject message = env->CallObjectMethod(
        error, database_error::GetMethodId(database_error::kGetMessage));
    if (!::firebase::jni::CheckAndClearException(env) && message != nullptr) {
      *error_message = ::firebase::jni::JStringToString(env, message);
    }
    if (message != nullptr) env->DeleteLocalRef(message);
  }
  jint java_error_code = env->CallIntMethod(
      error, database_error::GetMethodId(database_error::kGetCode));
  return ::firebase::jni::CheckAndClearException(env)
             ? kErrorUnknownError
             : ErrorFromJavaErrorCode(java_error_code);
}

Error DatabaseInternal::ErrorFromJavaThrowable(
    jobject throwable, std::string* error_message) const {
  if (throwable == nullptr) return kErrorUnknownError;
  JNIEnv* env = app_->GetJNIEnv();
  jmethodID from_exception =
      database_error::GetMethodId(database_error::kFromException);
  if (from_exception != nullptr) {
    jobject db_error = env->CallStaticObjectMethod(
        database_error::GetClass(), from_exception, throwable);
    if (::firebase::jni::CheckAndClearException(env)) {
      if (db_error != nullptr) env->DeleteLocalRef(db_error);
    } else if (db_error != nullptr) {
      Error code = ErrorFromJavaDatabaseError(db_error, error_message);
      env->DeleteLocalRef(db_error);
      return code;
    }
  }
  return kErrorUnknownError;
}

namespace {
struct FutureCallbackData {
  FutureCallbackData(SafeFutureHandle<void> handle_,
                     ReferenceCountedFutureImpl* impl_, DatabaseInternal* db_)
      : handle(handle_),
        impl(impl_),
        db_ref(db_->this_ref()),
        backing_future(MakeFuture(impl_, handle_)) {}
  SafeFutureHandle<void> handle;
  ReferenceCountedFutureImpl* impl;
  DatabaseInternal::ThisRef db_ref;
  Future<void> backing_future;
};

void FutureCallback(JNIEnv* env, jobject result, util::FutureResult result_code,
                    const char* status_message, void* callback_data) {
  (void)env;
  auto* data = reinterpret_cast<FutureCallbackData*>(callback_data);
  if (data != nullptr) {
    DatabaseInternal::ThisRefLock lock(&data->db_ref);
    if (DatabaseInternal* db = lock.GetReference()) {
      Error error_code = kErrorNone;
      std::string message = status_message != nullptr ? status_message : "";
      if (result_code == util::kFutureResultCancelled) {
        error_code = kErrorWriteCanceled;
      } else if (result_code == util::kFutureResultFailure) {
        error_code = db->ErrorFromJavaThrowable(result, &message);
      }
      data->impl->Complete(data->handle, error_code, message.c_str());
    }
    delete data;
  }
}
}  // namespace

void DatabaseInternal::RegisterTaskCallback(
    JNIEnv* env, jobject task, SafeFutureHandle<void> handle,
    ReferenceCountedFutureImpl* future_impl, const char* error_msg) {
  if (::firebase::jni::CheckAndClearException(env) || task == nullptr) {
    if (task != nullptr) env->DeleteLocalRef(task);
    future_impl->Complete(handle, kErrorUnknownError, error_msg);
    return;
  }
  auto* callback_data = new FutureCallbackData(handle, future_impl, this);
  util::RegisterCallbackOnTask(env, task, FutureCallback,
                               reinterpret_cast<void*>(callback_data),
                               jni_task_id());
  if (::firebase::jni::CheckAndClearException(env)) {
    delete callback_data;
    future_impl->Complete(handle, kErrorUnknownError, error_msg);
  }
  env->DeleteLocalRef(task);
}

jobject DatabaseInternal::CreateJavaEventListener(ValueListener* listener) {
  JNIEnv* env = app_->GetJNIEnv();
  jobject java_listener = env->NewObject(
      cpp_value_event_listener::GetClass(),
      cpp_value_event_listener::GetMethodId(
          cpp_value_event_listener::kConstructor),
      reinterpret_cast<jlong>(this), reinterpret_cast<jlong>(listener));
  if (::firebase::jni::CheckAndClearException(env) || !java_listener) {
    if (java_listener) env->DeleteLocalRef(java_listener);
    return nullptr;
  }
  jobject global = env->NewGlobalRef(java_listener);
  env->DeleteLocalRef(java_listener);
  return ::firebase::jni::CheckAndClearException(env) ? nullptr : global;
}

jobject DatabaseInternal::CreateJavaEventListener(ChildListener* listener) {
  JNIEnv* env = app_->GetJNIEnv();
  jobject java_listener = env->NewObject(
      cpp_child_event_listener::GetClass(),
      cpp_child_event_listener::GetMethodId(
          cpp_child_event_listener::kConstructor),
      reinterpret_cast<jlong>(this), reinterpret_cast<jlong>(listener));
  if (::firebase::jni::CheckAndClearException(env) || !java_listener) {
    if (java_listener) env->DeleteLocalRef(java_listener);
    return nullptr;
  }
  jobject global = env->NewGlobalRef(java_listener);
  env->DeleteLocalRef(java_listener);
  return ::firebase::jni::CheckAndClearException(env) ? nullptr : global;
}

void DatabaseInternal::ClearJavaEventListener(jobject java_listener) {
  if (java_listener == nullptr) return;
  JNIEnv* env = app_->GetJNIEnv();
  env->CallVoidMethod(java_listener, cpp_event_listener::GetMethodId(
                                         cpp_event_listener::kDiscardPointers));
  ::firebase::jni::CheckAndClearException(env);
}

template <typename T>
jobject DatabaseInternal::RegisterListener(const internal::QuerySpec& spec,
                                           T* listener,
                                           ListenerCollection<T>* by_query,
                                           std::map<T*, jobject>* lookup) {
  MutexLock lock(listener_mutex_);
  JNIEnv* env = app_->GetJNIEnv();
  if (!by_query->Register(spec, listener)) return nullptr;
  auto found = lookup->find(listener);
  if (found != lookup->end()) return env->NewLocalRef(found->second);
  jobject java_listener = CreateJavaEventListener(listener);
  if (java_listener == nullptr) {
    by_query->Unregister(spec, listener);
    return nullptr;
  }
  lookup->insert(std::make_pair(listener, java_listener));
  return env->NewLocalRef(java_listener);
}

template <typename T>
jobject DatabaseInternal::UnregisterListenerLocked(
    const internal::QuerySpec& spec, T* listener,
    ListenerCollection<T>* by_query, std::map<T*, jobject>* lookup,
    jobject* out_global_to_discard) {
  if (out_global_to_discard != nullptr) *out_global_to_discard = nullptr;
  if (!by_query->Unregister(spec, listener)) return nullptr;
  auto found = lookup->find(listener);
  if (found == lookup->end()) return nullptr;
  JNIEnv* env = app_->GetJNIEnv();
  jobject java_listener_global = found->second;
  jobject java_listener = env->NewLocalRef(java_listener_global);
  if (!by_query->Exists(listener)) {
    lookup->erase(found);
    if (out_global_to_discard != nullptr) {
      *out_global_to_discard = java_listener_global;
    }
  }
  return java_listener;
}

template <typename T>
jobject DatabaseInternal::UnregisterListener(const internal::QuerySpec& spec,
                                             T* listener,
                                             ListenerCollection<T>* by_query,
                                             std::map<T*, jobject>* lookup) {
  jobject global_to_discard = nullptr;
  jobject java_listener = nullptr;
  {
    MutexLock lock(listener_mutex_);
    java_listener = UnregisterListenerLocked(spec, listener, by_query, lookup,
                                             &global_to_discard);
  }
  if (global_to_discard != nullptr) {
    ClearJavaEventListener(global_to_discard);
    app_->GetJNIEnv()->DeleteGlobalRef(global_to_discard);
  }
  return java_listener;
}

template <typename T>
std::vector<jobject> DatabaseInternal::UnregisterAllListeners(
    const internal::QuerySpec& spec, ListenerCollection<T>* by_query,
    std::map<T*, jobject>* lookup) {
  std::vector<jobject> java_listeners;
  std::vector<jobject> globals_to_discard;
  {
    MutexLock lock(listener_mutex_);
    std::vector<T*> listeners;
    if (by_query->Get(spec, &listeners)) {
      for (T* listener : listeners) {
        jobject global_to_discard = nullptr;
        jobject java_listener = UnregisterListenerLocked(
            spec, listener, by_query, lookup, &global_to_discard);
        if (java_listener != nullptr) java_listeners.push_back(java_listener);
        if (global_to_discard != nullptr) {
          globals_to_discard.push_back(global_to_discard);
        }
      }
    }
  }
  if (!globals_to_discard.empty()) {
    JNIEnv* env = app_->GetJNIEnv();
    for (jobject g : globals_to_discard) {
      ClearJavaEventListener(g);
      env->DeleteGlobalRef(g);
    }
  }
  return java_listeners;
}

jobject DatabaseInternal::RegisterValueEventListener(
    const internal::QuerySpec& spec, ValueListener* listener) {
  return RegisterListener(spec, listener, &value_listeners_by_query_,
                          &java_value_listener_lookup_);
}

jobject DatabaseInternal::UnregisterValueEventListener(
    const internal::QuerySpec& spec, ValueListener* listener) {
  return UnregisterListener(spec, listener, &value_listeners_by_query_,
                            &java_value_listener_lookup_);
}

std::vector<jobject> DatabaseInternal::UnregisterAllValueEventListeners(
    const internal::QuerySpec& spec) {
  return UnregisterAllListeners(spec, &value_listeners_by_query_,
                                &java_value_listener_lookup_);
}

jobject DatabaseInternal::RegisterChildEventListener(
    const internal::QuerySpec& spec, ChildListener* listener) {
  return RegisterListener(spec, listener, &child_listeners_by_query_,
                          &java_child_listener_lookup_);
}

jobject DatabaseInternal::UnregisterChildEventListener(
    const internal::QuerySpec& spec, ChildListener* listener) {
  return UnregisterListener(spec, listener, &child_listeners_by_query_,
                            &java_child_listener_lookup_);
}

std::vector<jobject> DatabaseInternal::UnregisterAllChildEventListeners(
    const internal::QuerySpec& spec) {
  return UnregisterAllListeners(spec, &child_listeners_by_query_,
                                &java_child_listener_lookup_);
}

jobject DatabaseInternal::CreateJavaTransactionHandler(
    TransactionData* transaction_fn) {
  JNIEnv* env = app_->GetJNIEnv();
  jobject java_handler = env->NewObject(
      cpp_transaction_handler::GetClass(),
      cpp_transaction_handler::GetMethodId(
          cpp_transaction_handler::kConstructor),
      reinterpret_cast<jlong>(this), reinterpret_cast<jlong>(transaction_fn));
  if (::firebase::jni::CheckAndClearException(env) || !java_handler) {
    if (java_handler) env->DeleteLocalRef(java_handler);
    return nullptr;
  }
  jobject java_handler_global = env->NewGlobalRef(java_handler);
  env->DeleteLocalRef(java_handler);
  if (::firebase::jni::CheckAndClearException(env) || !java_handler_global) {
    return nullptr;
  }
  {
    MutexLock lock(transaction_mutex_);
    java_transaction_handlers_.insert(java_handler_global);
    transaction_fn->java_handler = java_handler_global;
  }
  return java_handler_global;
}

void DatabaseInternal::DeleteJavaTransactionHandler(
    jobject java_handler_global) {
  if (java_handler_global == nullptr) return;
  bool should_delete = false;
  {
    MutexLock lock(transaction_mutex_);
    should_delete = java_transaction_handlers_.erase(java_handler_global) > 0;
  }
  if (should_delete) {
    JNIEnv* env = app_->GetJNIEnv();
    jlong transaction_ptr = env->CallLongMethod(
        java_handler_global, cpp_transaction_handler::GetMethodId(
                                 cpp_transaction_handler::kDiscardPointers));
    ::firebase::jni::CheckAndClearException(env);
    delete reinterpret_cast<TransactionData*>(transaction_ptr);
    env->DeleteGlobalRef(java_handler_global);
  }
}

void Callbacks::ChildListenerNativeOnCancelled(JNIEnv* env, jclass clazz,
                                               jlong db_ptr, jlong listener_ptr,
                                               jobject database_error) {
  (void)env;
  (void)clazz;
  if (db_ptr != 0 && listener_ptr != 0) {
    auto* db = reinterpret_cast<DatabaseInternal*>(db_ptr);
    auto* listener = reinterpret_cast<ChildListener*>(listener_ptr);
    std::string error_msg;
    Error error_code =
        db->ErrorFromJavaDatabaseError(database_error, &error_msg);
    listener->OnCancelled(error_code, error_msg.c_str());
  }
}

void Callbacks::ChildListenerNativeOnChildAdded(JNIEnv* env, jclass clazz,
                                                jlong db_ptr,
                                                jlong listener_ptr,
                                                jobject data_snapshot,
                                                jobject previous_child_name) {
  (void)clazz;
  if (db_ptr != 0 && listener_ptr != 0) {
    auto* db = reinterpret_cast<DatabaseInternal*>(db_ptr);
    auto* listener = reinterpret_cast<ChildListener*>(listener_ptr);
    std::string prev =
        previous_child_name
            ? ::firebase::jni::JStringToString(env, previous_child_name)
            : std::string();
    listener->OnChildAdded(
        DataSnapshot(new DataSnapshotInternal(db, data_snapshot)),
        previous_child_name ? prev.c_str() : nullptr);
  }
}

void Callbacks::ChildListenerNativeOnChildChanged(JNIEnv* env, jclass clazz,
                                                  jlong db_ptr,
                                                  jlong listener_ptr,
                                                  jobject data_snapshot,
                                                  jobject previous_child_name) {
  (void)clazz;
  if (db_ptr != 0 && listener_ptr != 0) {
    auto* db = reinterpret_cast<DatabaseInternal*>(db_ptr);
    auto* listener = reinterpret_cast<ChildListener*>(listener_ptr);
    std::string prev =
        previous_child_name
            ? ::firebase::jni::JStringToString(env, previous_child_name)
            : std::string();
    listener->OnChildChanged(
        DataSnapshot(new DataSnapshotInternal(db, data_snapshot)),
        previous_child_name ? prev.c_str() : nullptr);
  }
}

void Callbacks::ChildListenerNativeOnChildMoved(JNIEnv* env, jclass clazz,
                                                jlong db_ptr,
                                                jlong listener_ptr,
                                                jobject data_snapshot,
                                                jobject previous_child_name) {
  (void)clazz;
  if (db_ptr != 0 && listener_ptr != 0) {
    auto* db = reinterpret_cast<DatabaseInternal*>(db_ptr);
    auto* listener = reinterpret_cast<ChildListener*>(listener_ptr);
    std::string prev =
        previous_child_name
            ? ::firebase::jni::JStringToString(env, previous_child_name)
            : std::string();
    listener->OnChildMoved(
        DataSnapshot(new DataSnapshotInternal(db, data_snapshot)),
        previous_child_name ? prev.c_str() : nullptr);
  }
}

void Callbacks::ChildListenerNativeOnChildRemoved(JNIEnv* env, jclass clazz,
                                                  jlong db_ptr,
                                                  jlong listener_ptr,
                                                  jobject data_snapshot) {
  (void)env;
  (void)clazz;
  if (db_ptr != 0 && listener_ptr != 0) {
    auto* db = reinterpret_cast<DatabaseInternal*>(db_ptr);
    auto* listener = reinterpret_cast<ChildListener*>(listener_ptr);
    listener->OnChildRemoved(
        DataSnapshot(new DataSnapshotInternal(db, data_snapshot)));
  }
}

void Callbacks::ValueListenerNativeOnCancelled(JNIEnv* env, jclass clazz,
                                               jlong db_ptr, jlong listener_ptr,
                                               jobject database_error) {
  (void)env;
  (void)clazz;
  if (db_ptr != 0 && listener_ptr != 0) {
    auto* db = reinterpret_cast<DatabaseInternal*>(db_ptr);
    auto* listener = reinterpret_cast<ValueListener*>(listener_ptr);
    std::string error_msg;
    Error error_code =
        db->ErrorFromJavaDatabaseError(database_error, &error_msg);
    listener->OnCancelled(error_code, error_msg.c_str());
  }
}

void Callbacks::ValueListenerNativeOnDataChange(JNIEnv* env, jclass clazz,
                                                jlong db_ptr,
                                                jlong listener_ptr,
                                                jobject data_snapshot) {
  (void)env;
  (void)clazz;
  if (db_ptr != 0 && listener_ptr != 0) {
    auto* db = reinterpret_cast<DatabaseInternal*>(db_ptr);
    auto* listener = reinterpret_cast<ValueListener*>(listener_ptr);
    listener->OnValueChanged(
        DataSnapshot(new DataSnapshotInternal(db, data_snapshot)));
  }
}

jobject Callbacks::TransactionHandlerDoTransaction(JNIEnv* env, jclass clazz,
                                                   jlong db_ptr,
                                                   jlong transaction_data_ptr,
                                                   jobject java_mutable_data) {
  (void)env;
  (void)clazz;
  if (db_ptr != 0 && transaction_data_ptr != 0) {
    auto* db = reinterpret_cast<DatabaseInternal*>(db_ptr);
    auto* data = reinterpret_cast<TransactionData*>(transaction_data_ptr);
    MutableData mutable_data = MutableDataInternal::MakeMutableData(
        new MutableDataInternal(db, java_mutable_data));
    if (data->transaction(&mutable_data, data->context) ==
        kTransactionResultSuccess) {
      return java_mutable_data;
    }
  }
  return nullptr;
}

void Callbacks::TransactionHandlerOnComplete(
    JNIEnv* env, jclass clazz, jlong db_ptr, jlong transaction_data_ptr,
    jobject database_error, jboolean was_committed, jobject data_snapshot) {
  (void)env;
  (void)clazz;
  if (db_ptr != 0 && transaction_data_ptr != 0) {
    auto* db = reinterpret_cast<DatabaseInternal*>(db_ptr);
    auto* data = reinterpret_cast<TransactionData*>(transaction_data_ptr);
    ReferenceCountedFutureImpl* future = data->future;
    SafeFutureHandle<DataSnapshot> handle = data->handle;
    Future<DataSnapshot> backing_future = data->backing_future;
    jobject java_handler_global = data->java_handler;

    Error error = kErrorNone;
    std::string error_message;
    bool has_snapshot_result = false;
    if (was_committed) {
      has_snapshot_result = true;
    } else if (database_error == nullptr) {
      has_snapshot_result = true;
      error = kErrorTransactionAbortedByUser;
      error_message =
          "The transaction was aborted, because the transaction function "
          "returned kTransactionResultAbort.";
    } else {
      error = db->ErrorFromJavaDatabaseError(database_error, &error_message);
    }

    DataSnapshot snapshot_result(
        (has_snapshot_result && data_snapshot != nullptr)
            ? new DataSnapshotInternal(db, data_snapshot)
            : nullptr);

    db->DeleteJavaTransactionHandler(java_handler_global);

    if (has_snapshot_result) {
      future->CompleteWithResult(handle, error, error_message.c_str(),
                                 snapshot_result);
    } else {
      future->Complete(handle, error, error_message.c_str());
    }
  }
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
