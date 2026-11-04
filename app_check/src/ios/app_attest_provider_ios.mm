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

#include "app_check/src/ios/app_attest_provider_ios.h"

#include <algorithm>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#import "FIRAppAttestProvider.h"

#include "app/src/cleanup_notifier.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/util_ios.h"
#include "app_check/src/ios/app_check_ios.h"
#include "app_check/src/ios/util_ios.h"
#include "firebase/app_check.h"
#include "firebase/app_check/app_attest_provider.h"

namespace firebase {
namespace app_check {
namespace internal {
namespace {
Mutex& GetFactoryMutex() {
  static auto* mutex = new Mutex();
  return *mutex;
}
std::vector<const AppAttestProviderFactoryInternal*>& GetLiveFactories() {
  static auto* factories =
      new std::vector<const AppAttestProviderFactoryInternal*>();
  return *factories;
}
}  // namespace

AppAttestProviderFactoryInternal::AppAttestProviderFactoryInternal() {
  MutexLock lock(GetFactoryMutex());
  GetLiveFactories().push_back(this);
}

AppAttestProviderFactoryInternal::~AppAttestProviderFactoryInternal() {
  std::map<App*, AppCheckProvider*> providers;
  {
    MutexLock lock(GetFactoryMutex());
    auto& live = GetLiveFactories();
    live.erase(std::remove(live.begin(), live.end(), this), live.end());
    providers.swap(created_providers_);
  }
  for (auto& [app, raw_provider] : providers) {
    std::unique_ptr<AppCheckProvider> provider(raw_provider);
    if (auto* notifier = CleanupNotifier::FindByOwner(app);
        notifier && provider) {
      notifier->UnregisterObject(provider.get());
    }
  }
}

AppCheckProvider* AppAttestProviderFactoryInternal::CreateProvider(App* app) {
  if (!app || app->GetPlatformApp() == nil) return nullptr;
  AppCheckProvider* raw_provider = nullptr;
  {
    MutexLock lock(GetFactoryMutex());
    if (auto it = created_providers_.find(app);
        it != created_providers_.end()) {
      return it->second;
    }
    FIRAppAttestProvider* ios_provider =
        [[FIRAppAttestProvider alloc] initWithApp:app->GetPlatformApp()];
    if (!ios_provider) return nullptr;
    raw_provider = new IosAppCheckProvider(ios_provider);
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

AppAttestProviderFactory* AppAttestProviderFactory::GetInstance() {
  static auto* g_factory = new AppAttestProviderFactory();
  return g_factory;
}

AppAttestProviderFactory::AppAttestProviderFactory()
    : internal_(new internal::AppAttestProviderFactoryInternal()) {}

AppAttestProviderFactory::~AppAttestProviderFactory() {
  delete std::exchange(internal_, nullptr);
}

AppCheckProvider* AppAttestProviderFactory::CreateProvider(App* app) {
  return (internal_ && app) ? internal_->CreateProvider(app) : nullptr;
}

}  // namespace app_check
}  // namespace firebase
