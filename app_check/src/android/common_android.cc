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

#include "app_check/src/android/common_android.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "app/src/app_android.h"
#include "app/src/app_common.h"
#include "app/src/jni/declaration.h"
#include "app/src/jni/exception.h"
#include "app/src/util.h"
#include "app_check/src/include/firebase/app_check.h"

namespace firebase::app_check::internal {

// clang-format off
#define APP_CHECK_TOKEN_METHODS(X)                                             \
  X(GetToken, "getToken", "()Ljava/lang/String;")                              \
  X(GetExpireTimeMillis, "getExpireTimeMillis", "()J")
// clang-format on

JNI_METHOD_LOOKUP_DECLARATION(app_check_token, APP_CHECK_TOKEN_METHODS,
                              FIELD_LOOKUP_NONE,
                              "com/google/firebase/appcheck/AppCheckToken")

bool CacheCommonAndroidMethodIds(JNIEnv* env, jobject activity) {
  return app_check_token::CacheMethodIds(env, activity) &&
         app_check_provider::CacheMethodIds(env, activity);
}

void ReleaseCommonAndroidClasses(JNIEnv* env) {
  app_check_token::ReleaseClass(env);
  app_check_provider::ReleaseClass(env);
}

AppCheckToken CppTokenFromAndroidToken(JNIEnv* env, jobject token_obj) {
  AppCheckToken cpp_token;
  if (!env || !token_obj) return cpp_token;

  jobject token_str = env->CallObjectMethod(
      token_obj, app_check_token::GetMethodId(app_check_token::kGetToken));
  if (!jni::CheckAndClearException(env) && token_str) {
    cpp_token.token = ::firebase::jni::JStringToString(env, token_str);
  } else if (token_str) {
    env->DeleteLocalRef(token_str);
  }

  jlong millis = env->CallLongMethod(
      token_obj,
      app_check_token::GetMethodId(app_check_token::kGetExpireTimeMillis));
  if (!jni::CheckAndClearException(env)) {
    cpp_token.expire_time_millis = static_cast<int64_t>(millis);
  }
  return cpp_token;
}

JNIEnv* GetJniEnv() { return util::GetJNIEnvFromApp(); }

namespace {
using TokenCb = std::function<void(AppCheckToken, int, const std::string&)>;

void TokenResultCallback(JNIEnv* env, jobject result,
                         firebase::util::FutureResult result_code,
                         const char* status_message, void* callback_data) {
  std::unique_ptr<TokenCb> cb(static_cast<TokenCb*>(callback_data));
  if (!cb || !*cb) return;
  bool ok = (result_code == util::kFutureResultSuccess && result != nullptr);
  (*cb)(ok ? CppTokenFromAndroidToken(env, result) : AppCheckToken{},
        ok ? kAppCheckErrorNone : kAppCheckErrorUnknown,
        status_message ? status_message : "");
}
}  // namespace

AndroidAppCheckProvider::AndroidAppCheckProvider(jobject local_provider)
    : android_provider_(nullptr),
      jni_task_id_(CreateApiIdentifier("AppCheckProvider", this)) {
  if (JNIEnv* env = GetJniEnv(); env && local_provider) {
    android_provider_ = env->NewGlobalRef(local_provider);
  }
}

AndroidAppCheckProvider::~AndroidAppCheckProvider() {
  if (JNIEnv* env = GetJniEnv()) {
    firebase::util::CancelCallbacks(env, jni_task_id_.c_str());
    if (android_provider_) {
      env->DeleteGlobalRef(std::exchange(android_provider_, nullptr));
    }
  }
}

void AndroidAppCheckProvider::FetchToken(app_check_provider::Method method,
                                         TokenCb completion_callback) {
  JNIEnv* env = GetJniEnv();
  if (!env || !android_provider_) {
    if (completion_callback) {
      completion_callback(AppCheckToken{}, kAppCheckErrorInvalidConfiguration,
                          "AndroidAppCheckProvider is not initialized.");
    }
    return;
  }
  jobject j_task = env->CallObjectMethod(
      android_provider_, app_check_provider::GetMethodId(method));
  std::string error = jni::GetAndClearExceptionMessage(env);
  if (error.empty() && j_task) {
    firebase::util::RegisterCallbackOnTask(
        env, j_task, TokenResultCallback,
        new TokenCb(std::move(completion_callback)), jni_task_id_.c_str());
  } else if (completion_callback) {
    completion_callback(AppCheckToken{}, kAppCheckErrorUnknown, error.c_str());
  }
  if (j_task) env->DeleteLocalRef(j_task);
}

void AndroidAppCheckProvider::GetToken(TokenCb completion_callback) {
  FetchToken(app_check_provider::kGetToken, std::move(completion_callback));
}

void AndroidAppCheckProvider::GetLimitedUseToken(TokenCb completion_callback) {
  FetchToken(app_check_provider::kGetLimitedUseToken,
             std::move(completion_callback));
}

}  // namespace firebase::app_check::internal
