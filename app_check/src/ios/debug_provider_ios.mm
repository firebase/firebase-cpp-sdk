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

#include "firebase/app_check/debug_provider.h"

#import "FIRAppCheckDebugProvider.h"
#import "FIRAppCheckDebugProviderFactory.h"

#include "app/src/util_ios.h"
#include "app_check/src/ios/app_check_ios.h"
#include "app_check/src/ios/util_ios.h"
#include "firebase/app_check.h"

namespace firebase {
namespace app_check {
namespace internal {

class DebugAppCheckProvider : public AppCheckProvider {
 public:
  DebugAppCheckProvider(FIRAppCheckDebugProvider* provider);
  virtual ~DebugAppCheckProvider();

  /// Fetches an AppCheckToken and then calls the provided callback method with
  /// the token or with an error code and error message.
  virtual void GetToken(
      std::function<void(AppCheckToken, int, const std::string&)> completion_callback) override;

 private:
  FIRAppCheckDebugProvider* ios_provider_;
};

DebugAppCheckProvider::DebugAppCheckProvider(FIRAppCheckDebugProvider* provider)
    : ios_provider_(provider) {}

DebugAppCheckProvider::~DebugAppCheckProvider() {}

void DebugAppCheckProvider::GetToken(
    std::function<void(AppCheckToken, int, const std::string&)> completion_callback) {
  [ios_provider_
      getTokenWithCompletion:^(FIRAppCheckToken* _Nullable token, NSError* _Nullable error) {
        completion_callback(firebase::app_check::internal::AppCheckTokenFromFIRAppCheckToken(token),
                            firebase::app_check::internal::AppCheckErrorFromNSError(error),
                            util::NSStringToString(error.localizedDescription).c_str());
      }];
}

DebugAppCheckProviderFactoryInternal::DebugAppCheckProviderFactoryInternal()
    : created_providers_() {
  ios_provider_factory_ = [[FIRAppCheckDebugProviderFactory alloc] init];
}

DebugAppCheckProviderFactoryInternal::~DebugAppCheckProviderFactoryInternal() {
  // Cleanup any providers created by this factory.
  for (auto it = created_providers_.begin(); it != created_providers_.end(); ++it) {
    delete it->second;
  }
  created_providers_.clear();
}

AppCheckProvider* DebugAppCheckProviderFactoryInternal::CreateProvider(App* app) {
  // Return the provider if it already exists.
  std::map<App*, AppCheckProvider*>::iterator it = created_providers_.find(app);
  if (it != created_providers_.end()) {
    return it->second;
  }
  // Otherwise, create a new provider
  FIRAppCheckDebugProvider* ios_provider =
      [ios_provider_factory_ createProviderWithApp:app->GetPlatformApp()];
  AppCheckProvider* cpp_provider = new internal::DebugAppCheckProvider(ios_provider);
  created_providers_[app] = cpp_provider;
  return cpp_provider;
}

}  // namespace internal
}  // namespace app_check
}  // namespace firebase
