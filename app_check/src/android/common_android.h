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

#ifndef FIREBASE_APP_CHECK_SRC_ANDROID_COMMON_ANDROID_H_
#define FIREBASE_APP_CHECK_SRC_ANDROID_COMMON_ANDROID_H_

#include <jni.h>

#include <string>

#include "app/src/app_common.h"
#include "app/src/util_android.h"
#include "app_check/src/include/firebase/app_check.h"
#include "firebase/app_check.h"

namespace firebase {
namespace app_check {
namespace internal {

// Used to setup the cache of AppCheckProvider interface method IDs to reduce
// time spent looking up methods by string.
// clang-format off
#define APP_CHECK_PROVIDER_METHODS(X)                                                        \
  X(GetToken, "getToken",                                                          \
    "()Lcom/google/android/gms/tasks/Task;")
// clang-format on

METHOD_LOOKUP_DECLARATION(app_check_provider, APP_CHECK_PROVIDER_METHODS)

// Cache the method ids so we don't have to look up JNI functions by name.
bool CacheCommonAndroidMethodIds(JNIEnv* env, jobject activity);

// Release app check classes cached by CacheCommonAndroidMethodIds().
void ReleaseCommonAndroidClasses(JNIEnv* env);

JNIEnv* GetJniEnv();

AppCheckToken CppTokenFromAndroidToken(JNIEnv* env, jobject token_obj);

// A generic class to wrap a java AppCheckProvider.
class AndroidAppCheckProvider : public AppCheckProvider {
 public:
  explicit AndroidAppCheckProvider(jobject debug_provider);
  virtual ~AndroidAppCheckProvider();

  /// Fetches an AppCheckToken and then calls the provided callback method with
  /// the token or with an error code and error message.
  void GetToken(std::function<void(AppCheckToken, int, const std::string&)>
                    completion_callback) override;

 private:
  jobject android_provider_;
};

}  // namespace internal
}  // namespace app_check
}  // namespace firebase

#endif  // FIREBASE_APP_CHECK_SRC_ANDROID_COMMON_ANDROID_H_
