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

#include "firebase/app_check/app_attest_provider.h"

#import "FIRAppAttestProvider.h"

#include "app/src/util_ios.h"
#include "app_check/src/ios/app_check_ios.h"
#include "app_check/src/ios/util_ios.h"
#include "firebase/app_check.h"

namespace firebase {
namespace app_check {

namespace internal {

class AppAttestProvider : public AppCheckProvider {
 public:
  AppAttestProvider(FIRAppAttestProvider* provider);
  virtual ~AppAttestProvider();

  /// Fetches an AppCheckToken and then calls the provided callback method with
  /// the token or with an error code and error message.
  virtual void GetToken(
      std::function<void(AppCheckToken, int, const std::string&)> completion_callback) override;

 private:
  FIRAppAttestProvider* provider_;
};

AppAttestProvider::AppAttestProvider(FIRAppAttestProvider* provider) : provider_(provider) {}

AppAttestProvider::~AppAttestProvider() {}

void AppAttestProvider::GetToken(
    std::function<void(AppCheckToken, int, const std::string&)> completion_callback) {
  [provider_ getTokenWithCompletion:^(FIRAppCheckToken* _Nullable token, NSError* _Nullable error) {
    completion_callback(firebase::app_check::internal::AppCheckTokenFromFIRAppCheckToken(token),
                        firebase::app_check::internal::AppCheckErrorFromNSError(error),
                        util::NSStringToString(error.localizedDescription).c_str());
  }];
}

}  // namespace internal

static AppAttestProviderFactory* g_app_attest_provider_factory = nullptr;

AppAttestProviderFactory* AppAttestProviderFactory::GetInstance() {
  if (!g_app_attest_provider_factory) {
    g_app_attest_provider_factory = new AppAttestProviderFactory();
  }
  return g_app_attest_provider_factory;
}

// There is no built-in iOS AppAttest factory, so this factory has no state.
AppAttestProviderFactory::AppAttestProviderFactory() {}

AppAttestProviderFactory::~AppAttestProviderFactory() {}

AppCheckProvider* AppAttestProviderFactory::CreateProvider(App* app) {
  // Note: FIRAppAttestProvider is only supported on iOS 14+
  FIRAppAttestProvider* createdProvider =
      [[FIRAppAttestProvider alloc] initWithApp:app->GetPlatformApp()];
  return new internal::AppAttestProvider(createdProvider);
}

}  // namespace app_check
}  // namespace firebase
