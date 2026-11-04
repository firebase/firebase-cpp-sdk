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

#include <functional>
#include <string>

#include "app/src/app_common.h"
#include "app/src/jni/declaration.h"
#include "app_check/src/include/firebase/app_check.h"
#include "firebase/app_check.h"

namespace firebase::app_check::internal {

// clang-format off
#define APP_CHECK_PROVIDER_METHODS(X)                                 \
  X(GetToken, "getToken",                                             \
    "()Lcom/google/android/gms/tasks/Task;")                          \
  X(GetLimitedUseToken, "getLimitedUseToken",                         \
    "()Lcom/google/android/gms/tasks/Task;")
// clang-format on

JNI_METHOD_LOOKUP_DECLARATION(app_check_provider, APP_CHECK_PROVIDER_METHODS,
                              FIELD_LOOKUP_NONE,
                              "com/google/firebase/appcheck/AppCheckProvider")

bool CacheCommonAndroidMethodIds(JNIEnv* env, jobject activity);
void ReleaseCommonAndroidClasses(JNIEnv* env);
JNIEnv* GetJniEnv();
AppCheckToken CppTokenFromAndroidToken(JNIEnv* env, jobject token_obj);

class AndroidAppCheckProvider : public AppCheckProvider {
 public:
  explicit AndroidAppCheckProvider(jobject debug_provider);
  ~AndroidAppCheckProvider() override;

  void GetToken(std::function<void(AppCheckToken, int, const std::string&)>
                    completion_callback) override;
  void GetLimitedUseToken(
      std::function<void(AppCheckToken, int, const std::string&)>
          completion_callback) override;

 private:
  void FetchToken(app_check_provider::Method method,
                  std::function<void(AppCheckToken, int, const std::string&)>
                      completion_callback);

  jobject android_provider_;
  std::string jni_task_id_;
};

}  // namespace firebase::app_check::internal

#endif  // FIREBASE_APP_CHECK_SRC_ANDROID_COMMON_ANDROID_H_
