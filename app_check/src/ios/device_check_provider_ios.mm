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

#include "firebase/app_check/device_check_provider.h"

#import "FIRDeviceCheckProvider.h"
#import "FIRDeviceCheckProviderFactory.h"

#include "app/src/util_ios.h"
#include "app_check/src/ios/util_ios.h"
#include "app_check/src/ios/app_check_ios.h"
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

 private:
  FIRDeviceCheckProvider* provider_;
};

DeviceCheckProvider::DeviceCheckProvider(FIRDeviceCheckProvider* provider)
    : provider_(provider) {}

DeviceCheckProvider::~DeviceCheckProvider() {}

void DeviceCheckProvider::GetToken(
    std::function<void(AppCheckToken, int, const std::string&)> completion_callback) {
  [provider_ getTokenWithCompletion:^(FIRAppCheckToken* _Nullable token, NSError* _Nullable error) {
    completion_callback(firebase::app_check::internal::AppCheckTokenFromFIRAppCheckToken(token),
                        firebase::app_check::internal::AppCheckErrorFromNSError(error),
                        util::NSStringToString(error.localizedDescription).c_str());
  }];
}

}  // namespace internal

static DeviceCheckProviderFactory* g_device_check_check_provider_factory = nullptr;
static FIRDeviceCheckProviderFactory* g_ios_device_check_check_provider_factory = nullptr;

DeviceCheckProviderFactory* DeviceCheckProviderFactory::GetInstance() {
  if (!g_device_check_check_provider_factory) {
    g_device_check_check_provider_factory = new DeviceCheckProviderFactory();
  }
  return g_device_check_check_provider_factory;
}

DeviceCheckProviderFactory::DeviceCheckProviderFactory() {
  if (!g_ios_device_check_check_provider_factory) {
    g_ios_device_check_check_provider_factory = [[FIRDeviceCheckProviderFactory alloc] init];
  }
}

DeviceCheckProviderFactory::~DeviceCheckProviderFactory() {}

AppCheckProvider* DeviceCheckProviderFactory::CreateProvider(App* app) {
  FIRDeviceCheckProvider* createdProvider =
      [g_ios_device_check_check_provider_factory createProviderWithApp:app->GetPlatformApp()];
  return new internal::DeviceCheckProvider(createdProvider);
}

}  // namespace app_check
}  // namespace firebase

