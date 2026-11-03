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

#include "app_check/src/ios/device_check_provider_ios.h"

#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "firebase/app_check/device_check_provider.h"

#import "FIRDeviceCheckProvider.h"
#import "FIRDeviceCheckProviderFactory.h"

#include "app/src/cleanup_notifier.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/util_ios.h"
#include "app_check/src/ios/app_check_ios.h"
#include "app_check/src/ios/util_ios.h"
#include "firebase/app_check.h"

namespace firebase {
namespace app_check {
namespace internal {

namespace {

Mutex& GetFactoryMutex() {
  static auto* mutex = new Mutex();
  return *mutex;
}

std::vector<const DeviceCheckProviderFactoryInternal*>& GetLiveFactories() {
  static auto* factories =
      new std::vector<const DeviceCheckProviderFactoryInternal*>();
  return *factories;
}

}  // namespace

class DeviceCheckProvider : public AppCheckProvider {
 public:
  explicit DeviceCheckProvider(FIRDeviceCheckProvider* provider);
  ~DeviceCheckProvider() override = default;

  /// Fetches an AppCheckToken and then calls the provided callback method with
  /// the token or with an error code and error message.
  void GetToken(
      std::function<void(AppCheckToken, int, const std::string&)> completion_callback) override;

  /// Fetches an AppCheckToken suitable for consumption in limited-use scenarios
  /// and then calls the provided callback function with the token or with an
  /// error code and error message.
  void GetLimitedUseToken(
      std::function<void(AppCheckToken, int, const std::string&)> completion_callback) override;

 private:
  FIRDeviceCheckProvider* ios_provider_;
};

DeviceCheckProvider::DeviceCheckProvider(FIRDeviceCheckProvider* provider)
    : ios_provider_(provider) {}

void DeviceCheckProvider::GetToken(
    std::function<void(AppCheckToken, int, const std::string&)> completion_callback) {
  if (!completion_callback) {
    return;
  }
  if (!ios_provider_) {
    completion_callback(AppCheckToken{}, kAppCheckErrorUnknown, "Provider not initialized");
    return;
  }
  [ios_provider_
      getTokenWithCompletion:^(FIRAppCheckToken* _Nullable token, NSError* _Nullable error) {
        if (!completion_callback) {
          return;
        }
        completion_callback(firebase::app_check::internal::AppCheckTokenFromFIRAppCheckToken(token),
                            firebase::app_check::internal::AppCheckErrorFromNSError(error),
                            util::NSStringToString(error.localizedDescription));
      }];
}

void DeviceCheckProvider::GetLimitedUseToken(
    std::function<void(AppCheckToken, int, const std::string&)> completion_callback) {
  if (!completion_callback) {
    return;
  }
  if (!ios_provider_) {
    completion_callback(AppCheckToken{}, kAppCheckErrorUnknown, "Provider not initialized");
    return;
  }
  [ios_provider_ getLimitedUseTokenWithCompletion:^(FIRAppCheckToken* _Nullable token,
                                                    NSError* _Nullable error) {
    if (!completion_callback) {
      return;
    }
    completion_callback(firebase::app_check::internal::AppCheckTokenFromFIRAppCheckToken(token),
                        firebase::app_check::internal::AppCheckErrorFromNSError(error),
                        util::NSStringToString(error.localizedDescription));
  }];
}

DeviceCheckProviderFactoryInternal::DeviceCheckProviderFactoryInternal() : created_providers_() {
  ios_provider_factory_ = std::make_unique<FIRDeviceCheckProviderFactoryPointer>(
      [[FIRDeviceCheckProviderFactory alloc] init]);
  MutexLock lock(GetFactoryMutex());
  GetLiveFactories().push_back(this);
}

DeviceCheckProviderFactoryInternal::~DeviceCheckProviderFactoryInternal() {
  // Note: created_providers_ is declared as std::map<App*, AppCheckProvider*>
  // in device_check_provider_ios.h; ownership is managed via std::unique_ptr on
  // creation and eviction.
  std::map<App*, AppCheckProvider*> providers;
  {
    MutexLock lock(GetFactoryMutex());
    auto& live = GetLiveFactories();
    live.erase(std::remove(live.begin(), live.end(), this), live.end());
    providers.swap(created_providers_);
  }
  for (auto& [app, raw_provider] : providers) {
    std::unique_ptr<AppCheckProvider> provider(raw_provider);
    CleanupNotifier* notifier = CleanupNotifier::FindByOwner(app);
    if (notifier != nullptr && provider != nullptr) {
      notifier->UnregisterObject(provider.get());
    }
  }
}

AppCheckProvider* DeviceCheckProviderFactoryInternal::CreateProvider(App* app) {
  if (!app || app->GetPlatformApp() == nil) {
    return nullptr;
  }
  MutexLock lock(GetFactoryMutex());
  // Return the provider if it already exists.
  auto it = created_providers_.find(app);
  if (it != created_providers_.end()) {
    return it->second;
  }
  if (!ios_provider_factory_ || ios_provider_factory() == nil) {
    return nullptr;
  }
  // Otherwise, create a new provider.
  FIRDeviceCheckProvider* ios_provider =
      [ios_provider_factory() createProviderWithApp:app->GetPlatformApp()];
  if (!ios_provider) {
    return nullptr;
  }
  auto cpp_provider = std::make_unique<internal::DeviceCheckProvider>(ios_provider);
  AppCheckProvider* raw_provider = cpp_provider.release();
  created_providers_[app] = raw_provider;

  CleanupNotifier* notifier = CleanupNotifier::FindByOwner(app);
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

DeviceCheckProviderFactory* DeviceCheckProviderFactory::GetInstance() {
  static auto* g_device_check_provider_factory = new DeviceCheckProviderFactory();
  return g_device_check_provider_factory;
}

DeviceCheckProviderFactory::DeviceCheckProviderFactory()
    : internal_(new internal::DeviceCheckProviderFactoryInternal()) {}

DeviceCheckProviderFactory::~DeviceCheckProviderFactory() {
  delete internal_;
  internal_ = nullptr;
}

AppCheckProvider* DeviceCheckProviderFactory::CreateProvider(App* app) {
  if (internal_ && app) {
    return internal_->CreateProvider(app);
  }
  return nullptr;
}

}  // namespace app_check
}  // namespace firebase
