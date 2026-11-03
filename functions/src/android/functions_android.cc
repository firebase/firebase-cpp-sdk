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

#include "functions/src/android/functions_android.h"

#include <jni.h>

#include <charconv>
#include <string>

#include "app/src/assert.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/util_android.h"
#include "functions/src/android/callable_reference_android.h"
#include "functions/src/include/firebase/functions/callable_reference.h"
#include "functions/src/include/firebase/functions/common.h"

namespace firebase {
namespace functions {
namespace internal {

// clang-format off
#define FUNCTIONS_METHODS(X)                                                   \
  X(GetInstance, "getInstance",                                                \
    "(Lcom/google/firebase/FirebaseApp;Ljava/lang/String;)"                    \
    "Lcom/google/firebase/functions/FirebaseFunctions;",                       \
    util::kMethodTypeStatic),                                                  \
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

METHOD_LOOKUP_DECLARATION(functions, FUNCTIONS_METHODS)
METHOD_LOOKUP_DEFINITION(functions,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/functions/FirebaseFunctions",
                         FUNCTIONS_METHODS)

// clang-format off
#define FUNCTIONS_EXCEPTION_METHODS(X)                                         \
  X(GetCode, "getCode",                                                        \
    "()Lcom/google/firebase/functions/FirebaseFunctionsException$Code;"),      \
  X(GetDetails, "getDetails", "()Ljava/lang/Object;")
// clang-format on

METHOD_LOOKUP_DECLARATION(functions_exception, FUNCTIONS_EXCEPTION_METHODS)
METHOD_LOOKUP_DEFINITION(
    functions_exception,
    PROGUARD_KEEP_CLASS
    "com/google/firebase/functions/FirebaseFunctionsException",
    FUNCTIONS_EXCEPTION_METHODS)

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

METHOD_LOOKUP_DECLARATION(code, CODE_METHODS, CODE_FIELDS)
METHOD_LOOKUP_DEFINITION(
    code,
    PROGUARD_KEEP_CLASS
    "com/google/firebase/functions/FirebaseFunctionsException$Code",
    CODE_METHODS, CODE_FIELDS)

// clang-format off
#define CALLABLE_OPTIONS_METHODS(X)                                            \
  X(GetLimitedUseAppCheckTokens, "getLimitedUseAppCheckTokens", "()Z")
// clang-format on

METHOD_LOOKUP_DECLARATION(options, CALLABLE_OPTIONS_METHODS)
METHOD_LOOKUP_DEFINITION(options,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/functions/HttpsCallableOptions",
                         CALLABLE_OPTIONS_METHODS)

// clang-format off
#define CALLABLE_OPTIONS_BUILDER_METHODS(X)                                    \
  X(Constructor, "<init>", "()V"),                                             \
  X(Build, "build",                                                            \
    "()Lcom/google/firebase/functions/HttpsCallableOptions;"),                 \
  X(SetLimitedUseAppCheckTokens, "setLimitedUseAppCheckTokens",                \
    "(Z)Lcom/google/firebase/functions/HttpsCallableOptions$Builder;")
// clang-format on

METHOD_LOOKUP_DECLARATION(options_builder, CALLABLE_OPTIONS_BUILDER_METHODS)
METHOD_LOOKUP_DEFINITION(
    options_builder,
    PROGUARD_KEEP_CLASS
    "com/google/firebase/functions/HttpsCallableOptions$Builder",
    CALLABLE_OPTIONS_BUILDER_METHODS)

// clang-format off
#define TIME_UNIT_FIELDS(X)                                                    \
  X(Milliseconds, "MILLISECONDS", "Ljava/util/concurrent/TimeUnit;",           \
    util::kFieldTypeStatic, util::kMethodOptional)
// clang-format on

METHOD_LOOKUP_DECLARATION(time_unit, METHOD_LOOKUP_NONE, TIME_UNIT_FIELDS)
METHOD_LOOKUP_DEFINITION(time_unit, "java/util/concurrent/TimeUnit",
                         METHOD_LOOKUP_NONE, TIME_UNIT_FIELDS)

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
  if (util::CheckAndClearJniExceptions(env) || !builder) {
    return nullptr;
  }
  jobject next_builder = env->CallObjectMethod(
      builder,
      options_builder::GetMethodId(
          options_builder::kSetLimitedUseAppCheckTokens),
      opts.limited_use_app_check_tokens ? JNI_TRUE : JNI_FALSE);
  if (util::CheckAndClearJniExceptions(env) || !next_builder) {
    env->DeleteLocalRef(builder);
    return nullptr;
  }
  builder = util::ContinueBuilder(env, builder, next_builder);
  jobject java_options = env->CallObjectMethod(
      builder, options_builder::GetMethodId(options_builder::kBuild));
  env->DeleteLocalRef(builder);
  if (util::CheckAndClearJniExceptions(env) || !java_options) {
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
  if (util::CheckAndClearJniExceptions(env) || !ms_unit) {
    return;
  }
  env->CallVoidMethod(callable_ref, set_timeout,
                      static_cast<jlong>(opts.timeout_ms), ms_unit);
  util::CheckAndClearJniExceptions(env);
  env->DeleteLocalRef(ms_unit);
}

}  // namespace

bool FunctionsInternal::Initialize(App* app) {
  MutexLock init_lock(init_mutex_);
  if (initialize_count_ == 0) {
    JNIEnv* env = app->GetJNIEnv();
    jobject activity = app->activity();
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
      util::CheckAndClearJniExceptions(env);
      return false;
    }

    // Cache error codes.
    code_map_ = new std::map<jobject, Error>();
    const auto cache_error_code = [env](code::Field code_field, Error error) {
      jclass code_class = code::GetClass();
      jobject code_object =
          env->GetStaticObjectField(code_class, code::GetFieldId(code_field));
      if (!util::CheckAndClearJniExceptions(env) && code_object) {
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

    util::CheckAndClearJniExceptions(env);
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
      for (const auto& entry : *code_map_) {
        env->DeleteGlobalRef(entry.first);
      }
      delete code_map_;
      code_map_ = nullptr;
    }

    util::CheckAndClearJniExceptions(env);
  }
}

FunctionsInternal::FunctionsInternal(App* app, const char* region)
    : region_(region ? region : "us-central1") {
  if (!app || !Initialize(app)) {
    return;
  }

  JNIEnv* env = app->GetJNIEnv();
  jobject platform_app = app->GetPlatformApp();
  jstring jregion = env->NewStringUTF(region_.c_str());
  if (util::CheckAndClearJniExceptions(env) || !jregion) {
    if (platform_app) env->DeleteLocalRef(platform_app);
    Terminate(app);
    return;
  }
  jobject instance = env->CallStaticObjectMethod(
      functions::GetClass(), functions::GetMethodId(functions::kGetInstance),
      platform_app, jregion);
  bool failed = util::CheckAndClearJniExceptions(env) || !instance;
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
  // If initialization failed, there is nothing to clean up.
  if (app_ == nullptr) return;

  JNIEnv* env = app_->GetJNIEnv();
  if (obj_) {
    env->DeleteGlobalRef(obj_);
    obj_ = nullptr;
  }
  util::CheckAndClearJniExceptions(env);

  Terminate(app_);
  app_ = nullptr;
}

HttpsCallableReferenceInternal* FunctionsInternal::GetHttpsCallable(
    const char* name) const {
  FIREBASE_ASSERT_RETURN(nullptr, is_valid());
  if (!name) return nullptr;
  JNIEnv* env = app_->GetJNIEnv();
  jstring name_string = env->NewStringUTF(name);
  if (util::CheckAndClearJniExceptions(env) || !name_string) {
    return nullptr;
  }
  jobject callable_reference_obj = env->CallObjectMethod(
      obj_, functions::GetMethodId(functions::kGetHttpsCallable), name_string);
  env->DeleteLocalRef(name_string);
  if (util::CheckAndClearJniExceptions(env) || !callable_reference_obj) {
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
  if (util::CheckAndClearJniExceptions(env) || !name_string) {
    env->DeleteLocalRef(java_options);
    return nullptr;
  }
  jobject callable_reference_obj = env->CallObjectMethod(
      obj_, functions::GetMethodId(functions::kGetHttpsCallableWithOptions),
      name_string, java_options);
  env->DeleteLocalRef(name_string);
  env->DeleteLocalRef(java_options);
  if (util::CheckAndClearJniExceptions(env) || !callable_reference_obj) {
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
  if (util::CheckAndClearJniExceptions(env) || !callable_reference_obj) {
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
  if (util::CheckAndClearJniExceptions(env) || !callable_reference_obj) {
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
    std::string str(origin);
    if (str.compare(0, 7, "http://") == 0) {
      str.erase(0, 7);
    } else if (str.compare(0, 8, "https://") == 0) {
      str.erase(0, 8);
    }
    while (!str.empty() && str.back() == '/') {
      str.pop_back();
    }
    size_t colon = str.rfind(':');
    if (colon != std::string::npos && colon > 0 && colon + 1 < str.size()) {
      int port = 0;
      const char* p_begin = str.data() + colon + 1;
      const char* p_end = str.data() + str.size();
      auto [ptr, ec] = std::from_chars(p_begin, p_end, port);
      if (ec == std::errc() && ptr == p_end && port > 0) {
        std::string host = str.substr(0, colon);
        jstring jhost = env->NewStringUTF(host.c_str());
        env->CallVoidMethod(obj_, use_emulator_id, jhost,
                            static_cast<jint>(port));
        env->DeleteLocalRef(jhost);
        util::CheckAndClearJniExceptions(env);
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
    util::CheckAndClearJniExceptions(env);
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
    *error_message = util::GetMessageFromException(env, java_exception);
    util::CheckAndClearJniExceptions(env);
  }
  if (env->IsInstanceOf(java_exception, functions_exception::GetClass())) {
    jobject code = env->CallObjectMethod(
        java_exception,
        functions_exception::GetMethodId(functions_exception::kGetCode));
    if (!util::CheckAndClearJniExceptions(env) && code != nullptr) {
      error = kErrorInternal;
      if (code_map_) {
        for (const auto& entry : *code_map_) {
          if (env->IsSameObject(code, entry.first)) {
            error = entry.second;
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
      if (!util::CheckAndClearJniExceptions(env) && jdetails != nullptr) {
        *details_out = util::JavaObjectToVariant(env, jdetails);
        util::CheckAndClearJniExceptions(env);
        env->DeleteLocalRef(jdetails);
      }
    }
  }
  if (error == kErrorNone) {
    error = kErrorInternal;
  }
  return error;
}

}  // namespace internal
}  // namespace functions
}  // namespace firebase
