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

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "app/src/app_common.h"
#include "app/src/assert.h"
#include "app/src/cleanup_notifier.h"
#include "app/src/embedded_file.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/jni/declaration.h"
#include "app/src/jni/exception.h"
#include "app_check/src/android/common_android.h"
#include "firebase/app_check/debug_provider.h"

namespace firebase::app_check::internal {

// clang-format off
#define DEBUG_PROVIDER_FACTORY_METHODS(X)                                      \
  X(GetInstance, "getInstance",                                                \
    "()Lcom/google/firebase/appcheck/debug/DebugAppCheckProviderFactory;",     \
    firebase::jni::kMethodTypeStatic)                                          \
  X(Create, "create",                                                          \
    "(Lcom/google/firebase/FirebaseApp;)"                                      \
    "Lcom/google/firebase/appcheck/AppCheckProvider;")

#define JNI_APP_CHECK_DEBUG_HELPER_METHODS(X)                                  \
  X(SetDebugToken, "SetDebugToken",                                            \
    "(Lcom/google/firebase/FirebaseApp;Ljava/lang/String;)V",                  \
    firebase::jni::kMethodTypeStatic)
// clang-format on

JNI_METHOD_LOOKUP_DECLARATION(
    debug_provider_factory, DEBUG_PROVIDER_FACTORY_METHODS, FIELD_LOOKUP_NONE,
    "com/google/firebase/appcheck/debug/DebugAppCheckProviderFactory")
JNI_METHOD_LOOKUP_DECLARATION(
    jni_debug_helper, JNI_APP_CHECK_DEBUG_HELPER_METHODS, FIELD_LOOKUP_NONE,
    "com/google/firebase/appcheck/internal/cpp/JniAppCheckDebugHelper")

namespace {
Mutex& GetFactoryMutex() {
  static auto* mutex = new Mutex();
  return *mutex;
}
std::vector<const DebugAppCheckProviderFactoryInternal*>& GetLiveFactories() {
  static auto* factories =
      new std::vector<const DebugAppCheckProviderFactoryInternal*>();
  return *factories;
}
bool g_methods_cached = false;
}  // namespace

bool CacheDebugProviderMethodIds(
    JNIEnv* env, jobject activity,
    const std::vector<firebase::internal::EmbeddedFile>& embedded_files) {
  MutexLock lock(GetFactoryMutex());
  g_methods_cached =
      debug_provider_factory::CacheMethodIds(env, activity) &&
      jni_debug_helper::CacheClassFromFiles(env, activity, &embedded_files) &&
      jni_debug_helper::CacheMethodIds(env, activity);
  if (!g_methods_cached) {
    debug_provider_factory::ReleaseClass(env);
    jni_debug_helper::ReleaseClass(env);
  }
  return g_methods_cached;
}

void ReleaseDebugProviderClasses(JNIEnv* env) {
  MutexLock lock(GetFactoryMutex());
  debug_provider_factory::ReleaseClass(env);
  jni_debug_helper::ReleaseClass(env);
  g_methods_cached = false;
}

DebugAppCheckProviderFactoryInternal::DebugAppCheckProviderFactoryInternal()
    : android_provider_factory_(nullptr) {
  MutexLock lock(GetFactoryMutex());
  GetLiveFactories().push_back(this);
}

DebugAppCheckProviderFactoryInternal::~DebugAppCheckProviderFactoryInternal() {
  std::map<App*, AppCheckProvider*> providers;
  jobject factory_ref = nullptr;
  {
    MutexLock lock(GetFactoryMutex());
    auto& live = GetLiveFactories();
    live.erase(std::remove(live.begin(), live.end(), this), live.end());
    providers.swap(created_providers_);
    factory_ref = std::exchange(android_provider_factory_, nullptr);
  }
  for (auto& [app, raw_provider] : providers) {
    std::unique_ptr<AppCheckProvider> provider(raw_provider);
    if (auto* notifier = CleanupNotifier::FindByOwner(app);
        notifier && provider) {
      notifier->UnregisterObject(provider.get());
    }
  }
  if (JNIEnv* env = GetJniEnv(); env && factory_ref) {
    env->DeleteGlobalRef(factory_ref);
  }
}

AppCheckProvider* DebugAppCheckProviderFactoryInternal::CreateProvider(
    App* app) {
  if (!app) return nullptr;
  AppCheckProvider* raw_provider = nullptr;
  {
    MutexLock lock(GetFactoryMutex());
    FIREBASE_ASSERT_MESSAGE_RETURN(
        nullptr, g_methods_cached,
        "DebugAppCheckProviderFactory methods were not cached.");
    if (auto it = created_providers_.find(app);
        it != created_providers_.end()) {
      return it->second;
    }
    JNIEnv* env = app->GetJNIEnv();
    if (!env) return nullptr;
    if (!android_provider_factory_) {
      jobject local = env->CallStaticObjectMethod(
          debug_provider_factory::GetClass(),
          debug_provider_factory::GetMethodId(
              debug_provider_factory::kGetInstance));
      if (jni::CheckAndClearException(env) || !local) {
        if (local) env->DeleteLocalRef(local);
        return nullptr;
      }
      android_provider_factory_ = env->NewGlobalRef(local);
      env->DeleteLocalRef(local);
      if (jni::CheckAndClearException(env) || !android_provider_factory_) {
        android_provider_factory_ = nullptr;
        return nullptr;
      }
    }
    jobject platform_app = app->GetPlatformApp();
    if (!platform_app) return nullptr;

    if (!debug_token_.empty()) {
      jstring jtoken = env->NewStringUTF(debug_token_.c_str());
      if (!jni::CheckAndClearException(env) && jtoken) {
        env->CallStaticVoidMethod(
            jni_debug_helper::GetClass(),
            jni_debug_helper::GetMethodId(jni_debug_helper::kSetDebugToken),
            platform_app, jtoken);
        jni::CheckAndClearException(env);
        env->DeleteLocalRef(jtoken);
      }
    }

    jobject j_prov = env->CallObjectMethod(
        android_provider_factory_,
        debug_provider_factory::GetMethodId(debug_provider_factory::kCreate),
        platform_app);
    bool ex = jni::CheckAndClearException(env);
    env->DeleteLocalRef(platform_app);
    if (ex || !j_prov) {
      if (j_prov) env->DeleteLocalRef(j_prov);
      return nullptr;
    }

    raw_provider = new internal::AndroidAppCheckProvider(j_prov);
    env->DeleteLocalRef(j_prov);
    created_providers_[app] = raw_provider;
  }

  if (auto* notifier = CleanupNotifier::FindByOwner(app)) {
    notifier->RegisterObject(
        raw_provider, [this, app, raw_provider](void* /*object*/) {
          std::unique_ptr<AppCheckProvider> to_delete;
          MutexLock cleanup_lock(GetFactoryMutex());
          const auto& live = GetLiveFactories();
          if (std::find(live.begin(), live.end(), this) == live.end()) return;
          if (auto it = created_providers_.find(app);
              it != created_providers_.end() && it->second == raw_provider) {
            to_delete.reset(it->second);
            created_providers_.erase(it);
          }
        });
  }
  return raw_provider;
}

void DebugAppCheckProviderFactoryInternal::SetDebugToken(const char* token) {
  MutexLock lock(GetFactoryMutex());
  debug_token_ = token ? token : "";
}

}  // namespace firebase::app_check::internal
