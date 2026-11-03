/*
 * Copyright 2016 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// Workaround for Xcode 15+ / Swift 5.10+ Concurrency and C++ verbose abort
// crash on older iOS versions (iOS 15/16).
// By providing this symbol in our own binary, we prevent dyld from
// crashing when it is missing from the system libc++ on older OSs.
#if defined(_LIBCPP_VERBOSE_ABORT_NOEXCEPT)
#define FIREBASE_LIBCPP_VERBOSE_ABORT_NOEXCEPT _LIBCPP_VERBOSE_ABORT_NOEXCEPT
#elif defined(__apple_build_version__) && __apple_build_version__ >= 16000000 && \
    __apple_build_version__ < 20000000
#define FIREBASE_LIBCPP_VERBOSE_ABORT_NOEXCEPT
#else
#define FIREBASE_LIBCPP_VERBOSE_ABORT_NOEXCEPT noexcept
#endif

#ifndef _LIBCPP_ABI_NAMESPACE
#define _LIBCPP_ABI_NAMESPACE __1
#endif

namespace std {
inline namespace _LIBCPP_ABI_NAMESPACE {
__attribute__((weak)) void __libcpp_verbose_abort(const char* format,
                                                  ...) FIREBASE_LIBCPP_VERBOSE_ABORT_NOEXCEPT {
  std::va_list list;
  va_start(list, format);
  std::vfprintf(stderr, format, list);
  va_end(list);
  std::abort();
}
}  // namespace _LIBCPP_ABI_NAMESPACE
}  // namespace std

#include "app/src/include/firebase/app.h"

#include <dispatch/dispatch.h>
#include <string>

#include "app/src/app_common.h"
#include "app/src/app_ios.h"
#include "app/src/assert.h"
#include "app/src/function_registry.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/include/firebase/version.h"
#include "app/src/log.h"
#include "app/src/util.h"
#include "app/src/util_ios.h"

#include "FIROptions.h"

@interface FIRApp ()
// Internal method (should be part of the public API *soon*) that allows registration of a library
// at the specified version which is added to the firebaseUserAgent string.
+ (void)registerLibrary:(NSString*)library withVersion:(NSString*)version;
// Use the private method firebaseUserAgent to get propagate the set of register libraries into
// the C++ SDK.
+ (NSString*)firebaseUserAgent;
@end

namespace firebase {

DEFINE_FIREBASE_VERSION_STRING(Firebase);

namespace {

// Copy values of FIROptions into AppOptions.
static void PlatformOptionsToAppOptions(FIROptions* platform_options, AppOptions* app_options) {
  if (!platform_options || !app_options) return;
  if (app_options->app_id()[0] == '\0') {
    const char* value = platform_options.googleAppID.UTF8String;
    if (value) app_options->set_app_id(value);
  }
  if (app_options->api_key()[0] == '\0') {
    const char* value = platform_options.APIKey.UTF8String;
    if (value) app_options->set_api_key(value);
  }
  if (app_options->package_name()[0] == '\0') {
    const char* value = platform_options.bundleID.UTF8String;
    if (value) app_options->set_package_name(value);
  }
  if (app_options->messaging_sender_id()[0] == '\0') {
    const char* value = platform_options.GCMSenderID.UTF8String;
    if (value) app_options->set_messaging_sender_id(value);
  }
  if (app_options->database_url()[0] == '\0') {
    const char* value = platform_options.databaseURL.UTF8String;
    if (value) app_options->set_database_url(value);
  }
  if (app_options->storage_bucket()[0] == '\0') {
    const char* value = platform_options.storageBucket.UTF8String;
    if (value) app_options->set_storage_bucket(value);
  }
  if (app_options->project_id()[0] == '\0') {
    NSString* value = platform_options.projectID;
    if (value) app_options->set_project_id(value.UTF8String);
  }
  if (app_options->client_id()[0] == '\0') {
    const char* value = platform_options.clientID.UTF8String;
    if (value) app_options->set_client_id(value);
  }
}

// Copy AppOptions into a FIROptions instance.
static FIROptions* AppOptionsToPlatformOptions(const AppOptions& app_options) {
  FIROptions* platform_options = [[FIROptions alloc] initWithGoogleAppID:@"" GCMSenderID:@""];
  if (app_options.app_id()[0] != '\0') {
    platform_options.googleAppID = @(app_options.app_id());
  }
  if (app_options.api_key()[0] != '\0') {
    platform_options.APIKey = @(app_options.api_key());
  }
  if (app_options.package_name()[0] != '\0') {
    platform_options.bundleID = @(app_options.package_name());
  }
  if (app_options.messaging_sender_id()[0] != '\0') {
    platform_options.GCMSenderID = @(app_options.messaging_sender_id());
  }
  if (app_options.database_url()[0] != '\0') {
    platform_options.databaseURL = @(app_options.database_url());
  }
  if (app_options.storage_bucket()[0] != '\0') {
    platform_options.storageBucket = @(app_options.storage_bucket());
  }
  if (app_options.project_id()[0] != '\0') {
    platform_options.projectID = @(app_options.project_id());
  }
  if (app_options.client_id()[0] != '\0') {
    platform_options.clientID = @(app_options.client_id());
  }
  return platform_options;
}

// Find an iOS SDK FIRApp instance by name.
static FIRApp* GetPlatformAppByName(const char* name) {
  if (!name) return nil;
  if ([FIRApp respondsToSelector:@selector(allApps)]) {
    NSDictionary* all_apps = FIRApp.allApps;
    if (!all_apps || !all_apps[@(name)]) {
      return nil;
    }
  }
  return app_common::IsDefaultAppName(name) ? [FIRApp defaultApp] : [FIRApp appNamed:@(name)];
}

// Settings cached by SetFirConfigurationLoggerLevel if App wasn't already initialized.
static Mutex& GetDelayedLoggerLevelMutex() {
  static Mutex m;
  return m;
}
static bool g_delayed_fir_configuration_logger_level_set = false;
static FIRLoggerLevel g_delayed_fir_configuration_logger_level = FIRLoggerLevelWarning;

// Create an iOS FIRApp instance.
static FIRApp* CreatePlatformApp(const AppOptions& options, const char* name) {
  FIRApp* platform_app = nil;
  AppOptions options_with_defaults = options;
  if (options_with_defaults.PopulateRequiredWithDefaults()) {
    FIROptions* platform_options = AppOptionsToPlatformOptions(options_with_defaults);
    // FIRApp will fail configuration if the bundle ID isn't set so fallback
    // to the main bundle ID.
    if (!platform_options.bundleID.length) {
      platform_options.bundleID = [[NSBundle mainBundle] bundleIdentifier];
    }
    if (platform_options) {
      __block FIRApp* local_platform_app = nil;
      void (^closure)(void) = ^{
        @try {
          if (app_common::IsDefaultAppName(name)) {
            [FIRApp configureWithOptions:platform_options];
          } else {
            [FIRApp configureWithName:@(name) options:platform_options];
          }
          local_platform_app = GetPlatformAppByName(name);
          // If the logger level was cached due to logging happening prior to
          // App's initialization, apply the delayed setting now, when FIRApp
          // is guaranteed to exist.
          {
            MutexLock lock(GetDelayedLoggerLevelMutex());
            if (g_delayed_fir_configuration_logger_level_set) {
              g_delayed_fir_configuration_logger_level_set = false;
              [[FIRConfiguration sharedInstance]
                  setLoggerLevel:g_delayed_fir_configuration_logger_level];
            }
          }
        } @catch (NSException* e) {
          LogError("Unable to configure Firebase app (%s)",
                   util::NSStringToString(e.reason).c_str());
          local_platform_app = nil;
        }
      };
      if ([NSThread isMainThread]) {
        closure();
      } else {
        dispatch_sync(dispatch_get_main_queue(), closure);
      }
      platform_app = local_platform_app;
    }
  }
  return platform_app;
}

// Create or get a iOS SDK FIRApp instance.
static FIRApp* CreateOrGetPlatformApp(const AppOptions& options, const char* name) {
  FIRApp* platform_app = GetPlatformAppByName(name);
  if (platform_app) {
    // If a FIRApp exists, make sure it has the requested options.
    AppOptions options_to_compare = options;
    options_to_compare.PopulateRequiredWithDefaults();
    if (options_to_compare.package_name()[0] == '\0') {
      const char* bundle_id = [[NSBundle mainBundle] bundleIdentifier].UTF8String;
      if (bundle_id) {
        options_to_compare.set_package_name(bundle_id);
      }
    }
    AppOptions existing_options;
    PlatformOptionsToAppOptions(platform_app.options, &existing_options);
    if (existing_options.package_name()[0] == '\0') {
      options_to_compare.set_package_name("");
    }
    if (options_to_compare != existing_options) {
      LogWarning("Existing instance of App %s found and options do not match "
                 "the requested options.  Deleting %s to attempt recreation "
                 "with requested options.",
                 name, name);
      dispatch_semaphore_t block_semaphore = dispatch_semaphore_create(0);
      [platform_app deleteApp:^(BOOL /*success*/) {
        // NOTE: The delete will always succeed if the app previously existed.
        dispatch_semaphore_signal(block_semaphore);
      }];
      dispatch_semaphore_wait(block_semaphore, DISPATCH_TIME_FOREVER);
      platform_app = nil;
    }
  }
  return platform_app ? platform_app : CreatePlatformApp(options, name);
}

}  // namespace

AppOptions* AppOptions::LoadDefault(AppOptions* app_options) {
  NSString* error_message = @"GoogleService-Info.plist was not found or could not be loaded";
  FIROptions* platform_options = nil;
  @try {
    platform_options = [FIROptions defaultOptions];
  } @catch (NSException* e) {
    if (e.reason.length > 0) {
      error_message = e.reason;
    }
    platform_options = nil;
  }
  if (platform_options) {
    app_options = app_options ? app_options : new AppOptions();
    PlatformOptionsToAppOptions(platform_options, app_options);
    return app_options;
  } else {
    LogError("Failed to read Firebase options from the app's resources (%s). "
             "Either make sure GoogleService-Info.plist is included in your build or specify "
             "options explicitly.",
             util::NSStringToString(error_message).c_str());
    return nullptr;
  }
}

void App::Initialize() {}

App::~App() {
  app_common::RemoveApp(this);
  delete internal_;
  internal_ = nullptr;
}

App* App::Create() {
  AppOptions options;
  return AppOptions::LoadDefault(&options) ? Create(options) : nullptr;
}

App* App::Create(const AppOptions& options) { return Create(options, kDefaultAppName); }

App* App::Create(const AppOptions& options, const char* name) {
  if (!name || name[0] == '\0') {
    LogError("App::Create: App name cannot be null or empty.");
    return nullptr;
  }
  App* app = GetInstance(name);
  if (app) {
    LogError("App %s already created, options will not be applied.", name);
    return app;
  }

  // Register C++/Unity user-agents before creating iOS app.
  app_common::RegisterSdkUsage(nullptr);

  LogDebug("Creating Firebase App %s for %s", name, kFirebaseVersionString);
  app = new App();
  app->options_ = options;
  app->name_ = name;

  // Add the app to a temporary map in case app check initialization
  // needs to reference the app before it is fully initialized.
  internal::AddPartialApp(app);
  FIRApp* platform_app = CreateOrGetPlatformApp(options, name);
  // Once the app is initialized, the entry in the temporary map is not needed.
  internal::RemovePartialApp(app);
  if (platform_app) {
    PlatformOptionsToAppOptions(platform_app.options, &app->options_);
    app->internal_ = new internal::AppInternal(platform_app);
    App* added_app = app_common::AddApp(app, &app->init_results_);
    if (added_app != app) {
      delete app;
      app = added_app;
    }
  } else {
    delete app;
    app = nullptr;
  }
  return app;
}

App* App::GetInstance() { return app_common::GetDefaultApp(); }

App* App::GetInstance(const char* name) { return app_common::FindAppByName(name); }

std::vector<App*> App::GetApps() { return app_common::GetAllApps(); }

#ifdef INTERNAL_EXPERIMENTAL
internal::FunctionRegistry* App::function_registry() {
  return app_common::GetAppFunctionRegistry(this);
}
#endif  // INTERNAL_EXPERIMENTAL

void App::RegisterLibrary(const char* library, const char* version, void* /* platform_resource */) {
  if (!library || !version) return;
  [FIRApp registerLibrary:@(library) withVersion:@(version)];
  app_common::RegisterLibrariesFromUserAgent(
      util::NSStringToString([FIRApp firebaseUserAgent]).c_str());
}

const char* App::GetUserAgent() { return app_common::GetUserAgent(); }

void App::SetDefaultConfigPath(const char* /*path*/) {}

void App::SetDataCollectionDefaultEnabled(bool enabled) {
  FIRApp* platform_app = GetPlatformApp();
  if (platform_app) {
    platform_app.dataCollectionDefaultEnabled = (enabled ? YES : NO);
  }
}

bool App::IsDataCollectionDefaultEnabled() const {
  FIRApp* platform_app = GetPlatformApp();
  return platform_app ? (platform_app.isDataCollectionDefaultEnabled != NO) : false;
}

FIRApp* App::GetPlatformApp() const {
  if (internal_) {
    return internal_->get();
  } else {
    // AppCheck initialization can depend on the FIRApp associated with an App
    // before internal_ has been created.
    return GetPlatformAppByName(name_.c_str());
  }
}

namespace internal {

void SetFirConfigurationLoggerLevel(FIRLoggerLevel level) {
  MutexLock lock(GetDelayedLoggerLevelMutex());
  // Check if a FIRApp has been created. FIRApp.allApps will return a
  // list of all FIRApps, or nil if no FIRApp has been created yet.
  if (FIRApp.allApps != nil) {
    // FIRApp has already been initialized, it's safe to set this
    // value now.
    g_delayed_fir_configuration_logger_level_set = false;
    [[FIRConfiguration sharedInstance] setLoggerLevel:level];
  } else {
    // FIRApp has not yet been initialized. Cache this value to set
    // later, in CreatePlatformApp().
    g_delayed_fir_configuration_logger_level = level;
    g_delayed_fir_configuration_logger_level_set = true;
  }
}

// Guards g_partial_apps.
static Mutex& GetPartialAppsMutex() {
  static Mutex m;
  return m;
}

// Stores partially initialized apps, indexed by app name.
static std::map<std::string, App*, std::less<>>& GetPartialAppsMap() {
  static std::map<std::string, App*, std::less<>> partial_apps;
  return partial_apps;
}

App* FindPartialAppByName(const char* name) {
  if (!name) return nullptr;
  MutexLock lock(GetPartialAppsMutex());
  auto& partial_apps = GetPartialAppsMap();
  auto it = partial_apps.find(name);
  if (it == partial_apps.end()) {
    return nullptr;
  }
  return it->second;
}

void AddPartialApp(App* app) {
  if (!app || !app->name()) return;
  MutexLock lock(GetPartialAppsMutex());
  GetPartialAppsMap().insert_or_assign(app->name(), app);
}

void RemovePartialApp(App* app) {
  if (!app || !app->name()) return;
  MutexLock lock(GetPartialAppsMutex());
  auto& partial_apps = GetPartialAppsMap();
  auto it = partial_apps.find(app->name());
  if (it != partial_apps.end() && it->second == app) {
    partial_apps.erase(it);
  }
}

}  // namespace internal
}  // namespace firebase
