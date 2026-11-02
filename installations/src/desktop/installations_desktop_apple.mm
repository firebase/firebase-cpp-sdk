// Copyright 2026 Google LLC
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

#import "installations/src/desktop/installations_desktop_apple.h"

#import <Foundation/Foundation.h>
#import <dispatch/dispatch.h>

#include <cstdlib>
#include <cstring>
#include <memory>
#include <set>
#include <string>

#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/log.h"
#include "installations/src/common.h"

// The Firebase Apple SDK is NOT a build-time dependency of
// firebase_installations on macOS desktop. All interaction with FIROptions,
// FIRApp, and FIRInstallations happens through the Objective-C runtime via
// NSClassFromString and the protocol declarations below. When a consumer
// embeds the Firebase Apple SDK (for example via
// firebase_cpp_link_apple_analytics_sdk() in
// cmake/firebase_apple_desktop_sdk.cmake), NSClassFromString resolves the
// real classes at runtime; otherwise it returns nil and this module stays
// inactive so installations_stub.cc falls back to the desktop stubs.

@protocol FIRCppDesktopInstallationsOptionsShape <NSObject>
- (instancetype)initWithGoogleAppID:(NSString*)googleAppID
                        GCMSenderID:(NSString*)GCMSenderID;
@property(nonatomic, copy, nullable) NSString* APIKey;
@property(nonatomic, copy) NSString* bundleID;
@property(nonatomic, copy, nullable) NSString* clientID;
@property(nonatomic, copy) NSString* GCMSenderID;
@property(nonatomic, copy, nullable) NSString* projectID;
@property(nonatomic, copy) NSString* googleAppID;
@property(nonatomic, copy, nullable) NSString* databaseURL;
@property(nonatomic, copy, nullable) NSString* storageBucket;
@end

@protocol FIRCppDesktopInstallationsAppShape <NSObject>
@property(nonatomic, copy, readonly) NSString* name;
@property(nonatomic, readonly) id<FIRCppDesktopInstallationsOptionsShape> options;
@end

@protocol FIRCppDesktopInstallationsAppClassShape <NSObject>
+ (void)configureWithOptions:(id<FIRCppDesktopInstallationsOptionsShape>)options;
+ (void)configureWithName:(NSString*)name
                  options:(id<FIRCppDesktopInstallationsOptionsShape>)options;
+ (nullable id<FIRCppDesktopInstallationsAppShape>)defaultApp;
+ (nullable id<FIRCppDesktopInstallationsAppShape>)appNamed:(NSString*)name;
+ (nullable NSDictionary<NSString*, id<FIRCppDesktopInstallationsAppShape>>*)allApps;
+ (BOOL)isDefaultAppConfigured;
@end

@protocol FIRCppDesktopInstallationsAuthTokenResultShape <NSObject>
@property(nonatomic, readonly) NSString* authToken;
@end

typedef void (^FIRCppDesktopInstallationsIDHandler)(
    NSString* _Nullable identifier, NSError* _Nullable error);
typedef void (^FIRCppDesktopInstallationsTokenHandler)(
    id<FIRCppDesktopInstallationsAuthTokenResultShape> _Nullable tokenResult,
    NSError* _Nullable error);

@protocol FIRCppDesktopInstallationsShape <NSObject>
- (void)installationIDWithCompletion:
    (FIRCppDesktopInstallationsIDHandler)completion;
- (void)authTokenForcingRefresh:(BOOL)forceRefresh
                     completion:
                         (FIRCppDesktopInstallationsTokenHandler)completion;
- (void)deleteWithCompletion:(void (^)(NSError* _Nullable error))completion;
@end

@protocol FIRCppDesktopInstallationsClassShape <NSObject>
+ (id<FIRCppDesktopInstallationsShape>)installations;
+ (id<FIRCppDesktopInstallationsShape>)installationsWithApp:
    (id<FIRCppDesktopInstallationsAppShape>)application;
@end

namespace firebase {
namespace installations {
namespace internal {
namespace apple {

struct InstanceState {
  Mutex mutex;
  bool alive = false;
  ReferenceCountedFutureImpl* future_impl = nullptr;
  id<FIRCppDesktopInstallationsShape> installations = nil;
};

namespace {

Mutex g_mutex;
BackendMode g_mode_override = kBackendModeAuto;
std::set<std::string>* g_configured_named_apps = nullptr;

Class<FIRCppDesktopInstallationsClassShape> InstallationsClass() {
  return (Class<FIRCppDesktopInstallationsClassShape>)NSClassFromString(
      @"FIRInstallations");
}

Class<FIRCppDesktopInstallationsAppClassShape> AppClass() {
  return (Class<FIRCppDesktopInstallationsAppClassShape>)NSClassFromString(
      @"FIRApp");
}

Class OptionsClass() { return NSClassFromString(@"FIROptions"); }

bool IsAppleSdkPresent() {
  return InstallationsClass() != Nil && AppClass() != Nil &&
         OptionsClass() != Nil;
}

NSString* SafeString(const char* c_str) {
  return c_str != nullptr ? @(c_str) : @"";
}

std::string StdString(NSString* ns_str) {
  if (ns_str == nil) return std::string();
  const char* utf8 = [ns_str UTF8String];
  return utf8 != nullptr ? std::string(utf8) : std::string();
}

BackendMode ResolveMode() {
  {
    MutexLock lock(g_mutex);
    if (g_mode_override != kBackendModeAuto) return g_mode_override;
  }
  const char* env = std::getenv(kBackendModeEnvironmentVariable);
  if (env != nullptr && env[0] != '\0') {
    return ParseBackendMode(env);
  }
  return ParseBackendMode(std::getenv(kFallbackBackendModeEnvironmentVariable));
}

bool IsDefaultAppName(const char* name) {
  return name == nullptr || name[0] == '\0' ||
         std::strcmp(name, firebase::kDefaultAppName) == 0;
}

id<FIRCppDesktopInstallationsOptionsShape> CreateOptions(
    const AppOptions& app_options) {
  id<FIRCppDesktopInstallationsOptionsShape> options =
      [[OptionsClass() alloc]
          initWithGoogleAppID:SafeString(app_options.app_id())
                  GCMSenderID:SafeString(app_options.messaging_sender_id())];
  if (std::strlen(app_options.api_key())) {
    options.APIKey = @(app_options.api_key());
  }
  if (std::strlen(app_options.project_id())) {
    options.projectID = @(app_options.project_id());
  }
  if (std::strlen(app_options.database_url())) {
    options.databaseURL = @(app_options.database_url());
  }
  if (std::strlen(app_options.storage_bucket())) {
    options.storageBucket = @(app_options.storage_bucket());
  }
  if (std::strlen(app_options.client_id())) {
    options.clientID = @(app_options.client_id());
  }
  if (std::strlen(app_options.package_name())) {
    options.bundleID = @(app_options.package_name());
  } else if ([[NSBundle mainBundle] bundleIdentifier]) {
    options.bundleID = [[NSBundle mainBundle] bundleIdentifier];
  }
  return options;
}

// Configures or retrieves the FIRApp corresponding to `app` on the main thread
// and returns the associated FIRInstallations instance, or nil if the Apple SDK
// cannot be configured for `app`.
id<FIRCppDesktopInstallationsShape> ConfigureOrGetInstallationsOnMainThread(
    const App& app) {
  const AppOptions app_options = app.options();
  const std::string app_name = app.name() ? app.name() : "";
  const bool is_default_app = IsDefaultAppName(app_name.c_str());

  __block id<FIRCppDesktopInstallationsShape> fir_installations = nil;
  void (^configure_block)(void) = ^{
    @autoreleasepool {
      Class<FIRCppDesktopInstallationsAppClassShape> app_class = AppClass();
      Class<FIRCppDesktopInstallationsClassShape> inst_class =
          InstallationsClass();
      if (app_class == Nil || inst_class == Nil) return;

      id<FIRCppDesktopInstallationsAppShape> fir_app = nil;
      bool default_configured = false;
      if ([(id)app_class respondsToSelector:@selector(isDefaultAppConfigured)]) {
        default_configured = [app_class isDefaultAppConfigured];
      } else {
        default_configured = ([app_class defaultApp] != nil);
      }

      if (is_default_app) {
        if (default_configured) {
          fir_app = [app_class defaultApp];
          std::string existing_app_id =
              fir_app ? StdString(fir_app.options.googleAppID) : std::string();
          if (!IsAppleGoogleAppId(existing_app_id.c_str())) {
            LogWarning(
                "Installations: Existing default FIRApp has non-Apple App ID "
                "'%s'; falling back to the Installations desktop stubs.",
                existing_app_id.c_str());
            return;
          }
        } else {
          if (!IsAppleGoogleAppId(app_options.app_id())) {
            LogWarning(
                "Installations: App ID '%s' is not an Apple platform App ID "
                "(1:<sender>:ios:<id>). Falling back to the Installations "
                "desktop stubs.",
                app_options.app_id());
            return;
          }
          @try {
            [app_class configureWithOptions:CreateOptions(app_options)];
            fir_app = [app_class defaultApp];
          } @catch (NSException* exception) {
            LogWarning(
                "Installations: Failed to configure default FIRApp (%s); "
                "falling back to the Installations desktop stubs.",
                [[exception reason] UTF8String]);
            return;
          }
        }
      } else {
        NSString* ns_name = SafeString(app_name.c_str());
        bool named_already_configured = false;
        {
          MutexLock lock(g_mutex);
          if (g_configured_named_apps != nullptr &&
              g_configured_named_apps->count(app_name) > 0) {
            named_already_configured = true;
          }
        }
        if (!named_already_configured && default_configured &&
            [(id)app_class respondsToSelector:@selector(allApps)]) {
          NSDictionary* all_apps = [app_class allApps];
          if (all_apps != nil && all_apps[ns_name] != nil) {
            named_already_configured = true;
          }
        }

        if (named_already_configured) {
          fir_app = [app_class appNamed:ns_name];
          std::string existing_app_id =
              fir_app ? StdString(fir_app.options.googleAppID) : std::string();
          if (!IsAppleGoogleAppId(existing_app_id.c_str())) {
            LogWarning(
                "Installations: Existing FIRApp '%s' has non-Apple App ID "
                "'%s'; falling back to the Installations desktop stubs.",
                app_name.c_str(), existing_app_id.c_str());
            return;
          }
        } else {
          if (!IsAppleGoogleAppId(app_options.app_id())) {
            LogWarning(
                "Installations: App ID '%s' is not an Apple platform App ID "
                "(1:<sender>:ios:<id>). Falling back to the Installations "
                "desktop stubs.",
                app_options.app_id());
            return;
          }
          @try {
            [app_class configureWithName:ns_name
                                 options:CreateOptions(app_options)];
            {
              MutexLock lock(g_mutex);
              if (g_configured_named_apps == nullptr) {
                g_configured_named_apps = new std::set<std::string>();
              }
              g_configured_named_apps->insert(app_name);
            }
            fir_app = [app_class appNamed:ns_name];
          } @catch (NSException* exception) {
            LogWarning(
                "Installations: Failed to configure FIRApp '%s' (%s); falling "
                "back to the Installations desktop stubs.",
                app_name.c_str(), [[exception reason] UTF8String]);
            return;
          }
        }
      }

      if (fir_app == nil) return;

      @try {
        fir_installations = [inst_class installationsWithApp:fir_app];
      } @catch (NSException* exception) {
        LogWarning(
            "Installations: Failed to obtain FIRInstallations instance (%s); "
            "falling back to the Installations desktop stubs.",
            [[exception reason] UTF8String]);
        fir_installations = nil;
      }
    }
  };

  if ([NSThread isMainThread]) {
    configure_block();
  } else {
    dispatch_sync(dispatch_get_main_queue(), configure_block);
  }
  return fir_installations;
}

}  // namespace

BackendMode ParseBackendMode(const char* value) {
  if (value == nullptr || value[0] == '\0') return kBackendModeAuto;
  NSString* mode = [@(value) lowercaseString];
  if ([mode isEqualToString:@"debug"]) return kBackendModeEnabled;
  for (NSString* off in
       @[ @"disabled", @"disable", @"0", @"off", @"false", @"no" ]) {
    if ([mode isEqualToString:off]) return kBackendModeDisabled;
  }
  for (NSString* on in
       @[ @"enabled", @"enable", @"1", @"on", @"true", @"yes" ]) {
    if ([mode isEqualToString:on]) return kBackendModeEnabled;
  }
  LogWarning("Installations: Ignoring unrecognized %s value '%s'.",
             kBackendModeEnvironmentVariable, value);
  return kBackendModeAuto;
}

bool IsAppleGoogleAppId(const char* app_id) {
  return app_id != nullptr && std::strstr(app_id, ":ios:") != nullptr;
}

void SetBackendMode(BackendMode mode) {
  MutexLock lock(g_mutex);
  g_mode_override = mode;
}

std::shared_ptr<InstanceState> CreateInstance(
    const App& app, ReferenceCountedFutureImpl* future_impl) {
  @autoreleasepool {
    const BackendMode mode = ResolveMode();
    if (mode == kBackendModeDisabled) {
      LogInfo(
          "Installations: Firebase Apple SDK backend disabled via %s; using "
          "desktop stubs.",
          kBackendModeEnvironmentVariable);
      return nullptr;
    }

    if (!IsAppleSdkPresent()) {
      if (mode == kBackendModeEnabled) {
        LogWarning(
            "Installations: %s requested the Firebase Apple SDK backend, but "
            "FIRInstallations / FIRApp are not linked into this binary. Link "
            "with firebase_cpp_link_apple_analytics_sdk(); falling back to "
            "desktop stubs.",
            kBackendModeEnvironmentVariable);
      }
      return nullptr;
    }

    id<FIRCppDesktopInstallationsShape> fir_installations =
        ConfigureOrGetInstallationsOnMainThread(app);
    if (fir_installations == nil) {
      return nullptr;
    }

    auto state = std::make_shared<InstanceState>();
    state->alive = true;
    state->future_impl = future_impl;
    state->installations = fir_installations;
    LogInfo(
        "Installations: Initialized macOS desktop backend via the Firebase "
        "Apple SDK (FIRInstallations).");
    return state;
  }
}

bool IsInstanceActive(const std::shared_ptr<InstanceState>& state) {
  if (!state) return false;
  MutexLock lock(state->mutex);
  return state->alive && state->future_impl != nullptr &&
         state->installations != nil;
}

void CleanupInstance(const std::shared_ptr<InstanceState>& state) {
  if (!state) return;
  @autoreleasepool {
    MutexLock lock(state->mutex);
    state->alive = false;
    state->future_impl = nullptr;
    state->installations = nil;
  }
}

void LogHeartbeat(const App& app) {
  @autoreleasepool {
    if (ResolveMode() == kBackendModeDisabled || !IsAppleSdkPresent()) return;
    Class<FIRCppDesktopInstallationsAppClassShape> app_class = AppClass();
    Class<FIRCppDesktopInstallationsClassShape> inst_class =
        InstallationsClass();
    if (app_class == Nil || inst_class == Nil) return;

    bool default_configured = false;
    if ([(id)app_class respondsToSelector:@selector(isDefaultAppConfigured)]) {
      default_configured = [app_class isDefaultAppConfigured];
    }
    if (!default_configured) return;

    id<FIRCppDesktopInstallationsAppShape> fir_app = nil;
    if (IsDefaultAppName(app.name())) {
      fir_app = [app_class defaultApp];
    } else {
      NSString* ns_name = SafeString(app.name());
      if ([(id)app_class respondsToSelector:@selector(allApps)]) {
        NSDictionary* all_apps = [app_class allApps];
        if (all_apps != nil && all_apps[ns_name] != nil) {
          fir_app = all_apps[ns_name];
        }
      }
    }
    if (fir_app != nil) {
      @try {
        (void)[inst_class installationsWithApp:fir_app];
      } @catch (NSException*) {
      }
    }
  }
}

void GetId(const std::shared_ptr<InstanceState>& state,
           SafeFutureHandle<std::string> handle) {
  if (!state) return;
  @autoreleasepool {
    id<FIRCppDesktopInstallationsShape> inst = nil;
    {
      MutexLock lock(state->mutex);
      if (!state->alive || state->future_impl == nullptr ||
          state->installations == nil) {
        return;
      }
      inst = state->installations;
    }

    std::shared_ptr<InstanceState> captured_state = state;
    [inst installationIDWithCompletion:^(NSString* _Nullable identifier,
                                         NSError* _Nullable error) {
      @autoreleasepool {
        MutexLock lock(captured_state->mutex);
        if (!captured_state->alive || captured_state->future_impl == nullptr) {
          return;
        }
        if (error != nil) {
          std::string err_msg = StdString([error localizedDescription]);
          captured_state->future_impl->Complete(
              handle, kInstallationsErrorFailure,
              err_msg.empty() ? "Failed to get installation ID."
                              : err_msg.c_str());
        } else {
          captured_state->future_impl->CompleteWithResult(
              handle, kInstallationsErrorNone, "", StdString(identifier));
        }
      }
    }];
  }
}

void GetToken(const std::shared_ptr<InstanceState>& state, bool force_refresh,
              SafeFutureHandle<std::string> handle) {
  if (!state) return;
  @autoreleasepool {
    id<FIRCppDesktopInstallationsShape> inst = nil;
    {
      MutexLock lock(state->mutex);
      if (!state->alive || state->future_impl == nullptr ||
          state->installations == nil) {
        return;
      }
      inst = state->installations;
    }

    std::shared_ptr<InstanceState> captured_state = state;
    [inst authTokenForcingRefresh:(force_refresh ? YES : NO)
                       completion:^(
                           id<FIRCppDesktopInstallationsAuthTokenResultShape> _Nullable token_result,
                           NSError* _Nullable error) {
                         @autoreleasepool {
                           MutexLock lock(captured_state->mutex);
                           if (!captured_state->alive ||
                               captured_state->future_impl == nullptr) {
                             return;
                           }
                           if (error != nil || token_result == nil) {
                             std::string err_msg =
                                 error ? StdString([error localizedDescription])
                                       : std::string();
                             captured_state->future_impl->Complete(
                                 handle, kInstallationsErrorFailure,
                                 err_msg.empty()
                                     ? "Failed to get installation auth token."
                                     : err_msg.c_str());
                           } else {
                             captured_state->future_impl->CompleteWithResult(
                                 handle, kInstallationsErrorNone, "",
                                 StdString([token_result authToken]));
                           }
                         }
                       }];
  }
}

void Delete(const std::shared_ptr<InstanceState>& state,
            SafeFutureHandle<void> handle) {
  if (!state) return;
  @autoreleasepool {
    id<FIRCppDesktopInstallationsShape> inst = nil;
    {
      MutexLock lock(state->mutex);
      if (!state->alive || state->future_impl == nullptr ||
          state->installations == nil) {
        return;
      }
      inst = state->installations;
    }

    std::shared_ptr<InstanceState> captured_state = state;
    [inst deleteWithCompletion:^(NSError* _Nullable error) {
      @autoreleasepool {
        MutexLock lock(captured_state->mutex);
        if (!captured_state->alive || captured_state->future_impl == nullptr) {
          return;
        }
        if (error != nil) {
          std::string err_msg = StdString([error localizedDescription]);
          captured_state->future_impl->Complete(
              handle, kInstallationsErrorFailure,
              err_msg.empty() ? "Failed to delete installation."
                              : err_msg.c_str());
        } else {
          captured_state->future_impl->Complete(handle, kInstallationsErrorNone,
                                                "");
        }
      }
    }];
  }
}

}  // namespace apple
}  // namespace internal
}  // namespace installations
}  // namespace firebase
