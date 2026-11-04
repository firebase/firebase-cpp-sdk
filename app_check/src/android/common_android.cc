#include "app/src/jni/declaration.h"
// Copyright 2023 Google LLC
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

#include "app/src/app_android.h"
#include "app/src/jni/exception.h"
#include "app_check/src/android/common_android.h"

#include <cstdint>
#include <string>

#include "app/src/app_common.h"
#include "app/src/util.h"
#include "app_check/src/include/firebase/app_check.h"

namespace firebase {
namespace app_check {
namespace internal {

namespace app_check_provider {
  ::firebase::jni::Global<jclass> g_class;
  jmethodID g_method_ids[kMethodCount];
  jfieldID g_field_ids[kFieldCount];
  jclass GetClass() { return g_class.get(); }
  jmethodID GetMethodId(Method method) { return g_method_ids[method]; }
  jfieldID GetFieldId(Field field) { return g_field_ids[field]; }
  bool CacheMethodIds(JNIEnv* env, jobject activity) {
    if (g_class) return true;
    ::firebase::jni::Local<::firebase::jni::Class> local_clazz(
        env, ::firebase::util::FindClass(
                 env, activity,
                 "com/google/firebase/appcheck/AppCheckProvider"));
    if (!local_clazz) {
      ::firebase::jni::CheckAndClearException(env);
      return false;
    }
    g_class = local_clazz;
    g_method_ids[kGetToken] = env->GetMethodID(g_class.get(), "getToken", "()Lcom/google/android/gms/tasks/Task;");
    if (!g_method_ids[kGetToken]) return false;
    g_method_ids[kGetLimitedUseToken] = env->GetMethodID(g_class.get(), "getLimitedUseToken", "()Lcom/google/android/gms/tasks/Task;");
    if (!g_method_ids[kGetLimitedUseToken]) return false;
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


// Used to setup the cache of AppCheckProvider interface method IDs to reduce
// time spent looking up methods by string.
// clang-format off
#define APP_CHECK_TOKEN_METHODS(X)                                                        \
  X(GetToken, "getToken",                                                          \
    "()Ljava/lang/String;") \
  X(GetExpireTimeMillis, "getExpireTimeMillis",                                                          \
    "()J")
// clang-format on

JNI_METHOD_LOOKUP_DECLARATION(app_check_token, APP_CHECK_TOKEN_METHODS, FIELD_LOOKUP_NONE, "com/google/firebase/appcheck/AppCheckToken")


bool CacheCommonAndroidMethodIds(JNIEnv* env, jobject activity) {
  // Cache the token and provider classes.
  if (!(app_check_token::CacheMethodIds(env, activity) &&
        app_check_provider::CacheMethodIds(env, activity))) {
    return false;
  }

  return true;
}

void ReleaseCommonAndroidClasses(JNIEnv* env) {
  app_check_token::ReleaseClass(env);
  app_check_provider::ReleaseClass(env);
}

// Converts an android AppCheckToken to a C++ AppCheckToken
AppCheckToken CppTokenFromAndroidToken(JNIEnv* env, jobject token_obj) {
  AppCheckToken cpp_token;

  if (env != nullptr && token_obj != nullptr) {
    jobject token_str = env->CallObjectMethod(
        token_obj, app_check_token::GetMethodId(app_check_token::kGetToken));
    if (!jni::CheckAndClearException(env) && token_str != nullptr) {
      cpp_token.token = ::firebase::jni::JStringToString(env, token_str);
    } else if (token_str != nullptr) {
      env->DeleteLocalRef(token_str);
    }

    jlong millis = env->CallLongMethod(
        token_obj,
        app_check_token::GetMethodId(app_check_token::kGetExpireTimeMillis));
    if (!jni::CheckAndClearException(env)) {
      cpp_token.expire_time_millis = static_cast<int64_t>(millis);
    }
  }

  return cpp_token;
}

JNIEnv* GetJniEnv() { return util::GetJNIEnvFromApp(); }

namespace {

// An object wrapping a token-completion callback method
struct TokenResultCallbackData {
  explicit TokenResultCallbackData(
      std::function<void(AppCheckToken, int, const std::string&)> callback_)
      : callback(std::move(callback_)) {}
  std::function<void(AppCheckToken, int, const std::string&)> callback;
};

void TokenResultCallback(JNIEnv* env, jobject result,
                         firebase::util::FutureResult result_code,
                         const char* status_message, void* callback_data) {
  int result_error_code = kAppCheckErrorNone;
  AppCheckToken result_token;
  bool success = (result_code == util::kFutureResultSuccess);
  if (success && result) {
    result_token = CppTokenFromAndroidToken(env, result);
  } else {
    // Using error code unknown for failure because Android AppCheck doesnt have
    // an error code enum
    result_error_code = kAppCheckErrorUnknown;
  }
  // Evaluate the provided callback method.
  auto* completion_callback_data =
      reinterpret_cast<TokenResultCallbackData*>(callback_data);
  if (completion_callback_data != nullptr) {
    if (completion_callback_data->callback) {
      completion_callback_data->callback(
          result_token, result_error_code,
          status_message ? status_message : "");
    }
    delete completion_callback_data;
  }
}
}  // anonymous namespace

AndroidAppCheckProvider::AndroidAppCheckProvider(jobject local_provider)
    : android_provider_(nullptr) {
  static const char* kApiIdentifier = "AppCheckProvider";
  jni_task_id_ = CreateApiIdentifier(kApiIdentifier, this);
  JNIEnv* env = GetJniEnv();
  if (env != nullptr && local_provider != nullptr) {
    android_provider_ = env->NewGlobalRef(local_provider);
  }
}

AndroidAppCheckProvider::~AndroidAppCheckProvider() {
  JNIEnv* env = GetJniEnv();
  if (env != nullptr) {
    firebase::util::CancelCallbacks(env, jni_task_id_.c_str());
    if (android_provider_ != nullptr) {
      env->DeleteGlobalRef(android_provider_);
      android_provider_ = nullptr;
    }
  }
}

void AndroidAppCheckProvider::GetToken(
    std::function<void(AppCheckToken, int, const std::string&)>
        completion_callback) {
  JNIEnv* env = GetJniEnv();
  if (env == nullptr || android_provider_ == nullptr) {
    if (completion_callback) {
      completion_callback(AppCheckToken{}, kAppCheckErrorInvalidConfiguration,
                          "AndroidAppCheckProvider is not initialized.");
    }
    return;
  }

  jobject j_task = env->CallObjectMethod(
      android_provider_,
      app_check_provider::GetMethodId(app_check_provider::kGetToken));
  std::string error = jni::GetAndClearExceptionMessage(env);
  if (error.empty() && j_task != nullptr) {
    // Create an object to wrap the callback function
    TokenResultCallbackData* completion_callback_data =
        new TokenResultCallbackData(std::move(completion_callback));
    firebase::util::RegisterCallbackOnTask(
        env, j_task, TokenResultCallback,
        reinterpret_cast<void*>(completion_callback_data),
        jni_task_id_.c_str());
  } else {
    AppCheckToken empty_token;
    if (completion_callback) {
      completion_callback(empty_token, kAppCheckErrorUnknown, error.c_str());
    }
  }
  if (j_task != nullptr) {
    env->DeleteLocalRef(j_task);
  }
}

void AndroidAppCheckProvider::GetLimitedUseToken(
    std::function<void(AppCheckToken, int, const std::string&)>
        completion_callback) {
  JNIEnv* env = GetJniEnv();
  if (env == nullptr || android_provider_ == nullptr) {
    if (completion_callback) {
      completion_callback(AppCheckToken{}, kAppCheckErrorInvalidConfiguration,
                          "AndroidAppCheckProvider is not initialized.");
    }
    return;
  }

  jobject j_task = env->CallObjectMethod(
      android_provider_,
      app_check_provider::GetMethodId(app_check_provider::kGetLimitedUseToken));
  std::string error = jni::GetAndClearExceptionMessage(env);
  if (error.empty() && j_task != nullptr) {
    // Create an object to wrap the callback function
    TokenResultCallbackData* completion_callback_data =
        new TokenResultCallbackData(std::move(completion_callback));
    firebase::util::RegisterCallbackOnTask(
        env, j_task, TokenResultCallback,
        reinterpret_cast<void*>(completion_callback_data),
        jni_task_id_.c_str());
  } else {
    AppCheckToken empty_token;
    if (completion_callback) {
      completion_callback(empty_token, kAppCheckErrorUnknown, error.c_str());
    }
  }
  if (j_task != nullptr) {
    env->DeleteLocalRef(j_task);
  }
}

}  // namespace internal
}  // namespace app_check
}  // namespace firebase
