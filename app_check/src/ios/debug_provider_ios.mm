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

#include "app_check/src/ios/debug_provider_ios.h"

#include <algorithm>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#import "FIRAppCheckDebugProvider.h"
#import "FIRAppCheckDebugProviderFactory.h"

#include "app/src/cleanup_notifier.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/util_ios.h"
#include "app_check/src/ios/app_check_ios.h"
#include "app_check/src/ios/util_ios.h"
#include "firebase/app_check.h"
#include "firebase/app_check/debug_provider.h"

namespace firebase::app_check::internal {
namespace {
NSString* const kDebugTokenUserDefaultsKey = @"FIRAAppCheckDebugToken";
NSString* const kGACDebugTokenUserDefaultsKey = @"GACAppCheckDebugToken";

Mutex& GetFactoryMutex() {
  static auto* mutex = new Mutex();
  return *mutex;
}
std::vector<const DebugAppCheckProviderFactoryInternal*>& GetLiveFactories() {
  static auto* factories =
      new std::vector<const DebugAppCheckProviderFactoryInternal*>();
  return *factories;
}
}  // namespace

DebugAppCheckProviderFactoryInternal::DebugAppCheckProviderFactoryInternal()
    : ios_provider_factory_(
          std::make_unique<FIRAppCheckDebugProviderFactoryPointer>(
              [[FIRAppCheckDebugProviderFactory alloc] init])) {
  MutexLock lock(GetFactoryMutex());
  GetLiveFactories().push_back(this);
}

DebugAppCheckProviderFactoryInternal::~DebugAppCheckProviderFactoryInternal() {
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

AppCheckProvider* DebugAppCheckProviderFactoryInternal::CreateProvider(
    App* app) {
  if (!app || app->GetPlatformApp() == nil) return nullptr;
  AppCheckProvider* raw_provider = nullptr;
  {
    MutexLock lock(GetFactoryMutex());
    if (auto it = created_providers_.find(app);
        it != created_providers_.end()) {
      return it->second;
    }
    if (!ios_provider_factory_ || ios_provider_factory() == nil) return nullptr;
    id<FIRAppCheckProvider> ios_provider =
        [ios_provider_factory() createProviderWithApp:app->GetPlatformApp()];
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

void DebugAppCheckProviderFactoryInternal::SetDebugToken(const char* token) {
  NSString* token_string =
      (token && token[0] != '\0') ? util::StringToNSString(token) : nil;
  [[NSUserDefaults standardUserDefaults] setObject:token_string
                                            forKey:kDebugTokenUserDefaultsKey];
  [[NSUserDefaults standardUserDefaults]
      setObject:token_string
         forKey:kGACDebugTokenUserDefaultsKey];
}

}  // namespace firebase::app_check::internal
