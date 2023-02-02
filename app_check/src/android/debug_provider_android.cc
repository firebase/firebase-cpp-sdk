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

#include "app_check/src/android/debug_provider_android.h"

#include "app/src/app_common.h"
#include "app/src/util_android.h"
#include "app_check/src/android/common_android.h"
#include "firebase/app_check/debug_provider.h"

namespace firebase {
namespace app_check {
namespace internal {

// Used to setup the cache of DebugProviderFactory class method IDs to reduce
// time spent looking up methods by string.
// clang-format off
#define DEBUG_PROVIDER_FACTORY_METHODS(X)                                      \
  X(GetInstance, "getInstance",                                                \
    "()Lcom/google/firebase/appcheck/debug/DebugAppCheckProviderFactory;",     \
       util::kMethodTypeStatic),                                               \
  X(Create, "create",                                                          \
    "(Lcom/google/firebase/FirebaseApp;)"                                      \
    "Lcom/google/firebase/appcheck/AppCheckProvider;")
// clang-format on

METHOD_LOOKUP_DECLARATION(debug_provider_factory,
                          DEBUG_PROVIDER_FACTORY_METHODS)
METHOD_LOOKUP_DEFINITION(
    debug_provider_factory,
    PROGUARD_KEEP_CLASS
    "com/google/firebase/appcheck/debug/DebugAppCheckProviderFactory",
    DEBUG_PROVIDER_FACTORY_METHODS)

static bool g_methods_cached = false;

const char kMethodsNotCachedError[] =
    "DebugAppCheckProviderFactory methods were not cached.";

bool CacheDebugProviderMethodIds(JNIEnv* env, jobject activity) {
  // Cache the DebugProvider classes.
  g_methods_cached = debug_provider_factory::CacheMethodIds(env, activity);
  return g_methods_cached;
}

void ReleaseDebugProviderClasses(JNIEnv* env) {
  debug_provider_factory::ReleaseClass(env);
  g_methods_cached = false;
}

DebugAppCheckProviderFactoryInternal::DebugAppCheckProviderFactoryInternal()
    : created_providers_(), android_provider_factory_(nullptr) {}

DebugAppCheckProviderFactoryInternal::~DebugAppCheckProviderFactoryInternal() {
  for (auto it = created_providers_.begin(); it != created_providers_.end();
       ++it) {
    delete it->second;
  }
  created_providers_.clear();
  JNIEnv* env = GetJniEnv();
  if (env != nullptr && android_provider_factory_ != nullptr) {
    env->DeleteGlobalRef(android_provider_factory_);
  }
}

AppCheckProvider* DebugAppCheckProviderFactoryInternal::CreateProvider(
    App* app) {
  FIREBASE_ASSERT_MESSAGE_RETURN(nullptr, g_methods_cached,
                                 kMethodsNotCachedError);

  // Return the provider if it already exists.
  std::map<App*, AppCheckProvider*>::iterator it = created_providers_.find(app);
  if (it != created_providers_.end()) {
    return it->second;
  }
  JNIEnv* env = app->GetJNIEnv();
  // Create a provider factory first if needed.
  if (android_provider_factory_ == nullptr) {
    jobject j_provider_factory_local =
        env->CallStaticObjectMethod(debug_provider_factory::GetClass(),
                                    debug_provider_factory::GetMethodId(
                                        debug_provider_factory::kGetInstance));
    FIREBASE_ASSERT(!util::CheckAndClearJniExceptions(env));
    // Hold a global references.
    // The global references will be freed when this factory is destroyed
    android_provider_factory_ = env->NewGlobalRef(j_provider_factory_local);
    env->DeleteLocalRef(j_provider_factory_local);
  }
  jobject platform_app = app->GetPlatformApp();
  jobject j_android_provider_local = env->CallObjectMethod(
      android_provider_factory_,
      debug_provider_factory::GetMethodId(debug_provider_factory::kCreate),
      platform_app);
  FIREBASE_ASSERT(!util::CheckAndClearJniExceptions(env));
  env->DeleteLocalRef(platform_app);

  // Pass a global ref into the cpp provider constructor.
  // It will be released when the cpp provider is destroyed
  AppCheckProvider* cpp_provider =
      new internal::AndroidAppCheckProvider(j_android_provider_local);
  env->DeleteLocalRef(j_android_provider_local);
  created_providers_[app] = cpp_provider;
  return cpp_provider;
}

}  // namespace internal
}  // namespace app_check
}  // namespace firebase
