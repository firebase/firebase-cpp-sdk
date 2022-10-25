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
  FIRAppCheckDebugProvider* provider_;
};


DebugAppCheckProvider::DebugAppCheckProvider(FIRAppCheckDebugProvider* provider)
    : provider_(provider) {}

DebugAppCheckProvider::~DebugAppCheckProvider() {}

void DebugAppCheckProvider::GetToken(
    std::function<void(AppCheckToken, int, const std::string&)> completion_callback) {
  [provider_ getTokenWithCompletion:^(FIRAppCheckToken* _Nullable token, NSError* _Nullable error) {
    completion_callback(firebase::app_check::internal::AppCheckTokenFromFIRAppCheckToken(token),
                        firebase::app_check::internal::AppCheckErrorFromNSError(error),
                        util::NSStringToString(error.localizedDescription).c_str());
  }];
}

DebugAppCheckProviderFactoryInternal::DebugAppCheckProviderFactoryInternal() {
  // TODO: initialize this properly
  ios_provider_factory_ = [[FIRAppCheckDebugProviderFactory alloc] init];
}

DebugAppCheckProviderFactoryInternal::~DebugAppCheckProviderFactoryInternal() {
  // TODO: release ios_provider_factory_ if needed
}

AppCheckProvider* DebugAppCheckProviderFactoryInternal::CreateProvider(App* app) {
  FIRAppCheckDebugProvider* createdProvider =
      [ios_provider_factory_ createProviderWithApp:app->GetPlatformApp()];
  return new internal::DebugAppCheckProvider(createdProvider);
}

}  // namespace internal
}  // namespace app_check
}  // namespace firebase
