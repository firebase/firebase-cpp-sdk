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

#include "app/src/app_state.h"
#include "app/src/log.h"
#include "app/src/service_interfaces.h"
#include "app/src/util_ios.h"
#include "app_check/src/ios/app_check_ios.h"
#include "app_check/src/ios/util_ios.h"
#include "firebase/app_check.h"

namespace firebase {
namespace app_check {
namespace internal {

// The keys used by the iOS SDK to get the debug token from.
static NSString* const kDebugTokenUserDefaultsKey = @"FIRAAppCheckDebugToken";
static NSString* const kGACDebugTokenUserDefaultsKey = @"GACAppCheckDebugToken";

class DebugAppCheckProvider : public AppCheckProvider {
 public:
  DebugAppCheckProvider(FIRAppCheckDebugProvider* provider);
  virtual ~DebugAppCheckProvider();

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

void DebugAppCheckProvider::GetLimitedUseToken(
    std::function<void(AppCheckToken, int, const std::string&)> completion_callback) {
  [ios_provider_ getLimitedUseTokenWithCompletion:^(FIRAppCheckToken* _Nullable token,
                                                    NSError* _Nullable error) {
    completion_callback(firebase::app_check::internal::AppCheckTokenFromFIRAppCheckToken(token),
                        firebase::app_check::internal::AppCheckErrorFromNSError(error),
                        util::NSStringToString(error.localizedDescription).c_str());
  }];
}

namespace {

const char kIosDebugProviderServiceTag = 0;

class IosDebugProviderService : public ::firebase::internal::Service {
 public:
  explicit IosDebugProviderService(FIRAppCheckDebugProvider* ios_provider)
      : provider_(ios_provider) {}

  int teardown_priority() const override { return -20; }

  DebugAppCheckProvider* provider() { return &provider_; }

 private:
  DebugAppCheckProvider provider_;
};

}  // namespace

DebugAppCheckProviderFactoryInternal::DebugAppCheckProviderFactoryInternal() {
  ios_provider_factory_ = std::make_unique<FIRAppCheckDebugProviderFactoryPointer>(
      [[FIRAppCheckDebugProviderFactory alloc] init]);
}

DebugAppCheckProviderFactoryInternal::~DebugAppCheckProviderFactoryInternal() = default;

AppCheckProvider* DebugAppCheckProviderFactoryInternal::CreateProvider(const App& app) {
  firebase::LogWarning("[FirebaseAppCheck] DebugAppCheckProviderFactory is in use. Debug tokens "
                       "are for development only; prefer a production provider where one is "
                       "available.");
  auto state = ::firebase::internal::AppState::FromApp(app);
  if (!state) return nullptr;

  auto service = state->GetOrCreateService<IosDebugProviderService>(
      &kIosDebugProviderServiceTag, "", [this, &app](::firebase::internal::AppState&) {
        FIRAppCheckDebugProvider* ios_provider =
            [ios_provider_factory() createProviderWithApp:app.GetPlatformApp()];
        return std::make_shared<IosDebugProviderService>(ios_provider);
      });
  return service ? service->provider() : nullptr;
}

void DebugAppCheckProviderFactoryInternal::SetDebugToken(const std::string& token) {
  // Write to the same location that the iOS SDK will read from.
  NSString* token_string = (!token.empty()) ? util::StringToNSString(token) : nullptr;
  [[NSUserDefaults standardUserDefaults] setObject:token_string forKey:kDebugTokenUserDefaultsKey];
  [[NSUserDefaults standardUserDefaults] setObject:token_string
                                            forKey:kGACDebugTokenUserDefaultsKey];
}

}  // namespace internal
}  // namespace app_check
}  // namespace firebase
