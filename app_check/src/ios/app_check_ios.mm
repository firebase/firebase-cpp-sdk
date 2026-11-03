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
  if (self.aliveGuard) {
    self.aliveGuard->store(false);
  }
}

- (void)getTokenWithCompletion:(nonnull void (^)(FIRAppCheckToken* _Nullable,
                                                 NSError* _Nullable))handler {
  if (!_cppProvider) {
    NSError* ios_error = firebase::app_check::internal::AppCheckErrorToNSError(
        firebase::app_check::kAppCheckErrorInvalidConfiguration, "AppCheckProvider is null.");
    handler(nil, ios_error);
    return;
  }
  std::shared_ptr<std::atomic<bool>> alive = self.aliveGuard;
  auto token_callback{[handler, alive](firebase::app_check::AppCheckToken token, int error_code,
                                const std::string& error_message) {
    if (!alive || !alive->load()) {
      NSError* ios_error = firebase::app_check::internal::AppCheckErrorToNSError(
          firebase::app_check::kAppCheckErrorUnknown, "Provider destroyed.");
      handler(nil, ios_error);
      return;
    }
    if (error_code != firebase::app_check::kAppCheckErrorNone) {
      NSError* ios_error = firebase::app_check::internal::AppCheckErrorToNSError(
          static_cast<firebase::app_check::AppCheckError>(error_code), error_message);
      handler(nil, ios_error);
      return;
    }
    FIRAppCheckToken* ios_token =
        firebase::app_check::internal::AppCheckTokenToFIRAppCheckToken(token);
    handler(ios_token, nil);
  }};
  _cppProvider->GetToken(token_callback);
}

- (void)getLimitedUseTokenWithCompletion:(nonnull void (^)(FIRAppCheckToken* _Nullable,
                                                           NSError* _Nullable))handler {
  if (!_cppProvider) {
    NSError* ios_error = firebase::app_check::internal::AppCheckErrorToNSError(
        firebase::app_check::kAppCheckErrorInvalidConfiguration, "AppCheckProvider is null.");
    handler(nil, ios_error);
    return;
  }
  std::shared_ptr<std::atomic<bool>> alive = self.aliveGuard;
  auto token_callback{[handler, alive](firebase::app_check::AppCheckToken token, int error_code,
                                const std::string& error_message) {
    if (!alive || !alive->load()) {
      NSError* ios_error = firebase::app_check::internal::AppCheckErrorToNSError(
          firebase::app_check::kAppCheckErrorUnknown, "Provider destroyed.");
      handler(nil, ios_error);
      return;
    }
    if (error_code != firebase::app_check::kAppCheckErrorNone) {
      NSError* ios_error = firebase::app_check::internal::AppCheckErrorToNSError(
          static_cast<firebase::app_check::AppCheckError>(error_code), error_message);
      handler(nil, ios_error);
      return;
    }
    FIRAppCheckToken* ios_token =
        firebase::app_check::internal::AppCheckTokenToFIRAppCheckToken(token);
    handler(ios_token, nil);
  }};
  _cppProvider->GetLimitedUseToken(token_callback);
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
  if (!_cppProviderFactory || !app) {
    return nil;
  }
  std::string app_name = firebase::util::NSStringToString(app.name);
  firebase::App* cpp_app = firebase::app_common::FindAppByName(app_name.c_str());
  if (cpp_app == nullptr) {
    cpp_app = firebase::internal::FindPartialAppByName(app_name.c_str());
  }
  if (cpp_app == nullptr) {
    return nil;
  }
  firebase::app_check::AppCheckProvider* cppProvider = _cppProviderFactory->CreateProvider(cpp_app);
  if (cppProvider == nullptr) {
    return nil;
  }
  return [[CppAppCheckProvider alloc] initWithProvider:cppProvider];
}

@end

@implementation AppCheckNotificationCenterWrapper {
  // Instance variables per wrapper (Fixes BUG-0828).
  NSMutableArray<NSValue*>* listeners_;
  NSString* app_name_;
  // Recursive mutex so holding the lock across OnAppCheckTokenChanged prevents
  // concurrent removeListener:/stopListening from returning and destroying a
  // listener mid-callback while allowing same-thread re-entrant add/remove.
  std::recursive_mutex listeners_mutex_;
}

- (id)init {
  return [self initWithApp:nullptr];
}

- (instancetype)initWithApp:(firebase::App* _Nullable)app {
  self = [super init];
  if (self) {
    listeners_ = [[NSMutableArray alloc] init];
    if (app != nullptr && app->GetPlatformApp() != nil) {
      app_name_ = [app->GetPlatformApp().name copy];
    } else if (app != nullptr && app->name() != nullptr) {
      app_name_ = firebase::util::StringToNSString(app->name());
    } else {
      app_name_ = nil;
    }
    [[NSNotificationCenter defaultCenter] addObserver:self
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
  [[NSNotificationCenter defaultCenter] removeObserver:self
                                                  name:FIRAppCheckAppCheckTokenDidChangeNotification
                                                object:nil];
  std::lock_guard<std::recursive_mutex> lock(listeners_mutex_);
  [listeners_ removeAllObjects];
}

- (void)addListener:(firebase::app_check::AppCheckListener* _Nonnull)listener {
  if (!listener) return;
  std::lock_guard<std::recursive_mutex> lock(listeners_mutex_);
  NSValue* value = [NSValue valueWithPointer:listener];
  if (![listeners_ containsObject:value]) {
    [listeners_ addObject:value];
  }
}

- (void)removeListener:(firebase::app_check::AppCheckListener* _Nonnull)listener {
  if (!listener) return;
  std::lock_guard<std::recursive_mutex> lock(listeners_mutex_);
  [listeners_ removeObject:[NSValue valueWithPointer:listener]];
}

- (void)appCheckTokenDidChangeNotification:(NSNotification*)notification {
  NSDictionary* userInfo = notification.userInfo;
  NSString* app_name = (NSString*)userInfo[kFIRAppCheckAppNameNotificationKey];
  if (app_name_ != nil && (app_name == nil || ![app_name_ isEqualToString:app_name])) {
    return;
  }
  NSString* token = (NSString*)userInfo[kFIRAppCheckTokenNotificationKey];
  // TODO(b/263138261): Include expiration time in this app check token.
  // Note: The notification contains a token string, but does not currently
  // contain expiration time.
  firebase::app_check::AppCheckToken cpp_token;
  cpp_token.token = firebase::util::NSStringToString(token);

  NSArray<NSValue*>* snapshot;
  {
    std::lock_guard<std::recursive_mutex> lock(listeners_mutex_);
    snapshot = [listeners_ copy];
  }
  for (NSValue* listener_value in snapshot) {
    std::lock_guard<std::recursive_mutex> lock(listeners_mutex_);
    if (![listeners_ containsObject:listener_value]) {
      continue;
    }
    firebase::app_check::AppCheckListener* listener =
        static_cast<firebase::app_check::AppCheckListener*>(listener_value.pointerValue);
    if (listener != nullptr) {
      listener->OnAppCheckTokenChanged(cpp_token);
    }
  }
}

@end

namespace firebase {
namespace app_check {
namespace internal {

AppCheckInternal::AppCheckInternal(App* app)
    : lifetime_state_(std::make_shared<LifetimeState>()), app_(app) {
  lifetime_state_->owner = this;
  future_manager().AllocFutureApi(this, kAppCheckFnCount);
  FIRAppCheck* fir_app_check = (app != nullptr && app->GetPlatformApp() != nil)
                                   ? [FIRAppCheck appCheckWithApp:app->GetPlatformApp()]
                                   : [FIRAppCheck appCheck];
  impl_ = std::make_unique<FIRAppCheckPointer>(fir_app_check);
  notification_center_wrapper_ = std::make_unique<AppCheckNotificationCenterWrapperPointer>(
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
    future()->CompleteWithResult(handle, kAppCheckErrorUnknown, "AppCheck instance destroyed.", AppCheckToken{});
  }
  future_manager().ReleaseFutureApi(this);
  app_ = nullptr;
}

::firebase::App* AppCheckInternal::app() const { return app_; }

ReferenceCountedFutureImpl* AppCheckInternal::future() {
  return future_manager().GetFutureApi(this);
}

void AppCheckInternal::SetAppCheckProviderFactory(AppCheckProviderFactory* factory) {
  if (factory == nullptr) {
    [FIRAppCheck setAppCheckProviderFactory:nil];
    return;
  }
  CppAppCheckProviderFactory* ios_factory =
      [[CppAppCheckProviderFactory alloc] initWithProviderFactory:factory];
  [FIRAppCheck setAppCheckProviderFactory:ios_factory];
}

void AppCheckInternal::SetTokenAutoRefreshEnabled(bool is_token_auto_refresh_enabled) {
  if (impl() != nil) {
    impl().isTokenAutoRefreshEnabled = is_token_auto_refresh_enabled;
  }
}

Future<AppCheckToken> AppCheckInternal::GetAppCheckToken(bool force_refresh) {
  __block SafeFutureHandle<AppCheckToken> handle =
      future()->SafeAlloc<AppCheckToken>(kAppCheckFnGetAppCheckToken);
  Future<AppCheckToken> result = MakeFuture(future(), handle);
  if (impl() == nil) {
    future()->CompleteWithResult(handle, kAppCheckErrorInvalidConfiguration,
                                 "FIRAppCheck instance is nil.", AppCheckToken{});
    return result;
  }
  std::shared_ptr<LifetimeState> state = lifetime_state_;
  {
    std::lock_guard<std::mutex> lock(state->mutex);
    state->pending_futures.push_back(handle);
  }

  [impl()
      tokenForcingRefresh:force_refresh
               completion:^(FIRAppCheckToken* _Nullable token, NSError* _Nullable error) {
                 int error_code = kAppCheckErrorNone;
                 std::string error_message = "";
                 AppCheckToken cpp_token;
                 if (error != nil) {
                   NSLog(@"Unable to retrieve App Check token: %@", error);
                   error_code = firebase::app_check::internal::AppCheckErrorFromNSError(error);
                   error_message = util::NSStringToString(error.localizedDescription);
                 } else if (token == nil) {
                   NSLog(@"App Check token is nil.");
                   error_code = kAppCheckErrorUnknown;
                   error_message = "AppCheck GetToken returned an empty token.";
                 } else {
                   cpp_token = AppCheckTokenFromFIRAppCheckToken(token);
                 }
                 {
                   std::lock_guard<std::mutex> lock(state->mutex);
                   auto it = std::find_if(state->pending_futures.begin(), state->pending_futures.end(),
                       [&](const SafeFutureHandle<AppCheckToken>& h) { return h.get().id() == handle.get().id(); });
                   if (it != state->pending_futures.end()) {
                     state->pending_futures.erase(it);
                   }
                   if (state->owner != nullptr) {
                     state->owner->future()->CompleteWithResult(
                         handle, error_code, error_message.c_str(), std::move(cpp_token));
                   }
                 }
               }];
  return result;
}

Future<AppCheckToken> AppCheckInternal::GetAppCheckTokenLastResult() {
  return static_cast<const Future<AppCheckToken>&>(
      future()->LastResult(kAppCheckFnGetAppCheckToken));
}

Future<AppCheckToken> AppCheckInternal::GetLimitedUseAppCheckToken() {
  __block SafeFutureHandle<AppCheckToken> handle =
      future()->SafeAlloc<AppCheckToken>(kAppCheckFnGetLimitedUseAppCheckToken);
  Future<AppCheckToken> result = MakeFuture(future(), handle);
  if (impl() == nil) {
    future()->CompleteWithResult(handle, kAppCheckErrorInvalidConfiguration,
                                 "FIRAppCheck instance is nil.", AppCheckToken{});
    return result;
  }
  std::shared_ptr<LifetimeState> state = lifetime_state_;
  {
    std::lock_guard<std::mutex> lock(state->mutex);
    state->pending_futures.push_back(handle);
  }

  [impl()
      limitedUseTokenWithCompletion:^(FIRAppCheckToken* _Nullable token, NSError* _Nullable error) {
        int error_code = kAppCheckErrorNone;
        std::string error_message = "";
        AppCheckToken cpp_token;
        if (error != nil) {
          NSLog(@"Unable to retrieve limited-use App Check token: %@", error);
          error_code = firebase::app_check::internal::AppCheckErrorFromNSError(error);
          error_message = util::NSStringToString(error.localizedDescription);
        } else if (token == nil) {
          NSLog(@"App Check token is nil.");
          error_code = kAppCheckErrorUnknown;
          error_message = "AppCheck GetLimitedUseToken returned an empty token.";
        } else {
          cpp_token = AppCheckTokenFromFIRAppCheckToken(token);
        }
        {
          std::lock_guard<std::mutex> lock(state->mutex);
          auto it = std::find_if(state->pending_futures.begin(), state->pending_futures.end(),
              [&](const SafeFutureHandle<AppCheckToken>& h) { return h.get().id() == handle.get().id(); });
          if (it != state->pending_futures.end()) {
            state->pending_futures.erase(it);
          }
          if (state->owner != nullptr) {
            state->owner->future()->CompleteWithResult(handle, error_code, error_message.c_str(),
                                                       std::move(cpp_token));
          }
        }
      }];
  return result;
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

}  // namespace internal
}  // namespace app_check
}  // namespace firebase
