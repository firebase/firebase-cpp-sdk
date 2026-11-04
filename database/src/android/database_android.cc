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
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/include/firebase/log.h"
#include "app/src/reference_counted_future_impl.h"
#include "app/src/util.h"
#include "app/src/app_android.h"
#include "app/src/jni/env.h"
#include "app/src/jni/ownership.h"
#include "database/src/android/data_snapshot_android.h"
#include "database/src/android/database_reference_android.h"
#include "database/src/android/disconnection_android.h"
#include "database/src/android/mutable_data_android.h"
#include "database/src/android/query_android.h"
#include "app/src/app_android.h"
#include "app/src/jni/env.h"
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
// clang-format on
namespace logger_level {

enum Method {
  kValueOf,
  kMethodCount
};

enum Field { kFieldCount = 0 };

::firebase::jni::Global<jclass> g_class;
jmethodID g_method_ids[kMethodCount];

jclass GetClass() { return g_class.get(); }
jmethodID GetMethodId(Method method) {
  return g_method_ids[method];
}

bool CacheMethodIds(JNIEnv* env, jobject activity) {
  if (g_class) return true;
  ::firebase::jni::Local<::firebase::jni::Class> local_clazz(env, env->FindClass("com/google/firebase/database/Logger$Level"));
  if (!local_clazz) {
    ::firebase::jni::CheckAndClearException(env);
    return false;
  }
  g_class = local_clazz;
  g_method_ids[kValueOf] = env->GetStaticMethodID(g_class.get(), "valueOf", "(Ljava/lang/String;)Lcom/google/firebase/database/Logger$Level;");
  if (!g_method_ids[kValueOf]) return false;
  return true;
}

void ReleaseClass(JNIEnv* env) {
  g_class.reset();
}

}  // namespace logger_level




// clang-format off
#define FIREBASE_DATABASE_METHODS(X)                                    \
  X(GetInstance, "getInstance",                                         \
    "(Lcom/google/firebase/FirebaseApp;)"                               \
    "Lcom/google/firebase/database/FirebaseDatabase;",                  \
    jni::kMethodTypeStatic),                                           \
  X(GetInstanceFromUrl, "getInstance",                                  \
    "(Lcom/google/firebase/FirebaseApp;Ljava/lang/String;)"             \
    "Lcom/google/firebase/database/FirebaseDatabase;",                  \
    jni::kMethodTypeStatic),                                           \
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
    jni::kMethodTypeStatic)
// clang-format on

namespace firebase_database {

enum Method {
  kGetInstance,
  kGetInstanceFromUrl,
  kGetApp,
  kGetRootReference,
  kGetReferenceFromPath,
  kGetReferenceFromUrl,
  kPurgeOutstandingWrites,
  kGoOnline,
  kGoOffline,
  kSetLogLevel,
  kSetPersistenceEnabled,
  kGetSdkVersion,
  kMethodCount
};

enum Field { kFieldCount = 0 };

::firebase::jni::Global<jclass> g_class;
jmethodID g_method_ids[kMethodCount];

jclass GetClass() { return g_class.get(); }
jmethodID GetMethodId(Method method) {
  return g_method_ids[method];
}

bool CacheMethodIds(JNIEnv* env, jobject activity) {
  if (g_class) return true;
  ::firebase::jni::Local<::firebase::jni::Class> local_clazz(env, env->FindClass("com/google/firebase/database/FirebaseDatabase"));
  if (!local_clazz) {
    ::firebase::jni::CheckAndClearException(env);
    return false;
  }
  g_class = local_clazz;
  g_method_ids[kGetInstance] = env->GetStaticMethodID(g_class.get(), "getInstance", "(Lcom/google/firebase/FirebaseApp;)Lcom/google/firebase/database/FirebaseDatabase;");
  if (!g_method_ids[kGetInstance]) return false;
  g_method_ids[kGetInstanceFromUrl] = env->GetStaticMethodID(g_class.get(), "getInstance", "(Lcom/google/firebase/FirebaseApp;Ljava/lang/String;)Lcom/google/firebase/database/FirebaseDatabase;");
  if (!g_method_ids[kGetInstanceFromUrl]) return false;
  g_method_ids[kGetApp] = env->GetMethodID(g_class.get(), "getApp", "()Lcom/google/firebase/FirebaseApp;");
  if (!g_method_ids[kGetApp]) return false;
  g_method_ids[kGetRootReference] = env->GetMethodID(g_class.get(), "getReference", "()Lcom/google/firebase/database/DatabaseReference;");
  if (!g_method_ids[kGetRootReference]) return false;
  g_method_ids[kGetReferenceFromPath] = env->GetMethodID(g_class.get(), "getReference", "(Ljava/lang/String;)Lcom/google/firebase/database/DatabaseReference;");
  if (!g_method_ids[kGetReferenceFromPath]) return false;
  g_method_ids[kGetReferenceFromUrl] = env->GetMethodID(g_class.get(), "getReferenceFromUrl", "(Ljava/lang/String;)Lcom/google/firebase/database/DatabaseReference;");
  if (!g_method_ids[kGetReferenceFromUrl]) return false;
  g_method_ids[kPurgeOutstandingWrites] = env->GetMethodID(g_class.get(), "purgeOutstandingWrites", "()V");
  if (!g_method_ids[kPurgeOutstandingWrites]) return false;
  g_method_ids[kGoOnline] = env->GetMethodID(g_class.get(), "goOnline", "()V");
  if (!g_method_ids[kGoOnline]) return false;
  g_method_ids[kGoOffline] = env->GetMethodID(g_class.get(), "goOffline", "()V");
  if (!g_method_ids[kGoOffline]) return false;
  g_method_ids[kSetLogLevel] = env->GetMethodID(g_class.get(), "setLogLevel", "(Lcom/google/firebase/database/Logger$Level;)V");
  if (!g_method_ids[kSetLogLevel]) return false;
  g_method_ids[kSetPersistenceEnabled] = env->GetMethodID(g_class.get(), "setPersistenceEnabled", "(Z)V");
  if (!g_method_ids[kSetPersistenceEnabled]) return false;
  g_method_ids[kGetSdkVersion] = env->GetStaticMethodID(g_class.get(), "getSdkVersion", "()Ljava/lang/String;");
  if (!g_method_ids[kGetSdkVersion]) return false;
  return true;
}

void ReleaseClass(JNIEnv* env) {
  g_class.reset();
}

}  // namespace firebase_database



// clang-format off
#define DATABASE_ERROR_METHODS(X)                                              \
    X(GetCode, "getCode", "()I"),                                              \
    X(GetMessage, "getMessage", "()Ljava/lang/String;"),                       \
    X(FromException, "fromException",                                          \
      "(Ljava/lang/Throwable;)Lcom/google/firebase/database/DatabaseError;",   \
      jni::kMethodTypeStatic, jni::kMethodOptional)
// clang-format on

// clang-format off
#define DATABASE_ERROR_FIELDS(X)                                               \
    X(Disconnected, "DISCONNECTED", "I", jni::kFieldTypeStatic)              \
    X(ExpiredToken, "EXPIRED_TOKEN", "I", jni::kFieldTypeStatic)             \
    X(InvalidToken, "INVALID_TOKEN", "I", jni::kFieldTypeStatic)             \
    X(MaxRetries, "MAX_RETRIES", "I", jni::kFieldTypeStatic)                 \
    X(NetworkError, "NETWORK_ERROR", "I", jni::kFieldTypeStatic)             \
    X(OperationFailed, "OPERATION_FAILED", "I", jni::kFieldTypeStatic)       \
    X(OverriddenBySet, "OVERRIDDEN_BY_SET", "I", jni::kFieldTypeStatic)      \
    X(PermissionDenied, "PERMISSION_DENIED", "I", jni::kFieldTypeStatic)     \
    X(Unavailable, "UNAVAILABLE", "I", jni::kFieldTypeStatic)                \
    X(UnknownError, "UNKNOWN_ERROR", "I", jni::kFieldTypeStatic)             \
    X(UserCodeException, "USER_CODE_EXCEPTION", "I", jni::kFieldTypeStatic)  \
    X(WriteCanceled, "WRITE_CANCELED", "I", jni::kFieldTypeStatic)
// clang-format on

namespace database_error {

enum Method {
  kGetCode,
  kGetMessage,
  kFromException,
  kMethodCount
};

enum Field {
  kDisconnected,
  kExpiredToken,
  kInvalidToken,
  kMaxRetries,
  kNetworkError,
  kOperationFailed,
  kOverriddenBySet,
  kPermissionDenied,
  kUnavailable,
  kUnknownError,
  kUserCodeException,
  kWriteCanceled,
  kFieldCount
};

::firebase::jni::Global<jclass> g_class;
jmethodID g_method_ids[kMethodCount];
jfieldID g_field_ids[kFieldCount];

jclass GetClass() { return g_class.get(); }
jmethodID GetMethodId(Method method) {
  return g_method_ids[method];
}

jfieldID GetFieldId(Field field) {
  return g_field_ids[field];
}

bool CacheMethodIds(JNIEnv* env, jobject activity) {
  if (g_class) return true;
  ::firebase::jni::Local<::firebase::jni::Class> local_clazz(env, env->FindClass("com/google/firebase/database/DatabaseError"));
  if (!local_clazz) {
    ::firebase::jni::CheckAndClearException(env);
    return false;
  }
  g_class = local_clazz;
  g_method_ids[kGetCode] = env->GetMethodID(g_class.get(), "getCode", "()I");
  if (!g_method_ids[kGetCode]) return false;
  g_method_ids[kGetMessage] = env->GetMethodID(g_class.get(), "getMessage", "()Ljava/lang/String;");
  if (!g_method_ids[kGetMessage]) return false;
  g_method_ids[kFromException] = env->GetStaticMethodID(g_class.get(), "fromException", "(Ljava/lang/Throwable;)Lcom/google/firebase/database/DatabaseError;");
  if (!g_method_ids[kFromException]) return false;
  g_field_ids[kDisconnected] = env->GetStaticFieldID(g_class.get(), "DISCONNECTED", "I");
  if (!g_field_ids[kDisconnected]) return false;
  g_field_ids[kExpiredToken] = env->GetStaticFieldID(g_class.get(), "EXPIRED_TOKEN", "I");
  if (!g_field_ids[kExpiredToken]) return false;
  g_field_ids[kInvalidToken] = env->GetStaticFieldID(g_class.get(), "INVALID_TOKEN", "I");
  if (!g_field_ids[kInvalidToken]) return false;
  g_field_ids[kMaxRetries] = env->GetStaticFieldID(g_class.get(), "MAX_RETRIES", "I");
  if (!g_field_ids[kMaxRetries]) return false;
  g_field_ids[kNetworkError] = env->GetStaticFieldID(g_class.get(), "NETWORK_ERROR", "I");
  if (!g_field_ids[kNetworkError]) return false;
  g_field_ids[kOperationFailed] = env->GetStaticFieldID(g_class.get(), "OPERATION_FAILED", "I");
  if (!g_field_ids[kOperationFailed]) return false;
  g_field_ids[kOverriddenBySet] = env->GetStaticFieldID(g_class.get(), "OVERRIDDEN_BY_SET", "I");
  if (!g_field_ids[kOverriddenBySet]) return false;
  g_field_ids[kPermissionDenied] = env->GetStaticFieldID(g_class.get(), "PERMISSION_DENIED", "I");
  if (!g_field_ids[kPermissionDenied]) return false;
  g_field_ids[kUnavailable] = env->GetStaticFieldID(g_class.get(), "UNAVAILABLE", "I");
  if (!g_field_ids[kUnavailable]) return false;
  g_field_ids[kUnknownError] = env->GetStaticFieldID(g_class.get(), "UNKNOWN_ERROR", "I");
  if (!g_field_ids[kUnknownError]) return false;
  g_field_ids[kUserCodeException] = env->GetStaticFieldID(g_class.get(), "USER_CODE_EXCEPTION", "I");
  if (!g_field_ids[kUserCodeException]) return false;
  g_field_ids[kWriteCanceled] = env->GetStaticFieldID(g_class.get(), "WRITE_CANCELED", "I");
  if (!g_field_ids[kWriteCanceled]) return false;
  return true;
}

void ReleaseClass(JNIEnv* env) {
  g_class.reset();
}

}  // namespace database_error



// clang-format off
#define CPP_TRANSACTION_HANDLER_METHODS(X)                                    \
    X(Constructor, "<init>", "(JJ)V"),                                        \
    X(DiscardPointers, "discardPointers", "()J")
// clang-format on
namespace cpp_transaction_handler {

enum Method {
  kConstructor,
  kDiscardPointers,
  kMethodCount
};

enum Field { kFieldCount = 0 };

::firebase::jni::Global<jclass> g_class;
jmethodID g_method_ids[kMethodCount];

jclass GetClass() { return g_class.get(); }
jmethodID GetMethodId(Method method) {
  return g_method_ids[method];
}

bool CacheMethodIds(JNIEnv* env, jobject activity) {
  if (g_class) return true;
  ::firebase::jni::Local<::firebase::jni::Class> local_clazz(env, env->FindClass("com/google/firebase/database/internal/cpp/CppTransactionHandler"));
  if (!local_clazz) {
    ::firebase::jni::CheckAndClearException(env);
    return false;
  }
  g_class = local_clazz;
  g_method_ids[kConstructor] = env->GetMethodID(g_class.get(), "<init>", "(JJ)V");
  if (!g_method_ids[kConstructor]) return false;
  g_method_ids[kDiscardPointers] = env->GetMethodID(g_class.get(), "discardPointers", "()V");
  if (!g_method_ids[kDiscardPointers]) return false;
  return true;
}

void ReleaseClass(JNIEnv* env) {
  g_class.reset();
}

}  // namespace cpp_transaction_handler



// clang-format off
#define CPP_EVENT_LISTENER_METHODS(X)                                    \
    X(DiscardPointers, "discardPointers", "()V")
// clang-format on
namespace cpp_event_listener {

enum Method {
  kDiscardPointers,
  kMethodCount
};

enum Field { kFieldCount = 0 };

::firebase::jni::Global<jclass> g_class;
jmethodID g_method_ids[kMethodCount];

jclass GetClass() { return g_class.get(); }
jmethodID GetMethodId(Method method) {
  return g_method_ids[method];
}

bool CacheMethodIds(JNIEnv* env, jobject activity) {
  if (g_class) return true;
  ::firebase::jni::Local<::firebase::jni::Class> local_clazz(env, env->FindClass("com/google/firebase/database/internal/cpp/CppEventListener"));
  if (!local_clazz) {
    ::firebase::jni::CheckAndClearException(env);
    return false;
  }
  g_class = local_clazz;
  g_method_ids[kDiscardPointers] = env->GetMethodID(g_class.get(), "discardPointers", "()V");
  if (!g_method_ids[kDiscardPointers]) return false;
  return true;
}

void ReleaseClass(JNIEnv* env) {
  g_class.reset();
}

}  // namespace cpp_event_listener



// clang-format off
#define CPP_VALUE_EVENT_LISTENER_METHODS(X)                                    \
    X(Constructor, "<init>", "(JJ)V")
// clang-format on
namespace cpp_value_event_listener {

enum Method {
  kConstructor,
  kMethodCount
};

enum Field { kFieldCount = 0 };

::firebase::jni::Global<jclass> g_class;
jmethodID g_method_ids[kMethodCount];

jclass GetClass() { return g_class.get(); }
jmethodID GetMethodId(Method method) {
  return g_method_ids[method];
}

bool CacheMethodIds(JNIEnv* env, jobject activity) {
  if (g_class) return true;
  ::firebase::jni::Local<::firebase::jni::Class> local_clazz(env, env->FindClass("com/google/firebase/database/internal/cpp/CppValueEventListener"));
  if (!local_clazz) {
    ::firebase::jni::CheckAndClearException(env);
    return false;
  }
  g_class = local_clazz;
  g_method_ids[kConstructor] = env->GetMethodID(g_class.get(), "<init>", "(JJ)V");
  if (!g_method_ids[kConstructor]) return false;
  return true;
}

void ReleaseClass(JNIEnv* env) {
  g_class.reset();
}

}  // namespace cpp_value_event_listener



// clang-format off
#define CPP_CHILD_EVENT_LISTENER_METHODS(X)                                    \
    X(Constructor, "<init>", "(JJ)V")
// clang-format on
namespace cpp_child_event_listener {

enum Method {
  kConstructor,
  kMethodCount
};

enum Field { kFieldCount = 0 };

::firebase::jni::Global<jclass> g_class;
jmethodID g_method_ids[kMethodCount];

jclass GetClass() { return g_class.get(); }
jmethodID GetMethodId(Method method) {
  return g_method_ids[method];
}

bool CacheMethodIds(JNIEnv* env, jobject activity) {
  if (g_class) return true;
  ::firebase::jni::Local<::firebase::jni::Class> local_clazz(env, env->FindClass("com/google/firebase/database/internal/cpp/CppChildEventListener"));
  if (!local_clazz) {
    ::firebase::jni::CheckAndClearException(env);
    return false;
  }
  g_class = local_clazz;
  g_method_ids[kConstructor] = env->GetMethodID(g_class.get(), "<init>", "(JJ)V");
  if (!g_method_ids[kConstructor]) return false;
  return true;
}

void ReleaseClass(JNIEnv* env) {
  g_class.reset();
}

}  // namespace cpp_child_event_listener



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
    // Something went wrong -> uninitialize the database
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
    // Something went wrong -> uninitialize the database
    Terminate(app_);
    app_ = nullptr;
  } else {
    obj_ = env->NewGlobalRef(database_obj);
    env->DeleteLocalRef(database_obj);
  }
  if (url_string != nullptr) env->DeleteLocalRef(url_string);
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
    {database_error::kUserCodeException, kErrorUnknownError},
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
  // Terminate() handles tearing this down.
  if (!(cpp_transaction_handler::CacheMethodIds(env, app->activity()) &&
        env->RegisterNatives(
            cpp_transaction_handler::GetClass(), kCppTransactionHandler,
            FIREBASE_ARRAYSIZE(kCppTransactionHandler)) == 0 &&
        cpp_event_listener::CacheMethodIds(env, app->activity()) &&
        cpp_value_event_listener::CacheMethodIds(env, app->activity()) &&
        env->RegisterNatives(
            cpp_value_event_listener::GetClass(), kCppValueEventListenerNatives,
            FIREBASE_ARRAYSIZE(kCppValueEventListenerNatives)) == 0 &&
        cpp_child_event_listener::CacheMethodIds(env, app->activity()) &&
        env->RegisterNatives(
            cpp_child_event_listener::GetClass(), kCppChildEventListenerNatives,
            FIREBASE_ARRAYSIZE(kCppChildEventListenerNatives)) == 0)) {
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
  safe_this_.ClearReference();
  // If initialization failed, there is nothing to clean up.
  if (app_ == nullptr) return;

  cleanup_.CleanupAll();

  JNIEnv* env = app_->GetJNIEnv();
  util::CancelCallbacks(env, jni_task_id_.c_str());
  {
    // If there are any pending listeners, delete their pointers.
    MutexLock lock(listener_mutex_);
    for (auto i = java_value_listener_lookup_.begin();
         i != java_value_listener_lookup_.end(); ++i) {
      ClearJavaEventListener(i->second);
      env->DeleteGlobalRef(i->second);
    }
    java_value_listener_lookup_.clear();
    for (auto i = java_child_listener_lookup_.begin();
         i != java_child_listener_lookup_.end(); ++i) {
      ClearJavaEventListener(i->second);
      env->DeleteGlobalRef(i->second);
    }
    java_child_listener_lookup_.clear();
    for (auto i = java_single_value_listeners_.begin();
         i != java_single_value_listeners_.end(); ++i) {
      ClearJavaEventListener(*i);
      env->DeleteGlobalRef(*i);
    }
    java_single_value_listeners_.clear();
    single_value_listeners_.clear();
  }
  {
    MutexLock lock(transaction_mutex_);
    for (auto i = java_transaction_handlers_.begin();
         i != java_transaction_handlers_.end(); ++i) {
      jlong transaction_ptr = env->CallLongMethod(
          *i, cpp_transaction_handler::GetMethodId(
                  cpp_transaction_handler::kDiscardPointers));
      ::firebase::jni::CheckAndClearException(env);
      TransactionData* data =
          reinterpret_cast<TransactionData*>(transaction_ptr);
      delete data;
      env->DeleteGlobalRef(*i);
    }
    java_transaction_handlers_.clear();
  }

  env->DeleteGlobalRef(obj_);
  obj_ = nullptr;
  Terminate(app_);
  app_ = nullptr;

  ::firebase::jni::CheckAndClearException(env);
}

App* DatabaseInternal::GetApp() const { return app_; }

DatabaseReference DatabaseInternal::GetReference() const {
  JNIEnv* env = app_->GetJNIEnv();
  jobject database_reference_obj = env->CallObjectMethod(
      obj_,
      firebase_database::GetMethodId(firebase_database::kGetRootReference));
  if (::firebase::jni::CheckAndClearException(env) ||
      database_reference_obj == nullptr) {
    if (database_reference_obj != nullptr) {
      env->DeleteLocalRef(database_reference_obj);
    }
    return DatabaseReference(nullptr);
  }
  DatabaseReferenceInternal* internal = new DatabaseReferenceInternal(
      const_cast<DatabaseInternal*>(this), database_reference_obj);
  env->DeleteLocalRef(database_reference_obj);
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
  jobject database_reference_obj = env->CallObjectMethod(
      obj_,
      firebase_database::GetMethodId(firebase_database::kGetReferenceFromPath),
      path_string);
  env->DeleteLocalRef(path_string);
  if (::firebase::jni::CheckAndClearException(env) ||
      database_reference_obj == nullptr) {
    if (database_reference_obj != nullptr) {
      env->DeleteLocalRef(database_reference_obj);
    }
    logger_.LogWarning("Database::GetReference(): Invalid path specified: %s",
                       path);
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
  if (::firebase::jni::CheckAndClearException(env) || url_string == nullptr) {
    if (url_string != nullptr) env->DeleteLocalRef(url_string);
    return DatabaseReference(nullptr);
  }
  jobject database_reference_obj = env->CallObjectMethod(
      obj_,
      firebase_database::GetMethodId(firebase_database::kGetReferenceFromUrl),
      url_string);
  env->DeleteLocalRef(url_string);
  if (::firebase::jni::CheckAndClearException(env) ||
      database_reference_obj == nullptr) {
    if (database_reference_obj != nullptr) {
      env->DeleteLocalRef(database_reference_obj);
    }
    logger_.LogWarning(
        "Database::GetReferenceFromUrl(): URL '%s' does not match the "
        "Database URL.",
        url);
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
  if (enum_name != nullptr) {
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
  return kErrorUnknownError;
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
  if (error == nullptr) {
    return kErrorUnknownError;
  }
  JNIEnv* env = app_->GetJNIEnv();

  if (error_message != nullptr) {
    jobject message = env->CallObjectMethod(
        error, database_error::GetMethodId(database_error::kGetMessage));
    if (::firebase::jni::CheckAndClearException(env)) {
      if (message != nullptr) env->DeleteLocalRef(message);
    } else if (message != nullptr) {
      *error_message = ::firebase::jni::JStringToString(env, message);
    }
  }
  jint java_error_code = env->CallIntMethod(
      error, database_error::GetMethodId(database_error::kGetCode));
  if (::firebase::jni::CheckAndClearException(env)) {
    return kErrorUnknownError;
  }
  return ErrorFromJavaErrorCode(java_error_code);
}

Error DatabaseInternal::ErrorFromJavaThrowable(
    jobject throwable, std::string* error_message) const {
  if (throwable == nullptr) {
    return kErrorUnknownError;
  }
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

jobject DatabaseInternal::CreateJavaEventListener(ValueListener* listener) {
  JNIEnv* env = app_->GetJNIEnv();
  jobject java_listener = env->NewObject(
      cpp_value_event_listener::GetClass(),
      cpp_value_event_listener::GetMethodId(
          cpp_value_event_listener::kConstructor),
      reinterpret_cast<jlong>(this), reinterpret_cast<jlong>(listener));
  if (::firebase::jni::CheckAndClearException(env) || java_listener == nullptr) {
    if (java_listener != nullptr) env->DeleteLocalRef(java_listener);
    return nullptr;
  }
  jobject java_listener_global = env->NewGlobalRef(java_listener);
  env->DeleteLocalRef(java_listener);
  if (::firebase::jni::CheckAndClearException(env)) {
    return nullptr;
  }
  return java_listener_global;
}

void DatabaseInternal::ClearJavaEventListener(jobject java_listener) {
  if (java_listener == nullptr) return;
  JNIEnv* env = app_->GetJNIEnv();
  env->CallVoidMethod(java_listener, cpp_event_listener::GetMethodId(
                                         cpp_event_listener::kDiscardPointers));
  ::firebase::jni::CheckAndClearException(env);
}

jobject DatabaseInternal::RegisterValueEventListener(
    const internal::QuerySpec& spec, ValueListener* listener) {
  MutexLock lock(listener_mutex_);
  JNIEnv* env = app_->GetJNIEnv();
  if (value_listeners_by_query_.Register(spec, listener)) {
    auto found = java_value_listener_lookup_.find(listener);
    if (found != java_value_listener_lookup_.end()) {
      return env->NewLocalRef(found->second);
    } else {
      jobject java_listener = CreateJavaEventListener(listener);
      if (java_listener == nullptr) {
        value_listeners_by_query_.Unregister(spec, listener);
        return nullptr;
      }
      java_value_listener_lookup_.insert(
          std::make_pair(listener, java_listener));
      return env->NewLocalRef(java_listener);
    }
  }
  return nullptr;
}

jobject DatabaseInternal::UnregisterValueEventListenerLocked(
    const internal::QuerySpec& spec, ValueListener* listener) {
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

jobject DatabaseInternal::UnregisterValueEventListener(
    const internal::QuerySpec& spec, ValueListener* listener) {
  MutexLock lock(listener_mutex_);
  return UnregisterValueEventListenerLocked(spec, listener);
}

std::vector<jobject> DatabaseInternal::UnregisterAllValueEventListeners(
    const internal::QuerySpec& spec) {
  MutexLock lock(listener_mutex_);
  std::vector<jobject> java_listeners;
  std::vector<ValueListener*> listeners;
  if (value_listeners_by_query_.Get(spec, &listeners)) {
    for (size_t i = 0; i < listeners.size(); i++) {
      jobject java_listener =
          UnregisterValueEventListenerLocked(spec, listeners[i]);
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
  if (::firebase::jni::CheckAndClearException(env) || java_listener == nullptr) {
    if (java_listener != nullptr) env->DeleteLocalRef(java_listener);
    return nullptr;
  }
  jobject java_listener_global = env->NewGlobalRef(java_listener);
  env->DeleteLocalRef(java_listener);
  if (::firebase::jni::CheckAndClearException(env)) {
    return nullptr;
  }
  return java_listener_global;
}

jobject DatabaseInternal::RegisterChildEventListener(
    const internal::QuerySpec& spec, ChildListener* listener) {
  MutexLock lock(listener_mutex_);
  JNIEnv* env = app_->GetJNIEnv();
  if (child_listeners_by_query_.Register(spec, listener)) {
    auto found = java_child_listener_lookup_.find(listener);
    if (found != java_child_listener_lookup_.end()) {
      return env->NewLocalRef(found->second);
    } else {
      jobject java_listener = CreateJavaEventListener(listener);
      if (java_listener == nullptr) {
        child_listeners_by_query_.Unregister(spec, listener);
        return nullptr;
      }
      java_child_listener_lookup_.insert(
          std::make_pair(listener, java_listener));
      return env->NewLocalRef(java_listener);
    }
  }
  return nullptr;
}

jobject DatabaseInternal::UnregisterChildEventListenerLocked(
    const internal::QuerySpec& spec, ChildListener* listener) {
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

jobject DatabaseInternal::UnregisterChildEventListener(
    const internal::QuerySpec& spec, ChildListener* listener) {
  MutexLock lock(listener_mutex_);
  return UnregisterChildEventListenerLocked(spec, listener);
}

std::vector<jobject> DatabaseInternal::UnregisterAllChildEventListeners(
    const internal::QuerySpec& spec) {
  MutexLock lock(listener_mutex_);
  std::vector<jobject> java_listeners;
  std::vector<ChildListener*> listeners;
  if (child_listeners_by_query_.Get(spec, &listeners)) {
    for (size_t i = 0; i < listeners.size(); i++) {
      jobject java_listener =
          UnregisterChildEventListenerLocked(spec, listeners[i]);
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
  if (::firebase::jni::CheckAndClearException(env) || java_handler == nullptr) {
    if (java_handler != nullptr) env->DeleteLocalRef(java_handler);
    return nullptr;
  }
  jobject java_handler_global = env->NewGlobalRef(java_handler);
  env->DeleteLocalRef(java_handler);
  if (::firebase::jni::CheckAndClearException(env) || java_handler_global == nullptr) {
    return nullptr;
  }
  auto found = java_transaction_handlers_.find(java_handler_global);
  if (found == java_transaction_handlers_.end()) {
    java_transaction_handlers_.insert(java_handler_global);
  }
  transaction_fn->java_handler = java_handler_global;
  return java_handler_global;
}

void DatabaseInternal::DeleteJavaTransactionHandler(
    jobject java_handler_global) {
  if (java_handler_global == nullptr) return;
  MutexLock lock(transaction_mutex_);
  JNIEnv* env = app_->GetJNIEnv();
  auto found = java_transaction_handlers_.find(java_handler_global);
  if (found != java_transaction_handlers_.end()) {
    java_transaction_handlers_.erase(found);
    jlong transaction_ptr = env->CallLongMethod(
        java_handler_global, cpp_transaction_handler::GetMethodId(
                                 cpp_transaction_handler::kDiscardPointers));
    ::firebase::jni::CheckAndClearException(env);
    TransactionData* data = reinterpret_cast<TransactionData*>(transaction_ptr);
    delete data;
    env->DeleteGlobalRef(java_handler_global);
  }
}

void Callbacks::ChildListenerNativeOnCancelled(JNIEnv* env, jclass clazz,
                                               jlong db_ptr, jlong listener_ptr,
                                               jobject database_error) {
  if (db_ptr != 0 && listener_ptr != 0) {
    DatabaseInternal* db = reinterpret_cast<DatabaseInternal*>(db_ptr);
    ChildListener* listener = reinterpret_cast<ChildListener*>(listener_ptr);
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
    DatabaseInternal* db = reinterpret_cast<DatabaseInternal*>(db_ptr);
    ChildListener* listener = reinterpret_cast<ChildListener*>(listener_ptr);
    std::string prev_key =
        previous_child_name ? ::firebase::jni::JStringToString(env, previous_child_name)
                            : std::string();
    listener->OnChildAdded(
        DataSnapshot(new DataSnapshotInternal(db, data_snapshot)),
        previous_child_name ? prev_key.c_str() : nullptr);
  }
}

void Callbacks::ChildListenerNativeOnChildChanged(JNIEnv* env, jclass clazz,
                                                  jlong db_ptr,
                                                  jlong listener_ptr,
                                                  jobject data_snapshot,
                                                  jobject previous_child_name) {
  (void)clazz;
  if (db_ptr != 0 && listener_ptr != 0) {
    DatabaseInternal* db = reinterpret_cast<DatabaseInternal*>(db_ptr);
    ChildListener* listener = reinterpret_cast<ChildListener*>(listener_ptr);
    std::string prev_key =
        previous_child_name ? ::firebase::jni::JStringToString(env, previous_child_name)
                            : std::string();
    listener->OnChildChanged(
        DataSnapshot(new DataSnapshotInternal(db, data_snapshot)),
        previous_child_name ? prev_key.c_str() : nullptr);
  }
}

void Callbacks::ChildListenerNativeOnChildMoved(JNIEnv* env, jclass clazz,
                                                jlong db_ptr,
                                                jlong listener_ptr,
                                                jobject data_snapshot,
                                                jobject previous_child_name) {
  (void)clazz;
  if (db_ptr != 0 && listener_ptr != 0) {
    DatabaseInternal* db = reinterpret_cast<DatabaseInternal*>(db_ptr);
    ChildListener* listener = reinterpret_cast<ChildListener*>(listener_ptr);
    std::string prev_key =
        previous_child_name ? ::firebase::jni::JStringToString(env, previous_child_name)
                            : std::string();
    listener->OnChildMoved(
        DataSnapshot(new DataSnapshotInternal(db, data_snapshot)),
        previous_child_name ? prev_key.c_str() : nullptr);
  }
}

void Callbacks::ChildListenerNativeOnChildRemoved(JNIEnv* env, jclass clazz,
                                                  jlong db_ptr,
                                                  jlong listener_ptr,
                                                  jobject data_snapshot) {
  (void)env;
  (void)clazz;
  if (db_ptr != 0 && listener_ptr != 0) {
    DatabaseInternal* db = reinterpret_cast<DatabaseInternal*>(db_ptr);
    ChildListener* listener = reinterpret_cast<ChildListener*>(listener_ptr);
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
    DatabaseInternal* db = reinterpret_cast<DatabaseInternal*>(db_ptr);
    ValueListener* listener = reinterpret_cast<ValueListener*>(listener_ptr);
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
    DatabaseInternal* db = reinterpret_cast<DatabaseInternal*>(db_ptr);
    ValueListener* listener = reinterpret_cast<ValueListener*>(listener_ptr);
    listener->OnValueChanged(
        DataSnapshot(new DataSnapshotInternal(db, data_snapshot)));
  }
}

jobject Callbacks::TransactionHandlerDoTransaction(JNIEnv* env, jclass clazz,
                                                   jlong db_ptr,
                                                   jlong transaction_data_ptr,
                                                   jobject java_mutable_data) {
  if (db_ptr != 0 && transaction_data_ptr != 0) {
    DatabaseInternal* db = reinterpret_cast<DatabaseInternal*>(db_ptr);
    TransactionData* data =
        reinterpret_cast<TransactionData*>(transaction_data_ptr);
    DoTransactionWithContext user_func = data->transaction;
    MutableData mutable_data = MutableDataInternal::MakeMutableData(
        new MutableDataInternal(db, java_mutable_data));
    TransactionResult result = user_func(&mutable_data, data->context);
    if (result == kTransactionResultSuccess) {
      return java_mutable_data;
    } else {
      return nullptr;
    }
  }
  return nullptr;
}

void Callbacks::TransactionHandlerOnComplete(
    JNIEnv* env, jclass clazz, jlong db_ptr, jlong transaction_data_ptr,
    jobject database_error, jboolean was_committed, jobject data_snapshot) {
  if (db_ptr != 0 && transaction_data_ptr != 0) {
    DatabaseInternal* db = reinterpret_cast<DatabaseInternal*>(db_ptr);
    TransactionData* data =
        reinterpret_cast<TransactionData*>(transaction_data_ptr);
    if (was_committed) {
      // Transaction success!
      jobject data_snapshot_global =
          data_snapshot ? env->NewGlobalRef(data_snapshot) : nullptr;
      data->future->Complete<DataSnapshot>(
          data->handle, kErrorNone, "",
          [db, data_snapshot_global](DataSnapshot* result) {
            if (data_snapshot_global != nullptr) {
              *result = DataSnapshot(
                  new DataSnapshotInternal(db, data_snapshot_global));
              db->GetApp()->GetJNIEnv()->DeleteGlobalRef(data_snapshot_global);
            } else {
              *result = DataSnapshot(nullptr);
            }
          });
    } else if (database_error == nullptr) {
      jobject data_snapshot_global =
          data_snapshot ? env->NewGlobalRef(data_snapshot) : nullptr;
      data->future->Complete<DataSnapshot>(
          data->handle, kErrorTransactionAbortedByUser,
          "The transaction was aborted, because the transaction function "
          "returned kTransactionResultAbort.",
          [db, data_snapshot_global](DataSnapshot* result) {
            if (data_snapshot_global != nullptr) {
              *result = DataSnapshot(
                  new DataSnapshotInternal(db, data_snapshot_global));
              db->GetApp()->GetJNIEnv()->DeleteGlobalRef(data_snapshot_global);
            } else {
              *result = DataSnapshot(nullptr);
            }
          });
    } else {
      // Future error.
      std::string error_message;
      Error error =
          db->ErrorFromJavaDatabaseError(database_error, &error_message);
      data->future->Complete(data->handle, error, error_message.c_str());
    }
    jobject java_handler_global = data->java_handler;
    db->DeleteJavaTransactionHandler(java_handler_global);  // deletes `data`.
  }
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
