#include "app/src/app_android.h"
// Copyright 2017 Google LLC
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

#include "app/src/jni/exception.h"
#include "functions/src/android/functions_android.h"

#include <jni.h>

#include <charconv>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

#include "app/src/assert.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/internal/common.h"
#include "functions/src/android/callable_reference_android.h"
#include "functions/src/include/firebase/functions/callable_reference.h"
#include "functions/src/include/firebase/functions/common.h"

namespace firebase::functions::internal {

// clang-format off
#define FUNCTIONS_METHODS(X)                                                   \
  X(GetInstance, "getInstance",                                                \
    "(Lcom/google/firebase/FirebaseApp;Ljava/lang/String;)"                    \
    "Lcom/google/firebase/functions/FirebaseFunctions;",                       \
    firebase::util::kMethodTypeStatic),                                                  \
  X(GetHttpsCallable, "getHttpsCallable",                                      \
    "(Ljava/lang/String;)"                                                     \
    "Lcom/google/firebase/functions/HttpsCallableReference;"),                 \
  X(GetHttpsCallableWithOptions, "getHttpsCallable",                           \
    "(Ljava/lang/String;"                                                      \
    "Lcom/google/firebase/functions/HttpsCallableOptions;)"                    \
    "Lcom/google/firebase/functions/HttpsCallableReference;"),                 \
  X(GetHttpsCallableFromUrl, "getHttpsCallableFromUrl",                        \
    "(Ljava/net/URL;)Lcom/google/firebase/functions/HttpsCallableReference;"), \
  X(GetHttpsCallableFromUrlWithOptions, "getHttpsCallableFromUrl",             \
    "(Ljava/net/URL;"                                                          \
    "Lcom/google/firebase/functions/HttpsCallableOptions;)"                    \
    "Lcom/google/firebase/functions/HttpsCallableReference;"),                 \
  X(UseFunctionsEmulator, "useFunctionsEmulator",                              \
    "(Ljava/lang/String;)V", util::kMethodTypeInstance,                        \
    util::kMethodOptional),                                                    \
  X(UseEmulator, "useEmulator",                                                \
    "(Ljava/lang/String;I)V", util::kMethodTypeInstance,                       \
    util::kMethodOptional)
// clang-format on

namespace functions {
  enum Method {
    kGetInstance,
    kGetHttpsCallable,
    kGetHttpsCallableWithOptions,
    kGetHttpsCallableFromUrl,
    kGetHttpsCallableFromUrlWithOptions,
    kUseFunctionsEmulator,
    kUseEmulator,
    kMethodCount
  };
  enum Field {
    kFieldCount
  };

  jclass GetClass();
  bool CacheMethodIds(JNIEnv* env, jobject activity);
  bool CacheFieldIds(JNIEnv* env, jobject activity);
  void ReleaseClass(JNIEnv* env);
  jmethodID GetMethodId(Method method);
  jfieldID GetFieldId(Field field);
}

namespace functions {
  ::firebase::jni::Global<jclass> g_class;
  jmethodID g_method_ids[kMethodCount];
  jfieldID g_field_ids[kFieldCount];
  jclass GetClass() { return g_class.get(); }
  jmethodID GetMethodId(Method method) { return g_method_ids[method]; }
  jfieldID GetFieldId(Field field) { return g_field_ids[field]; }
  bool CacheMethodIds(JNIEnv* env, jobject activity) {
    if (g_class) return true;
    ::firebase::jni::Local<::firebase::jni::Class> local_clazz(env, env->FindClass("com/google/firebase/functions/FirebaseFunctions"));
    if (!local_clazz) {
      ::firebase::jni::CheckAndClearException(env);
      return false;
    }
    g_class = local_clazz;
    g_method_ids[kGetInstance] = env->GetStaticMethodID(g_class.get(), "getInstance", "(Lcom/google/firebase/FirebaseApp;Ljava/lang/String;)Lcom/google/firebase/functions/FirebaseFunctions;");
    if (!g_method_ids[kGetInstance]) return false;
    g_method_ids[kGetHttpsCallable] = env->GetMethodID(g_class.get(), "getHttpsCallable", "(Ljava/lang/String;)Lcom/google/firebase/functions/HttpsCallableReference;");
    if (!g_method_ids[kGetHttpsCallable]) return false;
    g_method_ids[kGetHttpsCallableWithOptions] = env->GetMethodID(g_class.get(), "getHttpsCallable", "(Ljava/lang/String;Lcom/google/firebase/functions/HttpsCallableOptions;)Lcom/google/firebase/functions/HttpsCallableReference;");
    if (!g_method_ids[kGetHttpsCallableWithOptions]) return false;
    g_method_ids[kGetHttpsCallableFromUrl] = env->GetMethodID(g_class.get(), "getHttpsCallableFromUrl", "(Ljava/net/URL;)Lcom/google/firebase/functions/HttpsCallableReference;");
    if (!g_method_ids[kGetHttpsCallableFromUrl]) return false;
    g_method_ids[kGetHttpsCallableFromUrlWithOptions] = env->GetMethodID(g_class.get(), "getHttpsCallableFromUrl", "(Ljava/net/URL;Lcom/google/firebase/functions/HttpsCallableOptions;)Lcom/google/firebase/functions/HttpsCallableReference;");
    if (!g_method_ids[kGetHttpsCallableFromUrlWithOptions]) return false;
    g_method_ids[kUseFunctionsEmulator] = env->GetMethodID(g_class.get(), "useFunctionsEmulator", "(Ljava/lang/String;)V");
    ::firebase::jni::CheckAndClearException(env);
    g_method_ids[kUseEmulator] = env->GetMethodID(g_class.get(), "useEmulator", "(Ljava/lang/String;I)V");
    ::firebase::jni::CheckAndClearException(env);
    return CacheFieldIds(env, activity);
  }
  bool CacheFieldIds(JNIEnv* env, jobject activity) {
    if (!g_class) return false;
    return true;
  }
  void ReleaseClass(JNIEnv* env) {
    g_class = ::firebase::jni::Global<jclass>();
  }
}


// clang-format off
#define FUNCTIONS_EXCEPTION_METHODS(X)                                         \
  X(GetCode, "getCode",                                                        \
    "()Lcom/google/firebase/functions/FirebaseFunctionsException$Code;"),      \
  X(GetDetails, "getDetails", "()Ljava/lang/Object;")
// clang-format on

namespace functions_exception {
  enum Method {
    kGetCode,
    kGetDetails,
    kMethodCount
  };
  enum Field {
    kFieldCount
  };

  jclass GetClass();
  bool CacheMethodIds(JNIEnv* env, jobject activity);
  bool CacheFieldIds(JNIEnv* env, jobject activity);
  void ReleaseClass(JNIEnv* env);
  jmethodID GetMethodId(Method method);
  jfieldID GetFieldId(Field field);
}

namespace functions_exception {
  ::firebase::jni::Global<jclass> g_class;
  jmethodID g_method_ids[kMethodCount];
  jfieldID g_field_ids[kFieldCount];
  jclass GetClass() { return g_class.get(); }
  jmethodID GetMethodId(Method method) { return g_method_ids[method]; }
  jfieldID GetFieldId(Field field) { return g_field_ids[field]; }
  bool CacheMethodIds(JNIEnv* env, jobject activity) {
    if (g_class) return true;
    ::firebase::jni::Local<::firebase::jni::Class> local_clazz(env, env->FindClass("com/google/firebase/functions/FirebaseFunctionsException"));
    if (!local_clazz) {
      ::firebase::jni::CheckAndClearException(env);
      return false;
    }
    g_class = local_clazz;
    g_method_ids[kGetCode] = env->GetMethodID(g_class.get(), "getCode", "()Lcom/google/firebase/functions/FirebaseFunctionsException$Code;");
    if (!g_method_ids[kGetCode]) return false;
    g_method_ids[kGetDetails] = env->GetMethodID(g_class.get(), "getDetails", "()Ljava/lang/Object;");
    if (!g_method_ids[kGetDetails]) return false;
    return CacheFieldIds(env, activity);
  }
  bool CacheFieldIds(JNIEnv* env, jobject activity) {
    if (!g_class) return false;
    return true;
  }
  void ReleaseClass(JNIEnv* env) {
    g_class = ::firebase::jni::Global<jclass>();
  }
}


// clang-format off
#define CODE_METHODS(X)                                                        \
  X(Name, "name", "()Ljava/lang/String;")
#define CODE_FIELDS(X)                                                         \
  X(Ok, "OK",                                                                  \
    "Lcom/google/firebase/functions/FirebaseFunctionsException$Code;",         \
    util::kFieldTypeStatic),                                                   \
  X(Cancelled, "CANCELLED",                                                    \
    "Lcom/google/firebase/functions/FirebaseFunctionsException$Code;",         \
    util::kFieldTypeStatic),                                                   \
  X(Unknown, "UNKNOWN",                                                        \
    "Lcom/google/firebase/functions/FirebaseFunctionsException$Code;",         \
    util::kFieldTypeStatic),                                                   \
  X(InvalidArgument, "INVALID_ARGUMENT",                                       \
    "Lcom/google/firebase/functions/FirebaseFunctionsException$Code;",         \
    util::kFieldTypeStatic),                                                   \
  X(DeadlineExceeded, "DEADLINE_EXCEEDED",                                     \
    "Lcom/google/firebase/functions/FirebaseFunctionsException$Code;",         \
    util::kFieldTypeStatic),                                                   \
  X(NotFound, "NOT_FOUND",                                                     \
    "Lcom/google/firebase/functions/FirebaseFunctionsException$Code;",         \
    util::kFieldTypeStatic),                                                   \
  X(AlreadyExists, "ALREADY_EXISTS",                                           \
    "Lcom/google/firebase/functions/FirebaseFunctionsException$Code;",         \
    util::kFieldTypeStatic),                                                   \
  X(PermissionDenied, "PERMISSION_DENIED",                                     \
    "Lcom/google/firebase/functions/FirebaseFunctionsException$Code;",         \
    util::kFieldTypeStatic),                                                   \
  X(Unauthenticated, "UNAUTHENTICATED",                                        \
    "Lcom/google/firebase/functions/FirebaseFunctionsException$Code;",         \
    util::kFieldTypeStatic),                                                   \
  X(ResourceExhausted, "RESOURCE_EXHAUSTED",                                   \
    "Lcom/google/firebase/functions/FirebaseFunctionsException$Code;",         \
    util::kFieldTypeStatic),                                                   \
  X(FailedPrecondition, "FAILED_PRECONDITION",                                 \
    "Lcom/google/firebase/functions/FirebaseFunctionsException$Code;",         \
    util::kFieldTypeStatic),                                                   \
  X(Aborted, "ABORTED",                                                        \
    "Lcom/google/firebase/functions/FirebaseFunctionsException$Code;",         \
    util::kFieldTypeStatic),                                                   \
  X(OutOfRange, "OUT_OF_RANGE",                                                \
    "Lcom/google/firebase/functions/FirebaseFunctionsException$Code;",         \
    util::kFieldTypeStatic),                                                   \
  X(Unimplemented, "UNIMPLEMENTED",                                            \
    "Lcom/google/firebase/functions/FirebaseFunctionsException$Code;",         \
    util::kFieldTypeStatic),                                                   \
  X(Internal, "INTERNAL",                                                      \
    "Lcom/google/firebase/functions/FirebaseFunctionsException$Code;",         \
    util::kFieldTypeStatic),                                                   \
  X(Unavailable, "UNAVAILABLE",                                                \
    "Lcom/google/firebase/functions/FirebaseFunctionsException$Code;",         \
    util::kFieldTypeStatic),                                                   \
  X(DataLoss, "DATA_LOSS",                                                     \
    "Lcom/google/firebase/functions/FirebaseFunctionsException$Code;",         \
    util::kFieldTypeStatic)
// clang-format on

namespace code {
  enum Method {
    kName,
    kMethodCount
  };
  enum Field {
    kOk,
    kCancelled,
    kUnknown,
    kInvalidArgument,
    kDeadlineExceeded,
    kNotFound,
    kAlreadyExists,
    kPermissionDenied,
    kUnauthenticated,
    kResourceExhausted,
    kFailedPrecondition,
    kAborted,
    kOutOfRange,
    kUnimplemented,
    kInternal,
    kUnavailable,
    kDataLoss,
    kFieldCount
  };

  jclass GetClass();
  bool CacheMethodIds(JNIEnv* env, jobject activity);
  bool CacheFieldIds(JNIEnv* env, jobject activity);
  void ReleaseClass(JNIEnv* env);
  jmethodID GetMethodId(Method method);
  jfieldID GetFieldId(Field field);
}

namespace code {
  ::firebase::jni::Global<jclass> g_class;
  jmethodID g_method_ids[kMethodCount];
  jfieldID g_field_ids[kFieldCount];
  jclass GetClass() { return g_class.get(); }
  jmethodID GetMethodId(Method method) { return g_method_ids[method]; }
  jfieldID GetFieldId(Field field) { return g_field_ids[field]; }
  bool CacheMethodIds(JNIEnv* env, jobject activity) {
    if (g_class) return true;
    ::firebase::jni::Local<::firebase::jni::Class> local_clazz(env, env->FindClass("com/google/firebase/functions/FirebaseFunctionsException$Code"));
    if (!local_clazz) {
      ::firebase::jni::CheckAndClearException(env);
      return false;
    }
    g_class = local_clazz;
    g_method_ids[kName] = env->GetMethodID(g_class.get(), "name", "()Ljava/lang/String;");
    if (!g_method_ids[kName]) return false;
    return CacheFieldIds(env, activity);
  }
  bool CacheFieldIds(JNIEnv* env, jobject activity) {
    if (!g_class) return false;
    g_field_ids[kOk] = env->GetStaticFieldID(g_class.get(), "OK", "Lcom/google/firebase/functions/FirebaseFunctionsException$Code;");
    if (!g_field_ids[kOk]) return false;
    g_field_ids[kCancelled] = env->GetStaticFieldID(g_class.get(), "CANCELLED", "Lcom/google/firebase/functions/FirebaseFunctionsException$Code;");
    if (!g_field_ids[kCancelled]) return false;
    g_field_ids[kUnknown] = env->GetStaticFieldID(g_class.get(), "UNKNOWN", "Lcom/google/firebase/functions/FirebaseFunctionsException$Code;");
    if (!g_field_ids[kUnknown]) return false;
    g_field_ids[kInvalidArgument] = env->GetStaticFieldID(g_class.get(), "INVALID_ARGUMENT", "Lcom/google/firebase/functions/FirebaseFunctionsException$Code;");
    if (!g_field_ids[kInvalidArgument]) return false;
    g_field_ids[kDeadlineExceeded] = env->GetStaticFieldID(g_class.get(), "DEADLINE_EXCEEDED", "Lcom/google/firebase/functions/FirebaseFunctionsException$Code;");
    if (!g_field_ids[kDeadlineExceeded]) return false;
    g_field_ids[kNotFound] = env->GetStaticFieldID(g_class.get(), "NOT_FOUND", "Lcom/google/firebase/functions/FirebaseFunctionsException$Code;");
    if (!g_field_ids[kNotFound]) return false;
    g_field_ids[kAlreadyExists] = env->GetStaticFieldID(g_class.get(), "ALREADY_EXISTS", "Lcom/google/firebase/functions/FirebaseFunctionsException$Code;");
    if (!g_field_ids[kAlreadyExists]) return false;
    g_field_ids[kPermissionDenied] = env->GetStaticFieldID(g_class.get(), "PERMISSION_DENIED", "Lcom/google/firebase/functions/FirebaseFunctionsException$Code;");
    if (!g_field_ids[kPermissionDenied]) return false;
    g_field_ids[kUnauthenticated] = env->GetStaticFieldID(g_class.get(), "UNAUTHENTICATED", "Lcom/google/firebase/functions/FirebaseFunctionsException$Code;");
    if (!g_field_ids[kUnauthenticated]) return false;
    g_field_ids[kResourceExhausted] = env->GetStaticFieldID(g_class.get(), "RESOURCE_EXHAUSTED", "Lcom/google/firebase/functions/FirebaseFunctionsException$Code;");
    if (!g_field_ids[kResourceExhausted]) return false;
    g_field_ids[kFailedPrecondition] = env->GetStaticFieldID(g_class.get(), "FAILED_PRECONDITION", "Lcom/google/firebase/functions/FirebaseFunctionsException$Code;");
    if (!g_field_ids[kFailedPrecondition]) return false;
    g_field_ids[kAborted] = env->GetStaticFieldID(g_class.get(), "ABORTED", "Lcom/google/firebase/functions/FirebaseFunctionsException$Code;");
    if (!g_field_ids[kAborted]) return false;
    g_field_ids[kOutOfRange] = env->GetStaticFieldID(g_class.get(), "OUT_OF_RANGE", "Lcom/google/firebase/functions/FirebaseFunctionsException$Code;");
    if (!g_field_ids[kOutOfRange]) return false;
    g_field_ids[kUnimplemented] = env->GetStaticFieldID(g_class.get(), "UNIMPLEMENTED", "Lcom/google/firebase/functions/FirebaseFunctionsException$Code;");
    if (!g_field_ids[kUnimplemented]) return false;
    g_field_ids[kInternal] = env->GetStaticFieldID(g_class.get(), "INTERNAL", "Lcom/google/firebase/functions/FirebaseFunctionsException$Code;");
    if (!g_field_ids[kInternal]) return false;
    g_field_ids[kUnavailable] = env->GetStaticFieldID(g_class.get(), "UNAVAILABLE", "Lcom/google/firebase/functions/FirebaseFunctionsException$Code;");
    if (!g_field_ids[kUnavailable]) return false;
    g_field_ids[kDataLoss] = env->GetStaticFieldID(g_class.get(), "DATA_LOSS", "Lcom/google/firebase/functions/FirebaseFunctionsException$Code;");
    if (!g_field_ids[kDataLoss]) return false;
    return true;
  }
  void ReleaseClass(JNIEnv* env) {
    g_class = ::firebase::jni::Global<jclass>();
  }
}


// clang-format off
#define CALLABLE_OPTIONS_METHODS(X)                                            \
  X(GetLimitedUseAppCheckTokens, "getLimitedUseAppCheckTokens", "()Z")
// clang-format on

namespace options {
  enum Method {
    kGetLimitedUseAppCheckTokens,
    kMethodCount
  };
  enum Field {
    kFieldCount
  };

  jclass GetClass();
  bool CacheMethodIds(JNIEnv* env, jobject activity);
  bool CacheFieldIds(JNIEnv* env, jobject activity);
  void ReleaseClass(JNIEnv* env);
  jmethodID GetMethodId(Method method);
  jfieldID GetFieldId(Field field);
}

namespace options {
  ::firebase::jni::Global<jclass> g_class;
  jmethodID g_method_ids[kMethodCount];
  jfieldID g_field_ids[kFieldCount];
  jclass GetClass() { return g_class.get(); }
  jmethodID GetMethodId(Method method) { return g_method_ids[method]; }
  jfieldID GetFieldId(Field field) { return g_field_ids[field]; }
  bool CacheMethodIds(JNIEnv* env, jobject activity) {
    if (g_class) return true;
    ::firebase::jni::Local<::firebase::jni::Class> local_clazz(env, env->FindClass("com/google/firebase/functions/HttpsCallableOptions"));
    if (!local_clazz) {
      ::firebase::jni::CheckAndClearException(env);
      return false;
    }
    g_class = local_clazz;
    g_method_ids[kGetLimitedUseAppCheckTokens] = env->GetMethodID(g_class.get(), "getLimitedUseAppCheckTokens", "()Z");
    if (!g_method_ids[kGetLimitedUseAppCheckTokens]) return false;
    return CacheFieldIds(env, activity);
  }
  bool CacheFieldIds(JNIEnv* env, jobject activity) {
    if (!g_class) return false;
    return true;
  }
  void ReleaseClass(JNIEnv* env) {
    g_class = ::firebase::jni::Global<jclass>();
  }
}


// clang-format off
#define CALLABLE_OPTIONS_BUILDER_METHODS(X)                                    \
  X(Constructor, "<init>", "()V"),                                             \
  X(Build, "build",                                                            \
    "()Lcom/google/firebase/functions/HttpsCallableOptions;"),                 \
  X(SetLimitedUseAppCheckTokens, "setLimitedUseAppCheckTokens",                \
    "(Z)Lcom/google/firebase/functions/HttpsCallableOptions$Builder;")
// clang-format on

namespace options_builder {
  enum Method {
    kConstructor,
    kBuild,
    kSetLimitedUseAppCheckTokens,
    kMethodCount
  };
  enum Field {
    kFieldCount
  };

  jclass GetClass();
  bool CacheMethodIds(JNIEnv* env, jobject activity);
  bool CacheFieldIds(JNIEnv* env, jobject activity);
  void ReleaseClass(JNIEnv* env);
  jmethodID GetMethodId(Method method);
  jfieldID GetFieldId(Field field);
}

namespace options_builder {
  ::firebase::jni::Global<jclass> g_class;
  jmethodID g_method_ids[kMethodCount];
  jfieldID g_field_ids[kFieldCount];
  jclass GetClass() { return g_class.get(); }
  jmethodID GetMethodId(Method method) { return g_method_ids[method]; }
  jfieldID GetFieldId(Field field) { return g_field_ids[field]; }
  bool CacheMethodIds(JNIEnv* env, jobject activity) {
    if (g_class) return true;
    ::firebase::jni::Local<::firebase::jni::Class> local_clazz(env, env->FindClass("com/google/firebase/functions/HttpsCallableOptions$Builder"));
    if (!local_clazz) {
      ::firebase::jni::CheckAndClearException(env);
      return false;
    }
    g_class = local_clazz;
    g_method_ids[kConstructor] = env->GetMethodID(g_class.get(), "<init>", "()V");
    if (!g_method_ids[kConstructor]) return false;
    g_method_ids[kBuild] = env->GetMethodID(g_class.get(), "build", "()Lcom/google/firebase/functions/HttpsCallableOptions;");
    if (!g_method_ids[kBuild]) return false;
    g_method_ids[kSetLimitedUseAppCheckTokens] = env->GetMethodID(g_class.get(), "setLimitedUseAppCheckTokens", "(Z)Lcom/google/firebase/functions/HttpsCallableOptions$Builder;");
    if (!g_method_ids[kSetLimitedUseAppCheckTokens]) return false;
    return CacheFieldIds(env, activity);
  }
  bool CacheFieldIds(JNIEnv* env, jobject activity) {
    if (!g_class) return false;
    return true;
  }
  void ReleaseClass(JNIEnv* env) {
    g_class = ::firebase::jni::Global<jclass>();
  }
}


// clang-format off
#define TIME_UNIT_FIELDS(X)                                                    \
  X(Milliseconds, "MILLISECONDS", "Ljava/util/concurrent/TimeUnit;",           \
    util::kFieldTypeStatic, util::kMethodOptional)
// clang-format on

namespace time_unit {
  enum Method {
    kMethodCount
  };
  enum Field {
    kMilliseconds,
    kFieldCount
  };

  jclass GetClass();
  bool CacheMethodIds(JNIEnv* env, jobject activity);
  bool CacheFieldIds(JNIEnv* env, jobject activity);
  void ReleaseClass(JNIEnv* env);
  jmethodID GetMethodId(Method method);
  jfieldID GetFieldId(Field field);
}

namespace time_unit {
  ::firebase::jni::Global<jclass> g_class;
  jmethodID g_method_ids[kMethodCount];
  jfieldID g_field_ids[kFieldCount];
  jclass GetClass() { return g_class.get(); }
  jmethodID GetMethodId(Method method) { return g_method_ids[method]; }
  jfieldID GetFieldId(Field field) { return g_field_ids[field]; }
  bool CacheMethodIds(JNIEnv* env, jobject activity) {
    if (g_class) return true;
    ::firebase::jni::Local<::firebase::jni::Class> local_clazz(env, env->FindClass("java/util/concurrent/TimeUnit"));
    if (!local_clazz) {
      ::firebase::jni::CheckAndClearException(env);
      return false;
    }
    g_class = local_clazz;
    return CacheFieldIds(env, activity);
  }
  bool CacheFieldIds(JNIEnv* env, jobject activity) {
    if (!g_class) return false;
    g_field_ids[kMilliseconds] = env->GetStaticFieldID(g_class.get(), "MILLISECONDS", "Ljava/util/concurrent/TimeUnit;");
    if (!g_field_ids[kMilliseconds]) return false;
    return true;
  }
  void ReleaseClass(JNIEnv* env) {
    g_class = ::firebase::jni::Global<jclass>();
  }
}


Mutex FunctionsInternal::init_mutex_;
int FunctionsInternal::initialize_count_ = 0;
std::map<jobject, Error>* FunctionsInternal::code_map_ = nullptr;

namespace {

void ReleaseClasses(JNIEnv* env) {
  functions::ReleaseClass(env);
  callable_reference::ReleaseClass(env);
  callable_result::ReleaseClass(env);
  functions_exception::ReleaseClass(env);
  code::ReleaseClass(env);
  options::ReleaseClass(env);
  options_builder::ReleaseClass(env);
  time_unit::ReleaseClass(env);
}

jobject BuildJavaOptions(JNIEnv* env, const HttpsCallableOptions& opts) {
  jobject builder = env->NewObject(
      options_builder::GetClass(),
      options_builder::GetMethodId(options_builder::kConstructor));
  if (jni::CheckAndClearException(env) || !builder) {
    return nullptr;
  }
  jobject next_builder = env->CallObjectMethod(
      builder,
      options_builder::GetMethodId(
          options_builder::kSetLimitedUseAppCheckTokens),
      opts.limited_use_app_check_token ? JNI_TRUE : JNI_FALSE);
  if (jni::CheckAndClearException(env) || !next_builder) {
    env->DeleteLocalRef(builder);
    return nullptr;
  }
  builder = util::ContinueBuilder(env, builder, next_builder);
  jobject java_options = env->CallObjectMethod(
      builder, options_builder::GetMethodId(options_builder::kBuild));
  env->DeleteLocalRef(builder);
  if (jni::CheckAndClearException(env) || !java_options) {
    return nullptr;
  }
  return java_options;
}

void ApplyJavaTimeout(JNIEnv* env, jobject callable_ref,
                      const HttpsCallableOptions& opts) {
  if (!callable_ref || opts.timeout_ms <= 0 ||
      opts.timeout_ms == kDefaultTimeoutMilliseconds) {
    return;
  }
  jmethodID set_timeout =
      callable_reference::GetMethodId(callable_reference::kSetTimeout);
  jfieldID ms_field = time_unit::GetFieldId(time_unit::kMilliseconds);
  if (!set_timeout || !ms_field || !time_unit::GetClass()) {
    return;
  }
  jobject ms_unit =
      env->GetStaticObjectField(time_unit::GetClass(), ms_field);
  if (jni::CheckAndClearException(env) || !ms_unit) {
    return;
  }
  env->CallVoidMethod(callable_ref, set_timeout,
                      static_cast<jlong>(opts.timeout_ms), ms_unit);
  jni::CheckAndClearException(env);
  env->DeleteLocalRef(ms_unit);
}

}  // namespace

bool FunctionsInternal::Initialize(App* app) {
  MutexLock init_lock(init_mutex_);
  if (initialize_count_ == 0) {
    JNIEnv* env = app->GetJNIEnv();
    jobject activity = app->activity();
    if (!true) {
      jni::CheckAndClearException(env);
      return false;
    }
    bool ok = functions::CacheMethodIds(env, activity) &&
              callable_reference::CacheMethodIds(env, activity) &&
              callable_result::CacheMethodIds(env, activity) &&
              functions_exception::CacheMethodIds(env, activity) &&
              code::CacheMethodIds(env, activity) &&
              code::CacheFieldIds(env, activity) &&
              options::CacheMethodIds(env, activity) &&
              options_builder::CacheMethodIds(env, activity) &&
              time_unit::CacheFieldIds(env, activity);
    if (!ok) {
      ReleaseClasses(env);
      jni::CheckAndClearException(env);
      
      return false;
    }

    // Cache error codes.
    code_map_ = new std::map<jobject, Error>();
    const auto cache_error_code = [env](code::Field code_field, Error error) {
      jclass code_class = code::GetClass();
      jobject code_object =
          env->GetStaticObjectField(code_class, code::GetFieldId(code_field));
      if (!jni::CheckAndClearException(env) && code_object) {
        (*code_map_)[env->NewGlobalRef(code_object)] = error;
        env->DeleteLocalRef(code_object);
      }
    };
    cache_error_code(code::kOk, kErrorNone);
    cache_error_code(code::kCancelled, kErrorCancelled);
    cache_error_code(code::kUnknown, kErrorUnknown);
    cache_error_code(code::kInvalidArgument, kErrorInvalidArgument);
    cache_error_code(code::kDeadlineExceeded, kErrorDeadlineExceeded);
    cache_error_code(code::kNotFound, kErrorNotFound);
    cache_error_code(code::kAlreadyExists, kErrorAlreadyExists);
    cache_error_code(code::kPermissionDenied, kErrorPermissionDenied);
    cache_error_code(code::kUnauthenticated, kErrorUnauthenticated);
    cache_error_code(code::kResourceExhausted, kErrorResourceExhausted);
    cache_error_code(code::kFailedPrecondition, kErrorFailedPrecondition);
    cache_error_code(code::kAborted, kErrorAborted);
    cache_error_code(code::kOutOfRange, kErrorOutOfRange);
    cache_error_code(code::kUnimplemented, kErrorUnimplemented);
    cache_error_code(code::kInternal, kErrorInternal);
    cache_error_code(code::kUnavailable, kErrorUnavailable);
    cache_error_code(code::kDataLoss, kErrorDataLoss);

    jni::CheckAndClearException(env);
  }
  initialize_count_++;
  return true;
}

void FunctionsInternal::Terminate(App* app) {
  MutexLock init_lock(init_mutex_);
  FIREBASE_ASSERT(initialize_count_ > 0);
  initialize_count_--;
  if (initialize_count_ == 0) {
    JNIEnv* env = app->GetJNIEnv();
    ReleaseClasses(env);

    if (code_map_) {
      for (const auto& [code_obj, mapped_error] : *code_map_) {
        (void)mapped_error;
        env->DeleteGlobalRef(code_obj);
      }
      delete code_map_;
      code_map_ = nullptr;
    }

    jni::CheckAndClearException(env);
    
  }
}

FunctionsInternal::FunctionsInternal(App* app, const char* region)
    : region_(region ? region : "us-central1"),
      lifetime_state_(std::make_shared<FunctionsLifetimeState>()) {
  jni_task_id_ = "Functions_" + std::to_string(reinterpret_cast<uintptr_t>(this));
  if (!app || !Initialize(app)) {
    return;
  }

  JNIEnv* env = app->GetJNIEnv();
  jobject platform_app = app->GetPlatformApp();
  jstring jregion = env->NewStringUTF(region_.c_str());
  if (jni::CheckAndClearException(env) || !jregion) {
    if (platform_app) env->DeleteLocalRef(platform_app);
    Terminate(app);
    return;
  }
  jobject instance = env->CallStaticObjectMethod(
      functions::GetClass(), functions::GetMethodId(functions::kGetInstance),
      platform_app, jregion);
  bool failed = jni::CheckAndClearException(env) || !instance;
  env->DeleteLocalRef(jregion);
  if (platform_app) env->DeleteLocalRef(platform_app);

  if (failed) {
    Terminate(app);
    return;
  }

  app_ = app;
  obj_ = env->NewGlobalRef(instance);
  env->DeleteLocalRef(instance);
}

FunctionsInternal::~FunctionsInternal() {
  {
    MutexLock lock(lifetime_state_->mutex);
    lifetime_state_->alive = false;
  }
  // If initialization failed, there is nothing else to clean up.
  if (app_ == nullptr) return;

  JNIEnv* env = app_->GetJNIEnv();
  // Cancel any pending JNI Task callbacks registered by this instance so their
  // callback data is released, then clean up live HttpsCallableReferences.
  firebase::util::CancelCallbacks(env, jni_task_id_.c_str());  if (obj_) {
    env->DeleteGlobalRef(obj_);
    obj_ = nullptr;
  }
  jni::CheckAndClearException(env);

  Terminate(app_);
  app_ = nullptr;
}

HttpsCallableReferenceInternal* FunctionsInternal::GetHttpsCallable(
    const char* name) const {
  FIREBASE_ASSERT_RETURN(nullptr, is_valid());
  if (!name) return nullptr;
  JNIEnv* env = app_->GetJNIEnv();
  jstring name_string = env->NewStringUTF(name);
  if (jni::CheckAndClearException(env) || !name_string) {
    return nullptr;
  }
  jobject callable_reference_obj = env->CallObjectMethod(
      obj_, functions::GetMethodId(functions::kGetHttpsCallable), name_string);
  env->DeleteLocalRef(name_string);
  if (jni::CheckAndClearException(env) || !callable_reference_obj) {
    return nullptr;
  }
  HttpsCallableReferenceInternal* callable_reference =
      new HttpsCallableReferenceInternal(const_cast<FunctionsInternal*>(this),
                                         callable_reference_obj);
  env->DeleteLocalRef(callable_reference_obj);
  return callable_reference;
}

HttpsCallableReferenceInternal* FunctionsInternal::GetHttpsCallableWithOptions(
    const char* name, const HttpsCallableOptions& opts) const {
  FIREBASE_ASSERT_RETURN(nullptr, is_valid());
  if (!name) return nullptr;
  JNIEnv* env = app_->GetJNIEnv();
  jobject java_options = BuildJavaOptions(env, opts);
  if (!java_options) return nullptr;

  jstring name_string = env->NewStringUTF(name);
  if (jni::CheckAndClearException(env) || !name_string) {
    env->DeleteLocalRef(java_options);
    return nullptr;
  }
  jobject callable_reference_obj = env->CallObjectMethod(
      obj_, functions::GetMethodId(functions::kGetHttpsCallableWithOptions),
      name_string, java_options);
  env->DeleteLocalRef(name_string);
  env->DeleteLocalRef(java_options);
  if (jni::CheckAndClearException(env) || !callable_reference_obj) {
    return nullptr;
  }
  ApplyJavaTimeout(env, callable_reference_obj, opts);
  HttpsCallableReferenceInternal* callable_reference =
      new HttpsCallableReferenceInternal(const_cast<FunctionsInternal*>(this),
                                         callable_reference_obj);
  env->DeleteLocalRef(callable_reference_obj);
  return callable_reference;
}

HttpsCallableReferenceInternal* FunctionsInternal::GetHttpsCallableFromURL(
    const char* url) const {
  FIREBASE_ASSERT_RETURN(nullptr, is_valid());
  if (!url) return nullptr;
  JNIEnv* env = app_->GetJNIEnv();
  jobject url_object = util::CharsToURL(env, url);
  if (!url_object) return nullptr;

  jobject callable_reference_obj = env->CallObjectMethod(
      obj_, functions::GetMethodId(functions::kGetHttpsCallableFromUrl),
      url_object);
  env->DeleteLocalRef(url_object);
  if (jni::CheckAndClearException(env) || !callable_reference_obj) {
    return nullptr;
  }
  HttpsCallableReferenceInternal* callable_reference =
      new HttpsCallableReferenceInternal(const_cast<FunctionsInternal*>(this),
                                         callable_reference_obj);
  env->DeleteLocalRef(callable_reference_obj);
  return callable_reference;
}

HttpsCallableReferenceInternal*
FunctionsInternal::GetHttpsCallableFromURLWithOptions(
    const char* url, const HttpsCallableOptions& opts) const {
  FIREBASE_ASSERT_RETURN(nullptr, is_valid());
  if (!url) return nullptr;
  JNIEnv* env = app_->GetJNIEnv();
  jobject url_object = util::CharsToURL(env, url);
  if (!url_object) return nullptr;

  jobject java_options = BuildJavaOptions(env, opts);
  if (!java_options) {
    env->DeleteLocalRef(url_object);
    return nullptr;
  }

  jobject callable_reference_obj = env->CallObjectMethod(
      obj_,
      functions::GetMethodId(functions::kGetHttpsCallableFromUrlWithOptions),
      url_object, java_options);
  env->DeleteLocalRef(url_object);
  env->DeleteLocalRef(java_options);
  if (jni::CheckAndClearException(env) || !callable_reference_obj) {
    return nullptr;
  }
  ApplyJavaTimeout(env, callable_reference_obj, opts);
  HttpsCallableReferenceInternal* callable_reference =
      new HttpsCallableReferenceInternal(const_cast<FunctionsInternal*>(this),
                                         callable_reference_obj);
  env->DeleteLocalRef(callable_reference_obj);
  return callable_reference;
}

void FunctionsInternal::UseFunctionsEmulator(const char* origin) {
  FIREBASE_ASSERT_RETURN_VOID(is_valid());
  if (!origin || origin[0] == '\0') return;
  JNIEnv* env = app_->GetJNIEnv();

  // Prefer the non-deprecated useEmulator(String host, int port) if available.
  jmethodID use_emulator_id =
      functions::GetMethodId(functions::kUseEmulator);
  if (use_emulator_id) {
    std::string_view sv(origin);
    if (sv.substr(0, 7) == "http://") {
      sv.remove_prefix(7);
    } else if (sv.substr(0, 8) == "https://") {
      sv.remove_prefix(8);
    }
    while (!sv.empty() && sv.back() == '/') {
      sv.remove_suffix(1);
    }
    size_t colon = sv.rfind(':');
    if (colon != std::string_view::npos && colon > 0 && colon + 1 < sv.size()) {
      int port = 0;
      const char* p_begin = sv.data() + colon + 1;
      const char* p_end = sv.data() + sv.size();
      auto [ptr, ec] = std::from_chars(p_begin, p_end, port);
      if (ec == std::errc() && ptr == p_end && port > 0) {
        std::string host(sv.substr(0, colon));
        jstring jhost = env->NewStringUTF(host.c_str());
        env->CallVoidMethod(obj_, use_emulator_id, jhost,
                            static_cast<jint>(port));
        env->DeleteLocalRef(jhost);
        jni::CheckAndClearException(env);
        return;
      }
    }
  }

  jmethodID legacy_id =
      functions::GetMethodId(functions::kUseFunctionsEmulator);
  if (legacy_id) {
    jstring jorigin = env->NewStringUTF(origin);
    env->CallVoidMethod(obj_, legacy_id, jorigin);
    env->DeleteLocalRef(jorigin);
    jni::CheckAndClearException(env);
  }
}

Error FunctionsInternal::ErrorFromJavaFunctionsException(
    JNIEnv* env, jobject java_exception, std::string* error_message,
    Variant* details_out) {
  if (!java_exception) {
    return kErrorNone;
  }
  Error error = kErrorUnknown;
  if (error_message != nullptr) {
    *error_message = ::firebase::jni::ExtractExceptionMessage(env, static_cast<jthrowable>(java_exception));
    jni::CheckAndClearException(env);
  }
  if (env->IsInstanceOf(java_exception, functions_exception::GetClass())) {
    jobject code = env->CallObjectMethod(
        java_exception,
        functions_exception::GetMethodId(functions_exception::kGetCode));
    if (!jni::CheckAndClearException(env) && code != nullptr) {
      error = kErrorInternal;
      MutexLock init_lock(init_mutex_);
      if (code_map_) {
        for (const auto& [code_obj, mapped_error] : *code_map_) {
          if (env->IsSameObject(code, code_obj)) {
            error = mapped_error;
            break;
          }
        }
      }
      env->DeleteLocalRef(code);
    } else {
      error = kErrorInternal;
    }

    if (details_out != nullptr) {
      jobject jdetails = env->CallObjectMethod(
          java_exception,
          functions_exception::GetMethodId(functions_exception::kGetDetails));
      if (!jni::CheckAndClearException(env) && jdetails != nullptr) {
        *details_out = firebase::util::JavaObjectToVariant(env, jdetails);
        jni::CheckAndClearException(env);
        env->DeleteLocalRef(jdetails);
      }
    }
  }
  if (error == kErrorNone) {
    error = kErrorInternal;
  }
  return error;
}

}  // namespace firebase::functions::internal
