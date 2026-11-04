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
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/util_android.h"
#include "app_check/src/android/common_android.h"
#include "firebase/app_check/play_integrity_provider.h"

namespace firebase {
namespace app_check {
namespace internal {

// Used to setup the cache of PlayIntegrityProviderFactory class method IDs to
// reduce time spent looking up methods by string.
// clang-format off
#define PLAY_INTEGRITY_PROVIDER_FACTORY_METHODS(X)                                      \
  X(GetInstance, "getInstance",                                                \
    "()Lcom/google/firebase/appcheck/playintegrity/PlayIntegrityAppCheckProviderFactory;",     \
       util::kMethodTypeStatic),                                               \
  X(Create, "create",                                                          \
    "(Lcom/google/firebase/FirebaseApp;)"                                      \
    "Lcom/google/firebase/appcheck/AppCheckProvider;")
// clang-format on

METHOD_LOOKUP_DECLARATION(play_integrity_provider_factory,
                          PLAY_INTEGRITY_PROVIDER_FACTORY_METHODS)
METHOD_LOOKUP_DEFINITION(play_integrity_provider_factory,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/appcheck/playintegrity/"
                         "PlayIntegrityAppCheckProviderFactory",
                         PLAY_INTEGRITY_PROVIDER_FACTORY_METHODS)

namespace {

constexpr char kMethodsNotCachedError[] =
    "PlayIntegrityProviderFactory methods were not cached.";

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
  // Cache the PlayIntegrityProvider classes.
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
    : android_provider_factory_(nullptr), created_providers_() {
  MutexLock lock(GetFactoryMutex());
  GetLiveFactories().push_back(this);
}

PlayIntegrityProviderFactoryInternal::~PlayIntegrityProviderFactoryInternal() {
  // Note: created_providers_ is declared as std::map<App*, AppCheckProvider*>
  // in play_integrity_provider_android.h; ownership is managed via
  // std::unique_ptr on creation and eviction.
  std::map<App*, AppCheckProvider*> providers;
  jobject factory_ref = nullptr;
  {
    MutexLock lock(GetFactoryMutex());
    auto& live = GetLiveFactories();
    live.erase(std::remove(live.begin(), live.end(), this), live.end());
    providers.swap(created_providers_);
    factory_ref = android_provider_factory_;
    android_provider_factory_ = nullptr;
  }
  for (auto& [app, raw_provider] : providers) {
    std::unique_ptr<AppCheckProvider> provider(raw_provider);
;
    if (notifier != nullptr && provider != nullptr) {
      notifier->UnregisterObject(provider.get());
    }
  }
  JNIEnv* env = GetJniEnv();
  if (env != nullptr && factory_ref != nullptr) {
    env->DeleteGlobalRef(factory_ref);
  }
}

AppCheckProvider* PlayIntegrityProviderFactoryInternal::CreateProvider(
    App* app) {
  if (!app) return nullptr;
  AppCheckProvider* raw_provider = nullptr;
  {
    MutexLock lock(GetFactoryMutex());
    FIREBASE_ASSERT_MESSAGE_RETURN(nullptr, g_methods_cached,
                                   kMethodsNotCachedError);

    // Return the provider if it already exists.
    auto it = created_providers_.find(app);
    if (it != created_providers_.end()) {
      return it->second;
    }
    JNIEnv* env = app->GetJNIEnv();
    if (env == nullptr) {
      return nullptr;
    }
    // Create a provider factory first if needed.
    if (android_provider_factory_ == nullptr) {
      jobject j_provider_factory_local = env->CallStaticObjectMethod(
          play_integrity_provider_factory::GetClass(),
          play_integrity_provider_factory::GetMethodId(
              play_integrity_provider_factory::kGetInstance));
      if (util::CheckAndClearJniExceptions(env) ||
          j_provider_factory_local == nullptr) {
        if (j_provider_factory_local != nullptr) {
          env->DeleteLocalRef(j_provider_factory_local);
        }
        return nullptr;
      }
      // Hold a global reference. It will be freed when this factory is destroyed.
      android_provider_factory_ = env->NewGlobalRef(j_provider_factory_local);
      env->DeleteLocalRef(j_provider_factory_local);
      if (util::CheckAndClearJniExceptions(env) ||
          android_provider_factory_ == nullptr) {
        android_provider_factory_ = nullptr;
        return nullptr;
      }
    }
    jobject platform_app = app->GetPlatformApp();
    if (platform_app == nullptr) {
      return nullptr;
    }
    jobject j_android_provider_local =
        env->CallObjectMethod(android_provider_factory_,
                              play_integrity_provider_factory::GetMethodId(
                                  play_integrity_provider_factory::kCreate),
                              platform_app);
    bool create_exception = util::CheckAndClearJniExceptions(env);
    env->DeleteLocalRef(platform_app);
    if (create_exception || j_android_provider_local == nullptr) {
      if (j_android_provider_local != nullptr) {
        env->DeleteLocalRef(j_android_provider_local);
      }
      return nullptr;
    }

    // Pass a local ref into the C++ provider constructor; it creates its own
    // global ref and releases it when the C++ provider is destroyed.
    auto cpp_provider =
        std::make_unique<internal::AndroidAppCheckProvider>(
            j_android_provider_local);
    env->DeleteLocalRef(j_android_provider_local);
    raw_provider = cpp_provider.release();
    created_providers_[app] = raw_provider;
  }

;
  if (notifier != nullptr) {
    notifier->RegisterObject(
        raw_provider, [this, app, raw_provider](void* /*object*/) {
          std::unique_ptr<AppCheckProvider> provider_to_delete;
          {
            MutexLock cleanup_lock(GetFactoryMutex());
            const auto& live = GetLiveFactories();
            if (std::find(live.begin(), live.end(), this) == live.end()) {
              return;
            }
            auto provider_it = created_providers_.find(app);
            if (provider_it != created_providers_.end() &&
                provider_it->second == raw_provider) {
              provider_to_delete.reset(provider_it->second);
              created_providers_.erase(provider_it);
            }
          }
        });
  }
  return raw_provider;
}

}  // namespace internal

PlayIntegrityProviderFactory* PlayIntegrityProviderFactory::GetInstance() {
  static auto* g_play_integrity_provider_factory =
      new PlayIntegrityProviderFactory();
  return g_play_integrity_provider_factory;
}

PlayIntegrityProviderFactory::PlayIntegrityProviderFactory()
    : internal_(new internal::PlayIntegrityProviderFactoryInternal()) {}

PlayIntegrityProviderFactory::~PlayIntegrityProviderFactory() {
  delete internal_;
  internal_ = nullptr;
}

AppCheckProvider* PlayIntegrityProviderFactory::CreateProvider(App* app) {
  if (internal_ && app) {
    return internal_->CreateProvider(app);
  }
  return nullptr;
}

}  // namespace app_check
}  // namespace firebase
