// Copyright 2022 Google LLC
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

#ifndef FIREBASE_APP_CHECK_SRC_ANDROID_DEBUG_PROVIDER_ANDROID_H_
#define FIREBASE_APP_CHECK_SRC_ANDROID_DEBUG_PROVIDER_ANDROID_H_

#include <map>
#include <string>
#include <vector>

#include "app/src/embedded_file.h"
#include "firebase/app_check.h"

namespace firebase {
namespace app_check {
namespace internal {

// Cache the method ids so we don't have to look up JNI functions by name.
bool CacheDebugProviderMethodIds(
    JNIEnv* env, jobject activity,
    const std::vector<firebase::internal::EmbeddedFile>& embedded_files);

// Release provider classes cached by CacheDebugProviderMethodIds().
void ReleaseDebugProviderClasses(JNIEnv* env);

class DebugAppCheckProviderFactoryInternal : public AppCheckProviderFactory {
 public:
  DebugAppCheckProviderFactoryInternal();

  virtual ~DebugAppCheckProviderFactoryInternal();

  AppCheckProvider* CreateProvider(App* app) override;

  void SetDebugToken(const std::string& token);

 private:
  jobject android_provider_factory_;

  std::map<App*, AppCheckProvider*> created_providers_;

  std::string debug_token_;
};

}  // namespace internal
}  // namespace app_check
}  // namespace firebase

#endif  // FIREBASE_APP_CHECK_SRC_ANDROID_DEBUG_PROVIDER_ANDROID_H_
