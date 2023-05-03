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

#include "app_check/src/ios/app_check_ios.h"

#import "FIRApp.h"
#import "FIRAppCheck.h"
#import "FIRAppCheckProvider.h"
#import "FIRAppCheckProviderFactory.h"
#import "FIRAppCheckToken.h"

#include "app/src/app_common.h"
#include "app/src/app_ios.h"
#include "app/src/util_ios.h"
#include "app_check/src/common/common.h"
#include "app_check/src/ios/util_ios.h"
#include "firebase/app_check.h"

// Defines an iOS AppCheckProvider that wraps a given C++ Provider.
@interface CppAppCheckProvider : NSObject <FIRAppCheckProvider>

@property(nonatomic, nullable) firebase::app_check::AppCheckProvider* cppProvider;

- (id)initWithProvider:(firebase::app_check::AppCheckProvider* _Nonnull)provider;

@end

@implementation CppAppCheckProvider

- (id)initWithProvider:(firebase::app_check::AppCheckProvider* _Nonnull)provider {
  self = [super init];
  if (self) {
    self.cppProvider = provider;
  }
  return self;
}

- (void)getTokenWithCompletion:(nonnull void (^)(FIRAppCheckToken* _Nullable,
                                                 NSError* _Nullable))handler {
  auto token_callback{[handler](firebase::app_check::AppCheckToken token, int error_code,
                                const std::string& error_message) {
    NSError* ios_error = firebase::app_check::internal::AppCheckErrorToNSError(
        static_cast<firebase::app_check::AppCheckError>(error_code), error_message);
    FIRAppCheckToken* ios_token =
        firebase::app_check::internal::AppCheckTokenToFIRAppCheckToken(token);
    handler(ios_token, ios_error);
  }};
  _cppProvider->GetToken(token_callback);
}

@end

// Defines an iOS AppCheckProviderFactory that wraps a given C++ Factory.
@interface CppAppCheckProviderFactory : NSObject <FIRAppCheckProviderFactory>

@property(nonatomic, nullable) firebase::app_check::AppCheckProviderFactory* cppProviderFactory;

- (id)initWithProviderFactory:(firebase::app_check::AppCheckProviderFactory* _Nonnull)factory;

@end

@implementation CppAppCheckProviderFactory

- (id)initWithProviderFactory:(firebase::app_check::AppCheckProviderFactory* _Nonnull)factory {
  self = [super init];
  if (self) {
    self.cppProviderFactory = factory;
  }
  return self;
}

- (nullable id<FIRAppCheckProvider>)createProviderWithApp:(FIRApp*)app {
  std::string app_name = firebase::util::NSStringToString(app.name);
  firebase::App* cpp_app = firebase::app_common::FindAppByName(app_name.c_str());
  if (cpp_app == nullptr) {
    cpp_app = firebase::internal::FindPartialAppByName(app_name.c_str());
  }
  firebase::app_check::AppCheckProvider* cppProvider = _cppProviderFactory->CreateProvider(cpp_app);
  return [[CppAppCheckProvider alloc] initWithProvider:cppProvider];
}

@end

@implementation AppCheckNotificationCenterWrapper

// A collection of NSValues containing AppCheckListener*
NSMutableArray* listeners_;

- (id)init {
  self = [super init];
  if (self) {
    listeners_ = [[NSMutableArray alloc] init];
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(appCheckTokenDidChangeNotification:)
                                                 name:FIRAppCheckAppCheckTokenDidChangeNotification
                                               object:nil];
  }
  return self;
}

- (void)stopListening {
  [[NSNotificationCenter defaultCenter] removeObserver:self
                                                  name:FIRAppCheckAppCheckTokenDidChangeNotification
                                                object:nil];
}

- (void)addListener:(firebase::app_check::AppCheckListener* _Nonnull)listener {
  [listeners_ addObject:[NSValue valueWithPointer:listener]];
}

- (void)removeListener:(firebase::app_check::AppCheckListener* _Nonnull)listener {
  [listeners_ removeObject:[NSValue valueWithPointer:listener]];
}

- (void)appCheckTokenDidChangeNotification:(NSNotification*)notification {
  NSDictionary* userInfo = notification.userInfo;
  NSString* app_name = (NSString*)userInfo[kFIRAppCheckAppNameNotificationKey];
  NSString* token = (NSString*)userInfo[kFIRAppCheckTokenNotificationKey];
  // TODO(b/263138261): Include expiration time in this app check token.
  // Note: The notification contains a token string, but does not currently
  // contain expiration time.
  firebase::app_check::AppCheckToken cpp_token;
  cpp_token.token = firebase::util::NSStringToString(token);

  for (NSValue* listener_value in listeners_) {
    firebase::app_check::AppCheckListener* listener =
        static_cast<firebase::app_check::AppCheckListener*>(listener_value.pointerValue);
    listener->OnAppCheckTokenChanged(cpp_token);
  }
}

@end

namespace firebase {
namespace app_check {
namespace internal {

AppCheckInternal::AppCheckInternal(App* app) : app_(app) {
  future_manager().AllocFutureApi(this, kAppCheckFnCount);
  impl_ = MakeUnique<FIRAppCheckPointer>([FIRAppCheck appCheck]);
  notification_center_wrapper_ = MakeUnique<AppCheckNotificationCenterWrapperPointer>(
      [[AppCheckNotificationCenterWrapper alloc] init]);
}

AppCheckInternal::~AppCheckInternal() {
  [notification_center_wrapper() stopListening];
  future_manager().ReleaseFutureApi(this);
  app_ = nullptr;
}

::firebase::App* AppCheckInternal::app() const { return app_; }

ReferenceCountedFutureImpl* AppCheckInternal::future() {
  return future_manager().GetFutureApi(this);
}

void AppCheckInternal::SetAppCheckProviderFactory(AppCheckProviderFactory* factory) {
  CppAppCheckProviderFactory* ios_factory =
      [[CppAppCheckProviderFactory alloc] initWithProviderFactory:factory];
  [FIRAppCheck setAppCheckProviderFactory:ios_factory];
}

void AppCheckInternal::SetTokenAutoRefreshEnabled(bool is_token_auto_refresh_enabled) {
  impl().isTokenAutoRefreshEnabled = is_token_auto_refresh_enabled;
}

Future<AppCheckToken> AppCheckInternal::GetAppCheckToken(bool force_refresh) {
  __block SafeFutureHandle<AppCheckToken> handle =
      future()->SafeAlloc<AppCheckToken>(kAppCheckFnGetAppCheckToken);

  [impl()
      tokenForcingRefresh:force_refresh
               completion:^(FIRAppCheckToken* _Nullable token, NSError* _Nullable error) {
                 AppCheckToken cpp_token = AppCheckTokenFromFIRAppCheckToken(token);
                 if (error != nil) {
                   NSLog(@"Unable to retrieve App Check token: %@", error);
                   int error_code = firebase::app_check::internal::AppCheckErrorFromNSError(error);
                   std::string error_message = util::NSStringToString(error.localizedDescription);

                   future()->CompleteWithResult(handle, error_code, error_message.c_str(),
                                                cpp_token);
                   return;
                 }
                 if (token == nil) {
                   NSLog(@"App Check token is nil.");
                   future()->CompleteWithResult(handle, kAppCheckErrorUnknown,
                                                "AppCheck GetToken returned an empty token.",
                                                cpp_token);
                   return;
                 }
                 future()->CompleteWithResult(handle, kAppCheckErrorNone, cpp_token);
               }];
  return MakeFuture(future(), handle);
}

Future<AppCheckToken> AppCheckInternal::GetAppCheckTokenLastResult() {
  return static_cast<const Future<AppCheckToken>&>(
      future()->LastResult(kAppCheckFnGetAppCheckToken));
}

void AppCheckInternal::AddAppCheckListener(AppCheckListener* listener) {
  [notification_center_wrapper() addListener:listener];
}

void AppCheckInternal::RemoveAppCheckListener(AppCheckListener* listener) {
  [notification_center_wrapper() removeListener:listener];
}

}  // namespace internal
}  // namespace app_check
}  // namespace firebase
