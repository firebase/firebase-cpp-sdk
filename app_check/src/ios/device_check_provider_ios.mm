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

#include "firebase/app_check/device_check_provider.h"

#import "FIRDeviceCheckProvider.h"
#import "FIRDeviceCheckProviderFactory.h"

#include <memory>

#include "app/src/app_state.h"
#include "app/src/no_destructor.h"
#include "app/src/service_interfaces.h"
#include "app/src/util_ios.h"
#include "app_check/src/ios/app_check_ios.h"
#include "app_check/src/ios/util_ios.h"
#include "firebase/app_check.h"

namespace firebase {
namespace app_check {
namespace internal {

class DeviceCheckProvider : public AppCheckProvider {
 public:
  DeviceCheckProvider(FIRDeviceCheckProvider* provider);
  virtual ~DeviceCheckProvider();

  /// Fetches an AppCheckToken and then calls the provided callback method with
  /// the token or with an error code and error message.
  virtual void GetToken(
      std::function<void(AppCheckToken, int, const std::string&)> completion_callback) override;

  /// Fetches an AppCheckToken suitable for consumption in limited-use scenarios
  /// and then calls the provided callback function with the token or with an
  /// error code and error message.
  virtual void GetLimitedUseToken(
      std::function<void(AppCheckToken, int, const std::string&)> completion_callback) override;

 private:
  FIRDeviceCheckProvider* ios_provider_;
};

DeviceCheckProvider::DeviceCheckProvider(FIRDeviceCheckProvider* provider)
    : ios_provider_(provider) {}

DeviceCheckProvider::~DeviceCheckProvider() {}

void DeviceCheckProvider::GetToken(
    std::function<void(AppCheckToken, int, const std::string&)> completion_callback) {
  [ios_provider_
      getTokenWithCompletion:^(FIRAppCheckToken* _Nullable token, NSError* _Nullable error) {
        completion_callback(firebase::app_check::internal::AppCheckTokenFromFIRAppCheckToken(token),
                            firebase::app_check::internal::AppCheckErrorFromNSError(error),
                            util::NSStringToString(error.localizedDescription).c_str());
      }];
}

void DeviceCheckProvider::GetLimitedUseToken(
    std::function<void(AppCheckToken, int, const std::string&)> completion_callback) {
  [ios_provider_ getLimitedUseTokenWithCompletion:^(FIRAppCheckToken* _Nullable token,
                                                    NSError* _Nullable error) {
    completion_callback(firebase::app_check::internal::AppCheckTokenFromFIRAppCheckToken(token),
                        firebase::app_check::internal::AppCheckErrorFromNSError(error),
                        util::NSStringToString(error.localizedDescription).c_str());
  }];
}

namespace {

const char kIosDeviceCheckProviderServiceTag = 0;

class IosDeviceCheckProviderService : public ::firebase::internal::Service {
 public:
  explicit IosDeviceCheckProviderService(FIRDeviceCheckProvider* ios_provider)
      : provider_(ios_provider) {}

  int teardown_priority() const override { return -20; }

  DeviceCheckProvider* provider() { return &provider_; }

 private:
  DeviceCheckProvider provider_;
};

}  // namespace

DeviceCheckProviderFactoryInternal::DeviceCheckProviderFactoryInternal() {
  ios_provider_factory_ = std::make_unique<FIRDeviceCheckProviderFactoryPointer>(
      [[FIRDeviceCheckProviderFactory alloc] init]);
}

DeviceCheckProviderFactoryInternal::~DeviceCheckProviderFactoryInternal() = default;

AppCheckProvider* DeviceCheckProviderFactoryInternal::CreateProvider(const App& app) {
  auto state = ::firebase::internal::AppState::FromApp(app);
  if (!state) return nullptr;

  auto service = state->GetOrCreateService<IosDeviceCheckProviderService>(
      &kIosDeviceCheckProviderServiceTag, "", [this, &app](::firebase::internal::AppState&) {
        FIRDeviceCheckProvider* ios_provider =
            [ios_provider_factory() createProviderWithApp:app.GetPlatformApp()];
        return std::make_shared<IosDeviceCheckProviderService>(ios_provider);
      });
  return service ? service->provider() : nullptr;
}

}  // namespace internal

DeviceCheckProviderFactory* DeviceCheckProviderFactory::GetInstance() {
  static ::firebase::internal::NoDestructor<DeviceCheckProviderFactory>
      g_device_check_provider_factory;
  return g_device_check_provider_factory.get();
}

DeviceCheckProviderFactory::DeviceCheckProviderFactory()
    : internal_(new internal::DeviceCheckProviderFactoryInternal()) {}

DeviceCheckProviderFactory::~DeviceCheckProviderFactory() {
  if (internal_) {
    delete internal_;
    internal_ = nullptr;
  }
}

AppCheckProvider* DeviceCheckProviderFactory::CreateProvider(const App& app) {
  if (internal_) {
    return internal_->CreateProvider(app);
  }
  return nullptr;
}

}  // namespace app_check
}  // namespace firebase
