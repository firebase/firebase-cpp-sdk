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
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "app/src/app_android.h"
#include "app/src/assert.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/jni/declaration.h"
#include "app/src/jni/exception.h"
#include "functions/src/android/callable_reference_android.h"
#include "functions/src/include/firebase/functions/callable_reference.h"
#include "functions/src/include/firebase/functions/common.h"

namespace firebase::functions::internal {

// clang-format off
#define FUNCTIONS_METHODS(X)                                                   \
  X(GetInstance, "getInstance",                                                \
    "(Lcom/google/firebase/FirebaseApp;Ljava/lang/String;)"                    \
    "Lcom/google/firebase/functions/FirebaseFunctions;",                       \
    firebase::jni::kMethodTypeStatic)                                          \
  X(GetHttpsCallable, "getHttpsCallable",                                      \
    "(Ljava/lang/String;)Lcom/google/firebase/functions/HttpsCallableReference;") \
  X(GetHttpsCallableWithOptions, "getHttpsCallable",                           \
    "(Ljava/lang/String;Lcom/google/firebase/functions/HttpsCallableOptions;)" \
    "Lcom/google/firebase/functions/HttpsCallableReference;")                  \
  X(GetHttpsCallableFromUrl, "getHttpsCallableFromUrl",                        \
    "(Ljava/net/URL;)Lcom/google/firebase/functions/HttpsCallableReference;")  \
  X(GetHttpsCallableFromUrlWithOptions, "getHttpsCallableFromUrl",             \
    "(Ljava/net/URL;Lcom/google/firebase/functions/HttpsCallableOptions;)"     \
    "Lcom/google/firebase/functions/HttpsCallableReference;")                  \
  X(UseFunctionsEmulator, "useFunctionsEmulator", "(Ljava/lang/String;)V",     \
    jni::kMethodTypeInstance, jni::kMethodOptional)                            \
  X(UseEmulator, "useEmulator", "(Ljava/lang/String;I)V",                      \
    jni::kMethodTypeInstance, jni::kMethodOptional)

#define FUNCTIONS_EXCEPTION_METHODS(X)                                         \
  X(GetCode, "getCode",                                                        \
    "()Lcom/google/firebase/functions/FirebaseFunctionsException$Code;")       \
  X(GetDetails, "getDetails", "()Ljava/lang/Object;")

#define CODE_METHODS(X) X(Name, "name", "()Ljava/lang/String;")
#define CODE_FIELD_SIG \
  "Lcom/google/firebase/functions/FirebaseFunctionsException$Code;"
#define CODE_FIELDS(X)                                                         \
  X(Ok, "OK", CODE_FIELD_SIG, jni::kFieldTypeStatic)                           \
  X(Cancelled, "CANCELLED", CODE_FIELD_SIG, jni::kFieldTypeStatic)             \
  X(Unknown, "UNKNOWN", CODE_FIELD_SIG, jni::kFieldTypeStatic)                 \
  X(InvalidArgument, "INVALID_ARGUMENT", CODE_FIELD_SIG, jni::kFieldTypeStatic)\
  X(DeadlineExceeded, "DEADLINE_EXCEEDED", CODE_FIELD_SIG, jni::kFieldTypeStatic)\
  X(NotFound, "NOT_FOUND", CODE_FIELD_SIG, jni::kFieldTypeStatic)              \
  X(AlreadyExists, "ALREADY_EXISTS", CODE_FIELD_SIG, jni::kFieldTypeStatic)    \
  X(PermissionDenied, "PERMISSION_DENIED", CODE_FIELD_SIG, jni::kFieldTypeStatic)\
  X(Unauthenticated, "UNAUTHENTICATED", CODE_FIELD_SIG, jni::kFieldTypeStatic) \
  X(ResourceExhausted, "RESOURCE_EXHAUSTED", CODE_FIELD_SIG, jni::kFieldTypeStatic)\
  X(FailedPrecondition, "FAILED_PRECONDITION", CODE_FIELD_SIG, jni::kFieldTypeStatic)\
  X(Aborted, "ABORTED", CODE_FIELD_SIG, jni::kFieldTypeStatic)                 \
  X(OutOfRange, "OUT_OF_RANGE", CODE_FIELD_SIG, jni::kFieldTypeStatic)         \
  X(Unimplemented, "UNIMPLEMENTED", CODE_FIELD_SIG, jni::kFieldTypeStatic)     \
  X(Internal, "INTERNAL", CODE_FIELD_SIG, jni::kFieldTypeStatic)               \
  X(Unavailable, "UNAVAILABLE", CODE_FIELD_SIG, jni::kFieldTypeStatic)         \
  X(DataLoss, "DATA_LOSS", CODE_FIELD_SIG, jni::kFieldTypeStatic)

#define CALLABLE_OPTIONS_METHODS(X)                                            \
  X(GetLimitedUseAppCheckTokens, "getLimitedUseAppCheckTokens", "()Z")

#define CALLABLE_OPTIONS_BUILDER_METHODS(X)                                    \
  X(Constructor, "<init>", "()V")                                              \
  X(Build, "build", "()Lcom/google/firebase/functions/HttpsCallableOptions;")  \
  X(SetLimitedUseAppCheckTokens, "setLimitedUseAppCheckTokens",                \
    "(Z)Lcom/google/firebase/functions/HttpsCallableOptions$Builder;")

#define TIME_UNIT_FIELDS(X)                                                    \
  X(Milliseconds, "MILLISECONDS", "Ljava/util/concurrent/TimeUnit;",           \
    jni::kFieldTypeStatic, jni::kMethodOptional)
// clang-format on

JNI_METHOD_LOOKUP_DECLARATION(
    functions, FUNCTIONS_METHODS, FIELD_LOOKUP_NONE,
    "com/google/firebase/functions/FirebaseFunctions")
JNI_METHOD_LOOKUP_DECLARATION(
    functions_exception, FUNCTIONS_EXCEPTION_METHODS, FIELD_LOOKUP_NONE,
    "com/google/firebase/functions/FirebaseFunctionsException")
JNI_METHOD_LOOKUP_DECLARATION(
    code, CODE_METHODS, CODE_FIELDS,
    "com/google/firebase/functions/FirebaseFunctionsException$Code")
JNI_METHOD_LOOKUP_DECLARATION(
    options, CALLABLE_OPTIONS_METHODS, FIELD_LOOKUP_NONE,
    "com/google/firebase/functions/HttpsCallableOptions")
JNI_METHOD_LOOKUP_DECLARATION(
    options_builder, CALLABLE_OPTIONS_BUILDER_METHODS, FIELD_LOOKUP_NONE,
    "com/google/firebase/functions/HttpsCallableOptions$Builder")
JNI_METHOD_LOOKUP_DECLARATION(time_unit, METHOD_LOOKUP_NONE, TIME_UNIT_FIELDS,
                              "java/util/concurrent/TimeUnit")

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
  if (jni::CheckAndClearException(env) || !builder) return nullptr;
  jobject next = env->CallObjectMethod(
      builder,
      options_builder::GetMethodId(
          options_builder::kSetLimitedUseAppCheckTokens),
      opts.limited_use_app_check_token ? JNI_TRUE : JNI_FALSE);
  if (jni::CheckAndClearException(env) || !next) {
    env->DeleteLocalRef(builder);
    return nullptr;
  }
  builder = util::ContinueBuilder(env, builder, next);
  jobject java_options = env->CallObjectMethod(
      builder, options_builder::GetMethodId(options_builder::kBuild));
  env->DeleteLocalRef(builder);
  return jni::CheckAndClearException(env) ? nullptr : java_options;
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
  if (!set_timeout || !ms_field || !time_unit::GetClass()) return;
  jobject ms_unit = env->GetStaticObjectField(time_unit::GetClass(), ms_field);
  if (jni::CheckAndClearException(env) || !ms_unit) return;
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

    code_map_ = new std::map<jobject, Error>();
    static constexpr std::pair<code::Field, Error> kCodePairs[] = {
        {code::kOk, kErrorNone},
        {code::kCancelled, kErrorCancelled},
        {code::kUnknown, kErrorUnknown},
        {code::kInvalidArgument, kErrorInvalidArgument},
        {code::kDeadlineExceeded, kErrorDeadlineExceeded},
        {code::kNotFound, kErrorNotFound},
        {code::kAlreadyExists, kErrorAlreadyExists},
        {code::kPermissionDenied, kErrorPermissionDenied},
        {code::kUnauthenticated, kErrorUnauthenticated},
        {code::kResourceExhausted, kErrorResourceExhausted},
        {code::kFailedPrecondition, kErrorFailedPrecondition},
        {code::kAborted, kErrorAborted},
        {code::kOutOfRange, kErrorOutOfRange},
        {code::kUnimplemented, kErrorUnimplemented},
        {code::kInternal, kErrorInternal},
        {code::kUnavailable, kErrorUnavailable},
        {code::kDataLoss, kErrorDataLoss},
    };
    for (const auto& [field, err] : kCodePairs) {
      jobject obj = env->GetStaticObjectField(code::GetClass(),
                                              code::GetFieldId(field));
      if (!jni::CheckAndClearException(env) && obj) {
        (*code_map_)[env->NewGlobalRef(obj)] = err;
        env->DeleteLocalRef(obj);
      }
    }
    jni::CheckAndClearException(env);
  }
  initialize_count_++;
  return true;
}

void FunctionsInternal::Terminate(App* app) {
  MutexLock init_lock(init_mutex_);
  FIREBASE_ASSERT(initialize_count_ > 0);
  if (--initialize_count_ == 0) {
    JNIEnv* env = app->GetJNIEnv();
    ReleaseClasses(env);
    if (code_map_) {
      for (const auto& [code_obj, _] : *code_map_) {
        env->DeleteGlobalRef(code_obj);
      }
      delete std::exchange(code_map_, nullptr);
    }
    jni::CheckAndClearException(env);
  }
}

FunctionsInternal::FunctionsInternal(App* app, const char* region)
    : region_(region ? region : "us-central1"),
      lifetime_state_(std::make_shared<FunctionsLifetimeState>()),
      jni_task_id_("Functions_" +
                   std::to_string(reinterpret_cast<uintptr_t>(this))) {
  if (!app || !Initialize(app)) return;
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
  if (!app_) return;
  JNIEnv* env = app_->GetJNIEnv();
  firebase::util::CancelCallbacks(env, jni_task_id_.c_str());
  if (obj_) env->DeleteGlobalRef(std::exchange(obj_, nullptr));
  jni::CheckAndClearException(env);
  Terminate(std::exchange(app_, nullptr));
}

HttpsCallableReferenceInternal* FunctionsInternal::WrapCallable(
    JNIEnv* env, jobject ref_obj, const HttpsCallableOptions* opts) const {
  if (jni::CheckAndClearException(env) || !ref_obj) return nullptr;
  if (opts) ApplyJavaTimeout(env, ref_obj, *opts);
  auto* result = new HttpsCallableReferenceInternal(
      const_cast<FunctionsInternal*>(this), ref_obj);
  env->DeleteLocalRef(ref_obj);
  return result;
}

HttpsCallableReferenceInternal* FunctionsInternal::GetHttpsCallable(
    const char* name) const {
  FIREBASE_ASSERT_RETURN(nullptr, is_valid());
  if (!name) return nullptr;
  JNIEnv* env = app_->GetJNIEnv();
  jstring jname = env->NewStringUTF(name);
  if (jni::CheckAndClearException(env) || !jname) return nullptr;
  jobject ref_obj = env->CallObjectMethod(
      obj_, functions::GetMethodId(functions::kGetHttpsCallable), jname);
  env->DeleteLocalRef(jname);
  return WrapCallable(env, ref_obj);
}

HttpsCallableReferenceInternal* FunctionsInternal::GetHttpsCallableWithOptions(
    const char* name, const HttpsCallableOptions& opts) const {
  FIREBASE_ASSERT_RETURN(nullptr, is_valid());
  if (!name) return nullptr;
  JNIEnv* env = app_->GetJNIEnv();
  jobject java_options = BuildJavaOptions(env, opts);
  if (!java_options) return nullptr;
  jstring jname = env->NewStringUTF(name);
  if (jni::CheckAndClearException(env) || !jname) {
    env->DeleteLocalRef(java_options);
    return nullptr;
  }
  jobject ref_obj = env->CallObjectMethod(
      obj_, functions::GetMethodId(functions::kGetHttpsCallableWithOptions),
      jname, java_options);
  env->DeleteLocalRef(jname);
  env->DeleteLocalRef(java_options);
  return WrapCallable(env, ref_obj, &opts);
}

HttpsCallableReferenceInternal* FunctionsInternal::GetHttpsCallableFromURL(
    const char* url) const {
  FIREBASE_ASSERT_RETURN(nullptr, is_valid());
  if (!url) return nullptr;
  JNIEnv* env = app_->GetJNIEnv();
  jobject url_obj = util::CharsToURL(env, url);
  if (!url_obj) return nullptr;
  jobject ref_obj = env->CallObjectMethod(
      obj_, functions::GetMethodId(functions::kGetHttpsCallableFromUrl),
      url_obj);
  env->DeleteLocalRef(url_obj);
  return WrapCallable(env, ref_obj);
}

HttpsCallableReferenceInternal*
FunctionsInternal::GetHttpsCallableFromURLWithOptions(
    const char* url, const HttpsCallableOptions& opts) const {
  FIREBASE_ASSERT_RETURN(nullptr, is_valid());
  if (!url) return nullptr;
  JNIEnv* env = app_->GetJNIEnv();
  jobject url_obj = util::CharsToURL(env, url);
  if (!url_obj) return nullptr;
  jobject java_options = BuildJavaOptions(env, opts);
  if (!java_options) {
    env->DeleteLocalRef(url_obj);
    return nullptr;
  }
  jobject ref_obj = env->CallObjectMethod(
      obj_,
      functions::GetMethodId(functions::kGetHttpsCallableFromUrlWithOptions),
      url_obj, java_options);
  env->DeleteLocalRef(url_obj);
  env->DeleteLocalRef(java_options);
  return WrapCallable(env, ref_obj, &opts);
}

void FunctionsInternal::UseFunctionsEmulator(const char* origin) {
  FIREBASE_ASSERT_RETURN_VOID(is_valid());
  if (!origin || origin[0] == '\0') return;
  JNIEnv* env = app_->GetJNIEnv();
  if (jmethodID use_emulator_id =
          functions::GetMethodId(functions::kUseEmulator)) {
    std::string_view sv(origin);
    if (sv.substr(0, 7) == "http://") sv.remove_prefix(7);
    else if (sv.substr(0, 8) == "https://") sv.remove_prefix(8);
    while (!sv.empty() && sv.back() == '/') sv.remove_suffix(1);
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
  if (jmethodID legacy_id =
          functions::GetMethodId(functions::kUseFunctionsEmulator)) {
    jstring jorigin = env->NewStringUTF(origin);
    env->CallVoidMethod(obj_, legacy_id, jorigin);
    env->DeleteLocalRef(jorigin);
    jni::CheckAndClearException(env);
  }
}

Error FunctionsInternal::ErrorFromJavaFunctionsException(
    JNIEnv* env, jobject java_exception, std::string* error_message,
    Variant* details_out) {
  if (!java_exception) return kErrorNone;
  Error error = kErrorUnknown;
  if (error_message) {
    *error_message = ::firebase::jni::ExtractExceptionMessage(
        env, static_cast<jthrowable>(java_exception));
    jni::CheckAndClearException(env);
  }
  if (env->IsInstanceOf(java_exception, functions_exception::GetClass())) {
    error = kErrorInternal;
    jobject code = env->CallObjectMethod(
        java_exception,
        functions_exception::GetMethodId(functions_exception::kGetCode));
    if (!jni::CheckAndClearException(env) && code) {
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
    }
    if (details_out) {
      jobject jdetails = env->CallObjectMethod(
          java_exception,
          functions_exception::GetMethodId(functions_exception::kGetDetails));
      if (!jni::CheckAndClearException(env) && jdetails) {
        *details_out = firebase::util::JavaObjectToVariant(env, jdetails);
        jni::CheckAndClearException(env);
        env->DeleteLocalRef(jdetails);
      }
    }
  }
  return error == kErrorNone ? kErrorInternal : error;
}

}  // namespace firebase::functions::internal
