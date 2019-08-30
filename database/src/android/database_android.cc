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

#include "app/src/app_common.h"
#include "app/src/embedded_file.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/include/firebase/log.h"
#include "app/src/reference_counted_future_impl.h"
#include "app/src/util_android.h"
#include "database/database_resources.h"
#include "database/src/android/data_snapshot_android.h"
#include "database/src/android/database_reference_android.h"
#include "database/src/android/disconnection_android.h"
#include "database/src/android/mutable_data_android.h"
#include "database/src/android/query_android.h"
#include "database/src/android/util_android.h"
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
    util::kMethodTypeStatic)
// clang-format on
METHOD_LOOKUP_DECLARATION(logger_level, LOGGER_LEVEL_METHODS)

METHOD_LOOKUP_DEFINITION(logger_level,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/database/Logger$Level",
                         LOGGER_LEVEL_METHODS)

// clang-format off
#define FIREBASE_DATABASE_METHODS(X)                                    \
  X(GetInstance, "getInstance",                                         \
    "(Lcom/google/firebase/FirebaseApp;)"                               \
    "Lcom/google/firebase/database/FirebaseDatabase;",                  \
    util::kMethodTypeStatic),                                           \
  X(GetInstanceFromUrl, "getInstance",                                  \
    "(Lcom/google/firebase/FirebaseApp;Ljava/lang/String;)"             \
    "Lcom/google/firebase/database/FirebaseDatabase;",                  \
    util::kMethodTypeStatic),                                           \
  X(GetApp, "getApp", "()Lcom/google/firebase/FirebaseApp;"),           \
  X(GetRootReference, "getReference",                                   \
    "()Lcom/google/firebase/database/DatabaseReference;"),              \
  X(GetReferenceFromPath, "getReference",                               \
    "(Ljava/lang/String;)Lcom/google/firebase/database/"                \
    "DatabaseReference;"),                                              \
  X(GetReferenceFromUrl, "getReferenceFromUrl",                         \
    "(Ljava/lang/String;)"                                              \
    "Lcom/google/firebase/database/DatabaseReference;"),                \
  X(PurgeOutstandingWrites, "purgeOutstandingWrites", "()V"),           \
  X(GoOnline, "goOnline", "()V"),                                       \
  X(GoOffline, "goOffline", "()V"),                                     \
  X(SetLogLevel, "setLogLevel",                                         \
    "(Lcom/google/firebase/database/Logger$Level;)V"),                  \
  X(SetPersistenceEnabled, "setPersistenceEnabled", "(Z)V"),            \
  X(GetSdkVersion, "getSdkVersion", "()Ljava/lang/String;",             \
    util::kMethodTypeStatic)
// clang-format on

METHOD_LOOKUP_DECLARATION(firebase_database, FIREBASE_DATABASE_METHODS)
METHOD_LOOKUP_DEFINITION(firebase_database,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/database/FirebaseDatabase",
                         FIREBASE_DATABASE_METHODS)

// clang-format off
#define DATABASE_ERROR_METHODS(X)                                              \
    X(GetCode, "getCode", "()I"),                                              \
    X(GetMessage, "getMessage", "()Ljava/lang/String;")
// clang-format on

// clang-format off
#define DATABASE_ERROR_FIELDS(X)                                               \
    X(Disconnected, "DISCONNECTED", "I", util::kFieldTypeStatic),              \
    X(ExpiredToken, "EXPIRED_TOKEN", "I", util::kFieldTypeStatic),             \
    X(InvalidToken, "INVALID_TOKEN", "I", util::kFieldTypeStatic),             \
    X(MaxRetries, "MAX_RETRIES", "I", util::kFieldTypeStatic),                 \
    X(NetworkError, "NETWORK_ERROR", "I", util::kFieldTypeStatic),             \
    X(OperationFailed, "OPERATION_FAILED", "I", util::kFieldTypeStatic),       \
    X(OverriddenBySet, "OVERRIDDEN_BY_SET", "I", util::kFieldTypeStatic),      \
    X(PermissionDenied, "PERMISSION_DENIED", "I", util::kFieldTypeStatic),     \
    X(Unavailable, "UNAVAILABLE", "I", util::kFieldTypeStatic),                \
    X(UnknownError, "UNKNOWN_ERROR", "I", util::kFieldTypeStatic),             \
    X(UserCodeException, "USER_CODE_EXCEPTION", "I", util::kFieldTypeStatic),  \
    X(WriteCanceled, "WRITE_CANCELED", "I", util::kFieldTypeStatic)
// clang-format on

METHOD_LOOKUP_DECLARATION(database_error, DATABASE_ERROR_METHODS,
                          DATABASE_ERROR_FIELDS)
METHOD_LOOKUP_DEFINITION(database_error,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/database/DatabaseError",
                         DATABASE_ERROR_METHODS, DATABASE_ERROR_FIELDS)

// clang-format off
#define CPP_TRANSACTION_HANDLER_METHODS(X)                                    \
    X(Constructor, "<init>", "(JJ)V"),                                        \
    X(DiscardPointers, "discardPointers", "()J")
// clang-format on
METHOD_LOOKUP_DECLARATION(cpp_transaction_handler,
                          CPP_TRANSACTION_HANDLER_METHODS)
METHOD_LOOKUP_DEFINITION(
    cpp_transaction_handler,
    "com/google/firebase/database/internal/cpp/CppTransactionHandler",
    CPP_TRANSACTION_HANDLER_METHODS)

// clang-format off
#define CPP_EVENT_LISTENER_METHODS(X)                                    \
    X(DiscardPointers, "discardPointers", "()V")
// clang-format on
METHOD_LOOKUP_DECLARATION(cpp_event_listener, CPP_EVENT_LISTENER_METHODS)
METHOD_LOOKUP_DEFINITION(
    cpp_event_listener,
    "com/google/firebase/database/internal/cpp/CppEventListener",
    CPP_EVENT_LISTENER_METHODS)

// clang-format off
#define CPP_VALUE_EVENT_LISTENER_METHODS(X)                                    \
    X(Constructor, "<init>", "(JJ)V")
// clang-format on
METHOD_LOOKUP_DECLARATION(cpp_value_event_listener,
                          CPP_VALUE_EVENT_LISTENER_METHODS)
METHOD_LOOKUP_DEFINITION(
    cpp_value_event_listener,
    "com/google/firebase/database/internal/cpp/CppValueEventListener",
    CPP_VALUE_EVENT_LISTENER_METHODS)

// clang-format off
#define CPP_CHILD_EVENT_LISTENER_METHODS(X)                                    \
    X(Constructor, "<init>", "(JJ)V")
// clang-format on
METHOD_LOOKUP_DECLARATION(cpp_child_event_listener,
                          CPP_CHILD_EVENT_LISTENER_METHODS)
METHOD_LOOKUP_DEFINITION(
    cpp_child_event_listener,
    "com/google/firebase/database/internal/cpp/CppChildEventListener",
    CPP_CHILD_EVENT_LISTENER_METHODS)

Mutex DatabaseInternal::init_mutex_;  // NOLINT
int DatabaseInternal::initialize_count_ = 0;
std::map<jint, Error>* DatabaseInternal::java_error_to_cpp_ = nullptr;

DatabaseInternal::DatabaseInternal(App* app)
    : logger_(app_common::FindAppLoggerByName(app->name())) {
  app_ = nullptr;
  if (!Initialize(app)) return;
  app_ = app;

  JNIEnv* env = app_->GetJNIEnv();
  jobject platform_app = app->GetPlatformApp();
  jobject database_obj = env->CallStaticObjectMethod(
      firebase_database::GetClass(),
      firebase_database::GetMethodId(firebase_database::kGetInstance),
      platform_app);
  env->DeleteLocalRef(platform_app);
  if (database_obj == nullptr) {
    logger_.LogWarning("Could not create default Database");
    util::CheckAndClearJniExceptions(env);
    // Something went wrong -> uninitialize the database
    Terminate(app_);
    app_ = nullptr;
  } else {
    obj_ = env->NewGlobalRef(database_obj);
    env->DeleteLocalRef(database_obj);
  }
}

DatabaseInternal::DatabaseInternal(App* app, const char* url)
    : constructor_url_(url),
      logger_(app_common::FindAppLoggerByName(app->name())) {
  app_ = nullptr;
  if (!Initialize(app)) return;
  app_ = app;

  JNIEnv* env = app_->GetJNIEnv();
  jobject url_string = env->NewStringUTF(url);
  jobject platform_app = app->GetPlatformApp();
  jobject database_obj = env->CallStaticObjectMethod(
      firebase_database::GetClass(),
      firebase_database::GetMethodId(firebase_database::kGetInstanceFromUrl),
      platform_app, url_string);
  env->DeleteLocalRef(platform_app);
  if (database_obj == nullptr) {
    logger_.LogWarning("Could not create Database with URL '%s' .", url);
    util::CheckAndClearJniExceptions(env);
    // Something went wrong -> uninitialize the database
    Terminate(app_);
    app_ = nullptr;
  } else {
    obj_ = env->NewGlobalRef(database_obj);
    env->DeleteLocalRef(database_obj);
  }
  env->DeleteLocalRef(url_string);
}

// Which DatabaseError Java fields correspond to which C++ Error enum values.
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
    {database_error::kWriteCanceled, kErrorWriteCanceled},
    {database_error::kFieldCount, kErrorNone},  // sentinel value for end
};

// C++ log levels mapped to Logger.Level enum value names.
const char* kCppLogLevelToLoggerLevelName[] = {
    "DEBUG",  // kLogLevelVerbose --> Logger.Level.DEBUG
    "DEBUG",  // kLogLevelDebug --> Logger.Level.DEBUG
    "INFO",   // kLogLevelInfo --> Logger.Level.INFO
    "WARN",   // kLogLevelWarning --> Logger.Level.WARN
    "ERROR",  // kLogLevelError --> Logger.Level.ERROR
    "NONE",   // kLogLevelAssert --> Logger.Level.NONE
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
          // Call Initialize on all other RTDB internal classes.
          DatabaseReferenceInternal::Initialize(app) &&
          QueryInternal::Initialize(app) &&
          DataSnapshotInternal::Initialize(app) &&
          MutableDataInternal::Initialize(app) &&
          DisconnectionHandlerInternal::Initialize(app) &&
          InitializeEmbeddedClasses(app))) {
      ReleaseClasses(app);
      return false;
    }

    // Cache error codes
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
    util::CheckAndClearJniExceptions(env);
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
  // Terminate() handles tearing this down.
  // Load embedded classes.
  const std::vector<firebase::internal::EmbeddedFile> embedded_files =
      util::CacheEmbeddedFiles(
          env, app->activity(),
          firebase::internal::EmbeddedFile::ToVector(
              firebase_database_resources::database_resources_filename,
              firebase_database_resources::database_resources_data,
              firebase_database_resources::database_resources_size));
  if (!(cpp_transaction_handler::CacheClassFromFiles(env, app->activity(),
                                                     &embedded_files) &&
        cpp_event_listener::CacheClassFromFiles(env, app->activity(),
                                                &embedded_files) &&
        cpp_value_event_listener::CacheClassFromFiles(env, app->activity(),
                                                      &embedded_files) &&
        cpp_child_event_listener::CacheClassFromFiles(env, app->activity(),
                                                      &embedded_files) &&
        cpp_transaction_handler::CacheMethodIds(env, app->activity()) &&
        cpp_transaction_handler::RegisterNatives(
            env, kCppTransactionHandler,
            FIREBASE_ARRAYSIZE(kCppTransactionHandler)) &&
        cpp_event_listener::CacheMethodIds(env, app->activity()) &&
        cpp_value_event_listener::CacheMethodIds(env, app->activity()) &&
        cpp_value_event_listener::RegisterNatives(
            env, kCppValueEventListenerNatives,
            FIREBASE_ARRAYSIZE(kCppValueEventListenerNatives)) &&
        cpp_child_event_listener::CacheMethodIds(env, app->activity()) &&
        cpp_child_event_listener::RegisterNatives(
            env, kCppChildEventListenerNatives,
            FIREBASE_ARRAYSIZE(kCppChildEventListenerNatives)))) {
    return false;
  }
  return true;
}

void DatabaseInternal::ReleaseClasses(App* app) {
  JNIEnv* env = app->GetJNIEnv();
  firebase_database::ReleaseClass(env);
  logger_level::ReleaseClass(env);
  database_error::ReleaseClass(env);

  // Call Terminate on all other RTDB internal classes.
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
  initialize_count_--;
  if (initialize_count_ == 0) {
    ReleaseClasses(app);
    delete java_error_to_cpp_;
    java_error_to_cpp_ = nullptr;
  }
}

DatabaseInternal::~DatabaseInternal() {
  // If initialization failed, there is nothing to clean up.
  if (app_ == nullptr) return;

  cleanup_.CleanupAll();

  JNIEnv* env = app_->GetJNIEnv();
  {
    // If there are any pending listeners, delete their pointers.
    MutexLock lock(listener_mutex_);
    for (auto i = java_value_listener_lookup_.begin();
         i != java_value_listener_lookup_.end(); ++i) {
      ClearJavaEventListener(i->second);
    }
    for (auto i = java_child_listener_lookup_.begin();
         i != java_child_listener_lookup_.end(); ++i) {
      ClearJavaEventListener(i->second);
    }
    for (auto i = java_single_value_listeners_.begin();
         i != java_single_value_listeners_.end(); ++i) {
      ClearJavaEventListener(*i);
      env->DeleteGlobalRef(*i);
    }
    java_single_value_listeners_.clear();
  }
  {
    MutexLock lock(transaction_mutex_);
    for (auto i = java_transaction_handlers_.begin();
         i != java_transaction_handlers_.end(); ++i) {
      jlong transaction_ptr = env->CallLongMethod(
          *i, cpp_transaction_handler::GetMethodId(
                  cpp_transaction_handler::kDiscardPointers));
      TransactionData* data =
          reinterpret_cast<TransactionData*>(transaction_ptr);
      delete data;
      env->DeleteGlobalRef(*i);
    }
    java_single_value_listeners_.clear();
  }

  env->DeleteGlobalRef(obj_);
  obj_ = nullptr;
  Terminate(app_);
  app_ = nullptr;

  util::CheckAndClearJniExceptions(env);
}

App* DatabaseInternal::GetApp() const { return app_; }

DatabaseReference DatabaseInternal::GetReference() const {
  JNIEnv* env = app_->GetJNIEnv();
  jobject database_reference_obj = env->CallObjectMethod(
      obj_,
      firebase_database::GetMethodId(firebase_database::kGetRootReference));
  FIREBASE_ASSERT(database_reference_obj != nullptr);
  DatabaseReferenceInternal* internal = new DatabaseReferenceInternal(
      const_cast<DatabaseInternal*>(this), database_reference_obj);
  env->DeleteLocalRef(database_reference_obj);
  util::CheckAndClearJniExceptions(env);
  return DatabaseReference(internal);
}

DatabaseReference DatabaseInternal::GetReference(const char* path) const {
  FIREBASE_ASSERT_RETURN(DatabaseReference(nullptr), path != nullptr);
  JNIEnv* env = app_->GetJNIEnv();
  jobject path_string = env->NewStringUTF(path);
  jobject database_reference_obj = env->CallObjectMethod(
      obj_,
      firebase_database::GetMethodId(firebase_database::kGetReferenceFromPath),
      path_string);
  env->DeleteLocalRef(path_string);
  if (database_reference_obj == nullptr) {
    logger_.LogWarning("Database::GetReference(): Invalid path specified: %s",
                       path);
    util::CheckAndClearJniExceptions(env);
    return DatabaseReference(nullptr);
  }
  DatabaseReferenceInternal* internal = new DatabaseReferenceInternal(
      const_cast<DatabaseInternal*>(this), database_reference_obj);
  env->DeleteLocalRef(database_reference_obj);
  return DatabaseReference(internal);
}

DatabaseReference DatabaseInternal::GetReferenceFromUrl(const char* url) const {
  FIREBASE_ASSERT_RETURN(DatabaseReference(nullptr), url != nullptr);
  JNIEnv* env = app_->GetJNIEnv();
  jobject url_string = env->NewStringUTF(url);
  jobject database_reference_obj = env->CallObjectMethod(
      obj_,
      firebase_database::GetMethodId(firebase_database::kGetReferenceFromUrl),
      url_string);
  env->DeleteLocalRef(url_string);
  if (database_reference_obj == nullptr) {
    logger_.LogWarning(
        "Database::GetReferenceFromUrl(): URL '%s' does not match the "
        "Database URL.",
        url);
    util::CheckAndClearJniExceptions(env);
    return DatabaseReference(nullptr);
  }
  DatabaseReferenceInternal* internal = new DatabaseReferenceInternal(
      const_cast<DatabaseInternal*>(this), database_reference_obj);
  env->DeleteLocalRef(database_reference_obj);
  return DatabaseReference(internal);
}

void DatabaseInternal::GoOffline() const {
  JNIEnv* env = app_->GetJNIEnv();
  env->CallVoidMethod(
      obj_, firebase_database::GetMethodId(firebase_database::kGoOffline));
  util::CheckAndClearJniExceptions(env);
}

void DatabaseInternal::GoOnline() const {
  JNIEnv* env = app_->GetJNIEnv();
  env->CallVoidMethod(
      obj_, firebase_database::GetMethodId(firebase_database::kGoOnline));
  util::CheckAndClearJniExceptions(env);
}

void DatabaseInternal::PurgeOutstandingWrites() const {
  JNIEnv* env = app_->GetJNIEnv();
  env->CallVoidMethod(obj_, firebase_database::GetMethodId(
                                firebase_database::kPurgeOutstandingWrites));
  util::CheckAndClearJniExceptions(env);
}

void DatabaseInternal::SetPersistenceEnabled(bool enabled) const {
  JNIEnv* env = app_->GetJNIEnv();
  env->CallVoidMethod(
      obj_,
      firebase_database::GetMethodId(firebase_database::kSetPersistenceEnabled),
      enabled);
  util::CheckAndClearJniExceptions(env);
}

void DatabaseInternal::set_log_level(LogLevel log_level) {
  FIREBASE_ASSERT_RETURN_VOID(log_level <
                              (sizeof(kCppLogLevelToLoggerLevelName) /
                               sizeof(kCppLogLevelToLoggerLevelName[0])));
  JNIEnv* env = app_->GetJNIEnv();
  jstring enum_name =
      env->NewStringUTF(kCppLogLevelToLoggerLevelName[log_level]);
  if (!util::CheckAndClearJniExceptions(env)) {
    jobject log_level_enum_obj = env->CallStaticObjectMethod(
        logger_level::GetClass(),
        logger_level::GetMethodId(logger_level::kValueOf), enum_name);
    if (!util::CheckAndClearJniExceptions(env)) {
      env->CallVoidMethod(
          obj_, firebase_database::GetMethodId(firebase_database::kSetLogLevel),
          log_level_enum_obj);
      if (!util::CheckAndClearJniExceptions(env)) {
        logger_.SetLogLevel(log_level);
      }
      env->DeleteLocalRef(log_level_enum_obj);
    }
    env->DeleteLocalRef(enum_name);
  }
}

LogLevel DatabaseInternal::log_level() const { return logger_.GetLogLevel(); }

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
}

Error DatabaseInternal::ErrorFromJavaErrorCode(jint error_code) const {
  auto found = java_error_to_cpp_->find(error_code);
  if (found != java_error_to_cpp_->end()) {
    return found->second;
  } else {
    // Couldn't find the error, return kUnknownError.
    return kErrorUnknownError;
  }
}

Error DatabaseInternal::ErrorFromJavaDatabaseError(
    jobject error, std::string* error_message) const {
  JNIEnv* env = app_->GetJNIEnv();

  if (error_message != nullptr) {
    jobject message = env->CallObjectMethod(
        error, database_error::GetMethodId(database_error::kGetMessage));
    if (message) {
      *error_message = util::JniStringToString(env, message);
    }
  }
  jint java_error_code = env->CallIntMethod(
      error, database_error::GetMethodId(database_error::kGetCode));
  return ErrorFromJavaErrorCode(java_error_code);
}

jobject DatabaseInternal::CreateJavaEventListener(ValueListener* listener) {
  JNIEnv* env = app_->GetJNIEnv();
  jobject java_listener = env->NewObject(
      cpp_value_event_listener::GetClass(),
      cpp_value_event_listener::GetMethodId(
          cpp_value_event_listener::kConstructor),
      reinterpret_cast<jlong>(this), reinterpret_cast<jlong>(listener));
  jobject java_listener_global = env->NewGlobalRef(java_listener);
  env->DeleteLocalRef(java_listener);
  return java_listener_global;
}

void DatabaseInternal::ClearJavaEventListener(jobject java_listener) {
  JNIEnv* env = app_->GetJNIEnv();
  env->CallVoidMethod(java_listener, cpp_event_listener::GetMethodId(
                                         cpp_event_listener::kDiscardPointers));
}

jobject DatabaseInternal::RegisterValueEventListener(
    const internal::QuerySpec& spec, ValueListener* listener) {
  MutexLock lock(listener_mutex_);
  if (value_listeners_by_query_.Register(spec, listener)) {
    auto found = java_value_listener_lookup_.find(listener);
    if (found != java_value_listener_lookup_.end()) {
      return found->second;
    } else {
      jobject java_listener = CreateJavaEventListener(listener);
      java_value_listener_lookup_.insert(
          std::make_pair(listener, java_listener));
      return java_listener;
    }
  }
  return nullptr;
}

jobject DatabaseInternal::UnregisterValueEventListener(
    const internal::QuerySpec& spec, ValueListener* listener) {
  MutexLock lock(listener_mutex_);
  if (value_listeners_by_query_.Unregister(spec, listener)) {
    auto found = java_value_listener_lookup_.find(listener);
    if (found != java_value_listener_lookup_.end()) {
      JNIEnv* env = app_->GetJNIEnv();
      jobject java_listener_global = found->second;
      jobject java_listener = env->NewLocalRef(java_listener_global);

      if (!value_listeners_by_query_.Exists(listener)) {
        // No longer registered to any queries, let's remove this
        // EventListener object.
        ClearJavaEventListener(java_listener);
        java_value_listener_lookup_.erase(found);
        env->DeleteGlobalRef(java_listener_global);
      }
      return java_listener;
    }
  }
  return nullptr;
}

std::vector<jobject> DatabaseInternal::UnregisterAllValueEventListeners(
    const internal::QuerySpec& spec) {
  std::vector<jobject> java_listeners;
  std::vector<ValueListener*> listeners;
  if (value_listeners_by_query_.Get(spec, &listeners)) {
    for (int i = 0; i < listeners.size(); i++) {
      jobject java_listener = UnregisterValueEventListener(spec, listeners[i]);
      if (java_listener != nullptr) java_listeners.push_back(java_listener);
    }
  }
  return java_listeners;
}

jobject DatabaseInternal::CreateJavaEventListener(ChildListener* listener) {
  JNIEnv* env = app_->GetJNIEnv();
  jobject java_listener = env->NewObject(
      cpp_child_event_listener::GetClass(),
      cpp_child_event_listener::GetMethodId(
          cpp_child_event_listener::kConstructor),
      reinterpret_cast<jlong>(this), reinterpret_cast<jlong>(listener));
  jobject java_listener_global = env->NewGlobalRef(java_listener);
  env->DeleteLocalRef(java_listener);
  return java_listener_global;
}

jobject DatabaseInternal::RegisterChildEventListener(
    const internal::QuerySpec& spec, ChildListener* listener) {
  MutexLock lock(listener_mutex_);
  if (child_listeners_by_query_.Register(spec, listener)) {
    auto found = java_child_listener_lookup_.find(listener);
    if (found != java_child_listener_lookup_.end()) {
      return found->second;
    } else {
      jobject java_listener = CreateJavaEventListener(listener);
      java_child_listener_lookup_.insert(
          std::make_pair(listener, java_listener));
      return java_listener;
    }
  }
  return nullptr;
}

jobject DatabaseInternal::UnregisterChildEventListener(
    const internal::QuerySpec& spec, ChildListener* listener) {
  MutexLock lock(listener_mutex_);
  if (child_listeners_by_query_.Unregister(spec, listener)) {
    auto found = java_child_listener_lookup_.find(listener);
    if (found != java_child_listener_lookup_.end()) {
      JNIEnv* env = app_->GetJNIEnv();
      jobject java_listener_global = found->second;
      jobject java_listener = env->NewLocalRef(java_listener_global);

      if (!child_listeners_by_query_.Exists(listener)) {
        // No longer registered to any queries, let's remove this
        // EventListener object.
        ClearJavaEventListener(java_listener);
        java_child_listener_lookup_.erase(found);
        env->DeleteGlobalRef(java_listener_global);
      }
      return java_listener;
    }
  }
  return nullptr;
}

std::vector<jobject> DatabaseInternal::UnregisterAllChildEventListeners(
    const internal::QuerySpec& spec) {
  std::vector<jobject> java_listeners;
  std::vector<ChildListener*> listeners;
  if (child_listeners_by_query_.Get(spec, &listeners)) {
    for (int i = 0; i < listeners.size(); i++) {
      jobject java_listener = UnregisterChildEventListener(spec, listeners[i]);
      if (java_listener != nullptr) java_listeners.push_back(java_listener);
    }
  }
  return java_listeners;
}

jobject DatabaseInternal::CreateJavaTransactionHandler(
    TransactionData* transaction_fn) {
  MutexLock lock(transaction_mutex_);
  JNIEnv* env = app_->GetJNIEnv();
  jobject java_handler = env->NewObject(
      cpp_transaction_handler::GetClass(),
      cpp_transaction_handler::GetMethodId(
          cpp_transaction_handler::kConstructor),
      reinterpret_cast<jlong>(this), reinterpret_cast<jlong>(transaction_fn));
  jobject java_handler_global = env->NewGlobalRef(java_handler);
  env->DeleteLocalRef(java_handler);
  auto found = java_transaction_handlers_.find(java_handler_global);
  if (found == java_transaction_handlers_.end()) {
    java_transaction_handlers_.insert(java_handler_global);
  }
  transaction_fn->java_handler = java_handler_global;
  return java_handler_global;
}

void DatabaseInternal::DeleteJavaTransactionHandler(
    jobject java_handler_global) {
  MutexLock lock(transaction_mutex_);
  JNIEnv* env = app_->GetJNIEnv();
  auto found = java_transaction_handlers_.find(java_handler_global);
  if (found != java_transaction_handlers_.end()) {
    java_transaction_handlers_.erase(found);
  }
  jlong transaction_ptr = env->CallLongMethod(
      java_handler_global, cpp_transaction_handler::GetMethodId(
                               cpp_transaction_handler::kDiscardPointers));
  TransactionData* data = reinterpret_cast<TransactionData*>(transaction_ptr);
  delete data;
  env->DeleteGlobalRef(java_handler_global);
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
