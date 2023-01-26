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

bool CacheCommonAndroidMethodIds(
    JNIEnv* env, jobject activity) {
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
    jobject token_str = env->CallObjectMethod(token_obj,
        app_check_token::GetMethodId(app_check_token::kGetToken));
    util::CheckAndClearJniExceptions(env);
    cpp_token.token = util::JStringToString(env, token_str);

    jlong millis = env->CallLongMethod(token_obj,
        app_check_token::GetMethodId(app_check_token::kGetExpireTimeMillis));
    util::CheckAndClearJniExceptions(env);
    cpp_token.expire_time_millis = static_cast<int64_t>(millis);
  }

  return cpp_token;
}

}  // namespace internal
}  // namespace app_check
}  // namespace firebase
