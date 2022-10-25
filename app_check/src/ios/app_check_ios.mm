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

#import "FIRAppCheckToken.h"
#include "app_check/src/common/common.h"

namespace firebase {
namespace app_check {
namespace internal {

// Defines an iOS AppCheckProvider that wraps a given C++ Provider.
@interface CppAppCheckProvider : NSObject <FIRAppCheckProvider>

@property(atomic, nullable) AppCheckProvider
    *cppProvider;

(id)initWithProvider:(AppCheckProvider *)provider;

@end

@implementation CppAppCheckProvider

- (id)initWithProvider:provider {
    self = [super init];
    if (self) {
        self.cppProvider = provider;
    }
    return self;
}

// TODO: Can this define a custom destructor to free the C++ provider?

- (void)getTokenWithCompletion:(nonnull void (^)(FIRAppCheckToken * _Nullable,
                                                 NSError * _Nullable))handler {
    auto token_callback{
      [&got_token_promise](AppCheckToken token,
                           int error_code, const std::string& error_message) {
        // TODO: handle error code and error_message
        // TODO: move this to a method in util_ios
        // TODO: determine correct way to get expire time, and maybe use kMillisecondsPerSecond
        NSTimeInterval exp = static_cast<NSTimeInterval>(token.expire_time_millis) / 1000.0;
        FIRAppCheckToken *token
            = [[FIRAppCheckToken alloc] initWithToken:StringToNSString(token.token)
                                       expirationDate:[NSDate dateWithTimeIntervalSince1970:exp]];

        // Pass the token or error to the completion handler.
        handler(token, nil);
      }};
    self.cppProvider->GetToken(token_callback);
}

@end

// Defines an iOS AppCheckProviderFactory that wraps a given C++ Factory.
@interface CppAppCheckProviderFactory : NSObject <FIRAppCheckProviderFactory>

@property(atomic, nullable) AppCheckProviderFactory
    *cppProviderFactory;

(id)initWithProviderFactory:(AppCheckProviderFactory *)factory;

@end

@implementation CppAppCheckProviderFactory

// TODO: Can this define a custom destructor to free the C++ factory?

- (id)initWithProviderFactory:factory {
    self = [super init];
    if (self) {
        self.cppProviderFactory = factory;
    }
    return self;
}

- (nullable id<FIRAppCheckProvider>)createProviderWithApp:(FIRApp *)app {
    // TODO: convert from iOS app to C++ app 
    AppCheckProvider* cppProvider = cppProviderFactory.CreateProvider(app);
    return [[CppAppCheckProvider alloc] initWithProvider:cppProvider];
}

@end

AppCheckInternal::AppCheckInternal(App* app) : app_(app) {
  future_manager().AllocFutureApi(this, kAppCheckFnCount);
  // TODO: see if this is right way to get impl_
  // I also saw some std:move and reset and copy constructor and so on
  // Maybe I'm supposed to call MakeUnique using nil and then reset to the value
  // Note: could call [FIRAppCheck appCheck] every time I need it, though keeping a reference is nice
  impl_ = MakeUnique<FIRAppCheckPointer>([FIRAppCheck appCheck]);
}

AppCheckInternal::~AppCheckInternal() {
  future_manager().ReleaseFutureApi(this);
  app_ = nullptr;
}

::firebase::App* AppCheckInternal::app() const { return app_; }

ReferenceCountedFutureImpl* AppCheckInternal::future() {
  return future_manager().GetFutureApi(this);
}

void AppCheckInternal::SetAppCheckProviderFactory(AppCheckProviderFactory* factory) {
  CppAppCheckProviderFactory* ios_factory =
      [[CppAppCheckProviderFactory alloc] initWithFactory:factory];
  [FIRAppCheck setAppCheckProviderFactory:ios_factory];
}

void AppCheckInternal::SetTokenAutoRefreshEnabled(bool is_token_auto_refresh_enabled) {
  impl().isTokenAutoRefreshEnabled = is_token_auto_refresh_enabled;
}

Future<AppCheckToken> AppCheckInternal::GetAppCheckToken(bool force_refresh) {
  auto handle = future()->SafeAlloc<AppCheckToken>(kAppCheckFnGetAppCheckToken);
  
  // Note: force refresh is NO in the example. Do I need to convert bool to YES/NO ?
  // TODO: Unclear whether or not cpp wrapper of builtin ios provider is needed.
  // maybe cpp providers can have an optional "get ios provider" function 
  // TODO: do I need to do anything to capture handle?
  [impl() tokenForcingRefresh:force_refresh
                              completion:^(FIRAppCheckToken * _Nullable token,
                                              NSError * _Nullable error) {
      // TODO: add token conversion logic here
      // Including converting error to future and maybe returning null or empty
      if (error != nil) {
          // Handle any errors if the token was not retrieved.
          NSLog(@"Unable to retrieve App Check token: %@", error);
          return;
      }
      if (token == nil) {
          NSLog(@"Unable to retrieve App Check token.");
          return;
      }
      // Get the raw App Check token string.
      NSString *tokenString = token.token;
      // TODO ...
      AppCheckToken token;
      future()->CompleteWithResult(handle, 0, token);
  }];
  return MakeFuture(future(), handle);
}

Future<AppCheckToken> AppCheckInternal::GetAppCheckTokenLastResult() {
  return static_cast<const Future<AppCheckToken>&>(
      future()->LastResult(kAppCheckFnGetAppCheckToken));
}

void AppCheckInternal::AddAppCheckListener(AppCheckListener* listener) {}

void AppCheckInternal::RemoveAppCheckListener(AppCheckListener* listener) {}

}  // namespace internal
}  // namespace app_check
}  // namespace firebase
