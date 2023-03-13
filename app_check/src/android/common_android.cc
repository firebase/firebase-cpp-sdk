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

#include <string>

#include "app/src/app_common.h"
#include "app/src/util_android.h"
#include "app_check/src/include/firebase/app_check.h"

namespace firebase {
namespace app_check {
namespace internal {

METHOD_LOOKUP_DEFINITION(app_check_provider,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/appcheck/AppCheckProvider",
                         APP_CHECK_PROVIDER_METHODS)

// Used to setup the cache of AppCheckProvider interface method IDs to reduce
// time spent looking up methods by string.
// clang-format off
#define APP_CHECK_TOKEN_METHODS(X)                                                        \
  X(GetToken, "getToken",                                                          \
    "()Ljava/lang/String;"),                                                        \
  X(GetExpireTimeMillis, "getExpireTimeMillis",                                                          \
    "()J")
// clang-format on

METHOD_LOOKUP_DECLARATION(app_check_token, APP_CHECK_TOKEN_METHODS)
METHOD_LOOKUP_DEFINITION(app_check_token,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/appcheck/AppCheckToken",
                         APP_CHECK_TOKEN_METHODS)

static const char* kApiIdentifier = "AppCheckProvider";

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

  if (token_obj != nullptr) {
    jobject token_str = env->CallObjectMethod(
        token_obj, app_check_token::GetMethodId(app_check_token::kGetToken));
    util::CheckAndClearJniExceptions(env);
    cpp_token.token = util::JStringToString(env, token_str);

    jlong millis = env->CallLongMethod(
        token_obj,
        app_check_token::GetMethodId(app_check_token::kGetExpireTimeMillis));
    util::CheckAndClearJniExceptions(env);
    cpp_token.expire_time_millis = static_cast<int64_t>(millis);
  }

  return cpp_token;
}

JNIEnv* GetJniEnv() {
  // The JNI environment is the same regardless of App.
  App* app = app_common::GetAnyApp();
  FIREBASE_ASSERT(app != nullptr);
  return app->GetJNIEnv();
}

namespace {

// An object wrapping a token-completion callback method
struct TokenResultCallbackData {
  TokenResultCallbackData(
      std::function<void(AppCheckToken, int, const std::string&)> callback_)
      : callback(callback_) {}
  std::function<void(AppCheckToken, int, const std::string&)> callback;
};

void TokenResultCallback(JNIEnv* env, jobject result,
                         util::FutureResult result_code,
                         const char* status_message, void* callback_data) {
  int result_error_code = kAppCheckErrorNone;
  AppCheckToken result_token;
  bool success = (result_code == util::kFutureResultSuccess);
  std::string result_value = "";
  if (success && result) {
    result_token = CppTokenFromAndroidToken(env, result);
  } else {
    // Using error code unknown for failure because Android AppCheck doesnt have
    // an error code enum
    result_error_code = kAppCheckErrorUnknown;
  }
  // Evalute the provided callback method.
  auto* completion_callback_data =
      reinterpret_cast<TokenResultCallbackData*>(callback_data);
  completion_callback_data->callback(result_token, result_error_code,
                                     status_message);
  delete completion_callback_data;
}
}  // anonymous namespace

AndroidAppCheckProvider::AndroidAppCheckProvider(jobject local_provider)
    : android_provider_(nullptr) {
  JNIEnv* env = GetJniEnv();
  android_provider_ = env->NewGlobalRef(local_provider);
}

AndroidAppCheckProvider::~AndroidAppCheckProvider() {
  JNIEnv* env = GetJniEnv();
  if (env != nullptr && android_provider_ != nullptr) {
    env->DeleteGlobalRef(android_provider_);
  }
}

void AndroidAppCheckProvider::GetToken(
    std::function<void(AppCheckToken, int, const std::string&)>
        completion_callback) {
  JNIEnv* env = GetJniEnv();

  jobject j_task = env->CallObjectMethod(
      android_provider_,
      app_check_provider::GetMethodId(app_check_provider::kGetToken));
  std::string error = util::GetAndClearExceptionMessage(env);
  if (error.empty()) {
    // Create an object to wrap the callback function
    TokenResultCallbackData* completion_callback_data =
        new TokenResultCallbackData(completion_callback);
    util::RegisterCallbackOnTask(
        env, j_task, TokenResultCallback,
        reinterpret_cast<void*>(completion_callback_data), kApiIdentifier);
  } else {
    AppCheckToken empty_token;
    completion_callback(empty_token, kAppCheckErrorUnknown, error.c_str());
  }
  env->DeleteLocalRef(j_task);
}

}  // namespace internal
}  // namespace app_check
}  // namespace firebase
