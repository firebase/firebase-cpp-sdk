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

#include "app_check/src/android/play_integrity_provider_android.h"

#include <algorithm>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "app/src/app_common.h"
#include "app/src/assert.h"
#include "app/src/cleanup_notifier.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/jni/declaration.h"
#include "app/src/jni/exception.h"
#include "app_check/src/android/common_android.h"
#include "firebase/app_check/play_integrity_provider.h"

namespace firebase {
namespace app_check {
namespace internal {

// clang-format off
#define PLAY_INTEGRITY_PROVIDER_FACTORY_METHODS(X)                             \
  X(GetInstance, "getInstance",                                                \
    "()Lcom/google/firebase/appcheck/playintegrity/"                           \
    "PlayIntegrityAppCheckProviderFactory;",                                   \
    firebase::jni::kMethodTypeStatic)                                          \
  X(Create, "create",                                                          \
    "(Lcom/google/firebase/FirebaseApp;)"                                      \
    "Lcom/google/firebase/appcheck/AppCheckProvider;")
// clang-format on

JNI_METHOD_LOOKUP_DECLARATION(
    play_integrity_provider_factory, PLAY_INTEGRITY_PROVIDER_FACTORY_METHODS,
    FIELD_LOOKUP_NONE,
    "com/google/firebase/appcheck/playintegrity/"
    "PlayIntegrityAppCheckProviderFactory")

namespace {
Mutex& GetFactoryMutex() {
  static auto* mutex = new Mutex();
  return *mutex;
}
std::vector<const PlayIntegrityProviderFactoryInternal*>& GetLiveFactories() {
  static auto* factories =
      new std::vector<const PlayIntegrityProviderFactoryInternal*>();
  return *factories;
}
bool g_methods_cached = false;
}  // namespace

bool CachePlayIntegrityProviderMethodIds(JNIEnv* env, jobject activity) {
  MutexLock lock(GetFactoryMutex());
  g_methods_cached =
      play_integrity_provider_factory::CacheMethodIds(env, activity);
  return g_methods_cached;
}

void ReleasePlayIntegrityProviderClasses(JNIEnv* env) {
  MutexLock lock(GetFactoryMutex());
  play_integrity_provider_factory::ReleaseClass(env);
  g_methods_cached = false;
}

PlayIntegrityProviderFactoryInternal::PlayIntegrityProviderFactoryInternal()
    : android_provider_factory_(nullptr) {
  MutexLock lock(GetFactoryMutex());
  GetLiveFactories().push_back(this);
}

PlayIntegrityProviderFactoryInternal::~PlayIntegrityProviderFactoryInternal() {
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

AppCheckProvider* PlayIntegrityProviderFactoryInternal::CreateProvider(
    App* app) {
  if (!app) return nullptr;
  AppCheckProvider* raw_provider = nullptr;
  {
    MutexLock lock(GetFactoryMutex());
    FIREBASE_ASSERT_MESSAGE_RETURN(
        nullptr, g_methods_cached,
        "PlayIntegrityProviderFactory methods were not cached.");
    if (auto it = created_providers_.find(app);
        it != created_providers_.end()) {
      return it->second;
    }
    JNIEnv* env = app->GetJNIEnv();
    if (!env) return nullptr;
    if (!android_provider_factory_) {
      jobject local = env->CallStaticObjectMethod(
          play_integrity_provider_factory::GetClass(),
          play_integrity_provider_factory::GetMethodId(
              play_integrity_provider_factory::kGetInstance));
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
    jobject j_prov = env->CallObjectMethod(
        android_provider_factory_,
        play_integrity_provider_factory::GetMethodId(
            play_integrity_provider_factory::kCreate),
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

}  // namespace internal

PlayIntegrityProviderFactory* PlayIntegrityProviderFactory::GetInstance() {
  static auto* g_factory = new PlayIntegrityProviderFactory();
  return g_factory;
}

PlayIntegrityProviderFactory::PlayIntegrityProviderFactory()
    : internal_(new internal::PlayIntegrityProviderFactoryInternal()) {}

PlayIntegrityProviderFactory::~PlayIntegrityProviderFactory() {
  delete std::exchange(internal_, nullptr);
}

AppCheckProvider* PlayIntegrityProviderFactory::CreateProvider(App* app) {
  return (internal_ && app) ? internal_->CreateProvider(app) : nullptr;
}

}  // namespace app_check
}  // namespace firebase
