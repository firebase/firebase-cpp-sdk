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

#include <algorithm>
#include <atomic>
#include <memory>
#include <string>
#include <utility>

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

@interface CppAppCheckProvider : NSObject <FIRAppCheckProvider>
@property(nonatomic, nullable) firebase::app_check::AppCheckProvider* cppProvider;
@property(nonatomic, assign) std::shared_ptr<std::atomic<bool>> aliveGuard;
- (id)initWithProvider:(firebase::app_check::AppCheckProvider* _Nonnull)provider;
@end

@implementation CppAppCheckProvider
- (id)initWithProvider:(firebase::app_check::AppCheckProvider* _Nonnull)provider {
  self = [super init];
  if (self) {
    self.cppProvider = provider;
    self.aliveGuard = std::make_shared<std::atomic<bool>>(true);
  }
  return self;
}

- (void)dealloc {
  if (self.aliveGuard) self.aliveGuard->store(false);
}

- (void)dispatchTokenRequest:(BOOL)limitedUse
                  completion:(nonnull void (^)(FIRAppCheckToken* _Nullable,
                                               NSError* _Nullable))handler {
  using namespace firebase::app_check;
  if (!_cppProvider) {
    handler(nil, internal::AppCheckErrorToNSError(
                     kAppCheckErrorInvalidConfiguration,
                     "AppCheckProvider is null."));
    return;
  }
  auto alive = self.aliveGuard;
  auto cb = [handler, alive](AppCheckToken token, int err,
                             const std::string& msg) {
    if (!alive || !alive->load()) {
      handler(nil, internal::AppCheckErrorToNSError(kAppCheckErrorUnknown,
                                                    "Provider destroyed."));
      return;
    }
    if (err != kAppCheckErrorNone) {
      handler(nil, internal::AppCheckErrorToNSError(
                       static_cast<AppCheckError>(err), msg));
      return;
    }
    handler(internal::AppCheckTokenToFIRAppCheckToken(token), nil);
  };
  if (limitedUse) {
    _cppProvider->GetLimitedUseToken(cb);
  } else {
    _cppProvider->GetToken(cb);
  }
}

- (void)getTokenWithCompletion:(nonnull void (^)(FIRAppCheckToken* _Nullable,
                                                 NSError* _Nullable))handler {
  [self dispatchTokenRequest:NO completion:handler];
}

- (void)getLimitedUseTokenWithCompletion:
    (nonnull void (^)(FIRAppCheckToken* _Nullable, NSError* _Nullable))handler {
  [self dispatchTokenRequest:YES completion:handler];
}
@end

@interface CppAppCheckProviderFactory : NSObject <FIRAppCheckProviderFactory>
@property(nonatomic, nullable)
    firebase::app_check::AppCheckProviderFactory* cppProviderFactory;
- (id)initWithProviderFactory:
    (firebase::app_check::AppCheckProviderFactory* _Nonnull)factory;
@end

@implementation CppAppCheckProviderFactory
- (id)initWithProviderFactory:
    (firebase::app_check::AppCheckProviderFactory* _Nonnull)factory {
  self = [super init];
  if (self) self.cppProviderFactory = factory;
  return self;
}

- (nullable id<FIRAppCheckProvider>)createProviderWithApp:(FIRApp*)app {
  if (!_cppProviderFactory || !app) return nil;
  std::string app_name = firebase::util::NSStringToString(app.name);
  firebase::App* cpp_app =
      firebase::app_common::FindAppByName(app_name.c_str());
  if (!cpp_app) {
    cpp_app = firebase::internal::FindPartialAppByName(app_name.c_str());
  }
  if (!cpp_app) return nil;
  auto* cppProvider = _cppProviderFactory->CreateProvider(cpp_app);
  return cppProvider ? [[CppAppCheckProvider alloc] initWithProvider:cppProvider]
                     : nil;
}
@end

@implementation AppCheckNotificationCenterWrapper {
  NSMutableArray<NSValue*>* listeners_;
  NSString* app_name_;
  std::recursive_mutex listeners_mutex_;
}

- (id)init {
  return [self initWithApp:nullptr];
}

- (instancetype)initWithApp:(firebase::App* _Nullable)app {
  self = [super init];
  if (self) {
    listeners_ = [[NSMutableArray alloc] init];
    if (app && app->GetPlatformApp() != nil) {
      app_name_ = [app->GetPlatformApp().name copy];
    } else if (app && app->name()) {
      app_name_ = firebase::util::StringToNSString(app->name());
    }
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(appCheckTokenDidChangeNotification:)
               name:FIRAppCheckAppCheckTokenDidChangeNotification
             object:nil];
  }
  return self;
}

- (void)dealloc {
  [self stopListening];
}

- (void)stopListening {
  [[NSNotificationCenter defaultCenter]
      removeObserver:self
                name:FIRAppCheckAppCheckTokenDidChangeNotification
              object:nil];
  std::lock_guard<std::recursive_mutex> lock(listeners_mutex_);
  [listeners_ removeAllObjects];
}

- (void)addListener:(firebase::app_check::AppCheckListener* _Nonnull)listener {
  if (!listener) return;
  std::lock_guard<std::recursive_mutex> lock(listeners_mutex_);
  NSValue* value = [NSValue valueWithPointer:listener];
  if (![listeners_ containsObject:value]) [listeners_ addObject:value];
}

- (void)removeListener:
    (firebase::app_check::AppCheckListener* _Nonnull)listener {
  if (!listener) return;
  std::lock_guard<std::recursive_mutex> lock(listeners_mutex_);
  [listeners_ removeObject:[NSValue valueWithPointer:listener]];
}

- (void)appCheckTokenDidChangeNotification:(NSNotification*)notification {
  NSDictionary* userInfo = notification.userInfo;
  NSString* app_name = (NSString*)userInfo[kFIRAppCheckAppNameNotificationKey];
  if (app_name_ != nil &&
      (app_name == nil || ![app_name_ isEqualToString:app_name])) {
    return;
  }
  firebase::app_check::AppCheckToken cpp_token;
  cpp_token.token = firebase::util::NSStringToString(
      (NSString*)userInfo[kFIRAppCheckTokenNotificationKey]);

  NSArray<NSValue*>* snapshot;
  {
    std::lock_guard<std::recursive_mutex> lock(listeners_mutex_);
    snapshot = [listeners_ copy];
  }
  for (NSValue* listener_value in snapshot) {
    std::lock_guard<std::recursive_mutex> lock(listeners_mutex_);
    if (![listeners_ containsObject:listener_value]) continue;
    if (auto* listener = static_cast<firebase::app_check::AppCheckListener*>(
            listener_value.pointerValue)) {
      listener->OnAppCheckTokenChanged(cpp_token);
    }
  }
}
@end

namespace firebase::app_check::internal {

AppCheckInternal::AppCheckInternal(App* app)
    : lifetime_state_(std::make_shared<LifetimeState>()), app_(app) {
  lifetime_state_->owner = this;
  FIRAppCheck* fir_app_check =
      (app && app->GetPlatformApp() != nil)
          ? [FIRAppCheck appCheckWithApp:app->GetPlatformApp()]
          : [FIRAppCheck appCheck];
  impl_ = std::make_unique<FIRAppCheckPointer>(fir_app_check);
  notification_center_wrapper_ =
      std::make_unique<AppCheckNotificationCenterWrapperPointer>(
          [[AppCheckNotificationCenterWrapper alloc] initWithApp:app]);
}

AppCheckInternal::~AppCheckInternal() {
  [notification_center_wrapper() stopListening];
  std::vector<SafeFutureHandle<AppCheckToken>> pending;
  if (lifetime_state_) {
    std::lock_guard<std::mutex> lock(lifetime_state_->mutex);
    lifetime_state_->owner = nullptr;
    pending = std::move(lifetime_state_->pending_futures);
  }
  for (auto& handle : pending) {
    future()->CompleteWithResult(handle, kAppCheckErrorUnknown,
                                 "AppCheck instance destroyed.",
                                 AppCheckToken{});
  }
  app_ = nullptr;
}

void AppCheckInternal::SetAppCheckProviderFactory(
    AppCheckProviderFactory* factory) {
  [FIRAppCheck
      setAppCheckProviderFactory:factory ? [[CppAppCheckProviderFactory alloc]
                                               initWithProviderFactory:factory]
                                         : nil];
}

void AppCheckInternal::SetTokenAutoRefreshEnabled(
    bool is_token_auto_refresh_enabled) {
  if (impl() != nil) {
    impl().isTokenAutoRefreshEnabled = is_token_auto_refresh_enabled;
  }
}

Future<AppCheckToken> AppCheckInternal::FetchToken(AppCheckFn fn,
                                                   bool force_refresh) {
  __block SafeFutureHandle<AppCheckToken> handle =
      future()->SafeAlloc<AppCheckToken>(fn);
  Future<AppCheckToken> result = MakeFuture(future(), handle);
  if (impl() == nil) {
    future()->CompleteWithResult(handle, kAppCheckErrorInvalidConfiguration,
                                 "FIRAppCheck instance is nil.",
                                 AppCheckToken{});
    return result;
  }
  auto state = lifetime_state_;
  {
    std::lock_guard<std::mutex> lock(state->mutex);
    state->pending_futures.push_back(handle);
  }
  auto handler = ^(FIRAppCheckToken* _Nullable token,
                   NSError* _Nullable error) {
    int error_code = kAppCheckErrorNone;
    std::string error_message;
    AppCheckToken cpp_token;
    if (error != nil) {
      error_code = AppCheckErrorFromNSError(error);
      error_message = util::NSStringToString(error.localizedDescription);
    } else if (token == nil) {
      error_code = kAppCheckErrorUnknown;
      error_message = "AppCheck returned an empty token.";
    } else {
      cpp_token = AppCheckTokenFromFIRAppCheckToken(token);
    }
    std::lock_guard<std::mutex> lock(state->mutex);
    auto it = std::find_if(state->pending_futures.begin(),
                           state->pending_futures.end(),
                           [&](const SafeFutureHandle<AppCheckToken>& h) {
                             return h.get().id() == handle.get().id();
                           });
    if (it != state->pending_futures.end()) state->pending_futures.erase(it);
    if (state->owner) {
      state->owner->future()->CompleteWithResult(
          handle, error_code, error_message.c_str(), std::move(cpp_token));
    }
  };
  if (fn == kAppCheckFnGetAppCheckToken) {
    [impl() tokenForcingRefresh:force_refresh completion:handler];
  } else {
    [impl() limitedUseTokenWithCompletion:handler];
  }
  return result;
}

Future<AppCheckToken> AppCheckInternal::GetAppCheckToken(bool force_refresh) {
  return FetchToken(kAppCheckFnGetAppCheckToken, force_refresh);
}

Future<AppCheckToken> AppCheckInternal::GetAppCheckTokenLastResult() {
  return static_cast<const Future<AppCheckToken>&>(
      future()->LastResult(kAppCheckFnGetAppCheckToken));
}

Future<AppCheckToken> AppCheckInternal::GetLimitedUseAppCheckToken() {
  return FetchToken(kAppCheckFnGetLimitedUseAppCheckToken, false);
}

Future<AppCheckToken> AppCheckInternal::GetLimitedUseAppCheckTokenLastResult() {
  return static_cast<const Future<AppCheckToken>&>(
      future()->LastResult(kAppCheckFnGetLimitedUseAppCheckToken));
}

void AppCheckInternal::AddAppCheckListener(AppCheckListener* listener) {
  [notification_center_wrapper() addListener:listener];
}

void AppCheckInternal::RemoveAppCheckListener(AppCheckListener* listener) {
  [notification_center_wrapper() removeListener:listener];
}

}  // namespace firebase::app_check::internal
