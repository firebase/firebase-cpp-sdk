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
#include <cstring>
#include "app/src/include/firebase/app.h"

#include <dispatch/dispatch.h>
#include <condition_variable>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>

#include "app/src/app_common.h"
#include "app/src/app_ios.h"
#include "app/src/app_state.h"
#include "app/src/assert.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/include/firebase/version.h"
#include "app/src/log.h"
#include "app/src/no_destructor.h"
#include "app/src/util.h"
#include "app/src/util_ios.h"

// Workaround for Xcode 15+ / Swift 5.10+ Concurrency and C++ verbose abort
// crash on older iOS versions (iOS 15/16).
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

#include "FIROptions.h"

@interface FIRApp ()
+ (BOOL)isDefaultAppConfigured;
+ (void)registerLibrary:(NSString*)library withVersion:(NSString*)version;
+ (NSString*)firebaseUserAgent;
@end

namespace firebase {

DEFINE_FIREBASE_VERSION_STRING(Firebase);

namespace {

// Per-name native FIRApp lease table and non-owning weak map for iOS ObjC
// delegates (like FIRAppCheckProviderFactory) that only receive a FIRApp*.
// Reconciles C++ multi-instance App creation with FIRApp's process-wide
// name-keyed singleton registry.
struct IosNativeLeaseTable {
  struct Slot {
    std::weak_ptr<internal::AppState> state;
    bool creating = false;
    bool active = false;
  };
  std::mutex mutex;
  std::condition_variable cv;
  std::map<std::string, Slot, std::less<>> slots;
};

IosNativeLeaseTable& GetIosNativeLeaseTable() {
  static internal::NoDestructor<IosNativeLeaseTable> table;
  return *table;
}

bool AcquireIosNativeLease(const std::string& name) {
  IosNativeLeaseTable& table = GetIosNativeLeaseTable();
  std::shared_ptr<internal::AppState> live_owner;
  std::unique_lock<std::mutex> lock(table.mutex);
  for (;;) {
    auto it = table.slots.find(name);
    if (it == table.slots.end()) {
      IosNativeLeaseTable::Slot slot;
      slot.creating = true;
      slot.active = true;
      table.slots.emplace(name, std::move(slot));
      return true;
    }
    if (it->second.creating) {
      table.cv.wait(lock);
      continue;
    }
    if (it->second.active) {
      live_owner = it->second.state.lock();
      if (live_owner) {
        LogError("An active Firebase App named '%s' already holds the iOS "
                 "native FIRApp lease. Drop existing handles or choose a "
                 "distinct App name.",
                 name.c_str());
        return false;
      }
      // Wait for dying AppState to complete DestroyPlatformData().
      table.cv.wait(lock);
      continue;
    }
    it->second.creating = true;
    it->second.active = true;
    return true;
  }
}

void PublishIosAppStateInLease(const std::string& name,
                               const std::shared_ptr<internal::AppState>& state,
                               bool finished_creating) {
  IosNativeLeaseTable& table = GetIosNativeLeaseTable();
  {
    std::lock_guard<std::mutex> lock(table.mutex);
    auto it = table.slots.find(name);
    if (it != table.slots.end()) {
      it->second.state = state;
      if (finished_creating) {
        it->second.creating = false;
      }
      it->second.active = true;
    }
  }
  if (finished_creating) {
    table.cv.notify_all();
  }
}

void ReleaseIosNativeLease(const std::string& name) {
  IosNativeLeaseTable& table = GetIosNativeLeaseTable();
  {
    std::lock_guard<std::mutex> lock(table.mutex);
    table.slots.erase(name);
  }
  table.cv.notify_all();
}

static void PlatformOptionsToAppOptions(FIROptions* platform_options, AppOptions* app_options) {
  if (!strlen(app_options->app_id())) {
    const char* value = platform_options.googleAppID.UTF8String;
    if (value) app_options->set_app_id(value);
  }
  if (!strlen(app_options->api_key())) {
    const char* value = platform_options.APIKey.UTF8String;
    if (value) app_options->set_api_key(value);
  }
  if (!strlen(app_options->package_name())) {
    const char* value = platform_options.bundleID.UTF8String;
    if (value) app_options->set_package_name(value);
  }
  if (!strlen(app_options->messaging_sender_id())) {
    const char* value = platform_options.GCMSenderID.UTF8String;
    if (value) app_options->set_messaging_sender_id(value);
  }
  if (!strlen(app_options->database_url())) {
    const char* value = platform_options.databaseURL.UTF8String;
    if (value) app_options->set_database_url(value);
  }
  if (!strlen(app_options->storage_bucket())) {
    const char* value = platform_options.storageBucket.UTF8String;
    if (value) app_options->set_storage_bucket(value);
  }
  if (!strlen(app_options->project_id())) {
    NSString* value = platform_options.projectID;
    if (value) app_options->set_project_id(value.UTF8String);
  }
  if (!strlen(app_options->client_id())) {
    const char* value = platform_options.clientID.UTF8String;
    if (value) app_options->set_client_id(value);
  }
}

static FIROptions* AppOptionsToPlatformOptions(const AppOptions& app_options) {
  FIROptions* platform_options = [[FIROptions alloc] initWithGoogleAppID:@"" GCMSenderID:@""];
  if (strlen(app_options.app_id())) {
    platform_options.googleAppID = @(app_options.app_id());
  }
  if (strlen(app_options.api_key())) {
    platform_options.APIKey = @(app_options.api_key());
  }
  if (strlen(app_options.package_name())) {
    platform_options.bundleID = @(app_options.package_name());
  }
  if (strlen(app_options.messaging_sender_id())) {
    platform_options.GCMSenderID = @(app_options.messaging_sender_id());
  }
  if (strlen(app_options.database_url())) {
    platform_options.databaseURL = @(app_options.database_url());
  }
  if (strlen(app_options.storage_bucket())) {
    platform_options.storageBucket = @(app_options.storage_bucket());
  }
  if (strlen(app_options.project_id())) {
    platform_options.projectID = @(app_options.project_id());
  }
  if (strlen(app_options.client_id())) {
    platform_options.clientID = @(app_options.client_id());
  }
  return platform_options;
}

static FIRApp* GetPlatformAppByName(const char* name) {
  LogLevel log_level = GetLogLevel();
  SetLogLevel(kLogLevelAssert);
  FIRApp* platform_app =
      app_common::IsDefaultAppName(name) ? [FIRApp defaultApp] : [FIRApp appNamed:@(name)];
  SetLogLevel(log_level);
  return platform_app;
}

static bool g_delayed_fir_configuration_logger_level_set = false;
static FIRLoggerLevel g_delayed_fir_configuration_logger_level = FIRLoggerLevelWarning;

static FIRApp* CreatePlatformApp(const AppOptions& options, const char* name) {
  __block FIRApp* platform_app = nil;
  AppOptions options_with_defaults = options;
  if (options_with_defaults.PopulateRequiredWithDefaults()) {
    FIROptions* platform_options = AppOptionsToPlatformOptions(options_with_defaults);
    if (!platform_options.bundleID.length) {
      platform_options.bundleID = [[NSBundle mainBundle] bundleIdentifier];
    }
    if (platform_options) {
      void (^closure)(void) = ^{
        @try {
          if (app_common::IsDefaultAppName(name)) {
            [FIRApp configureWithOptions:platform_options];
          } else {
            [FIRApp configureWithName:@(name) options:platform_options];
          }
          platform_app = GetPlatformAppByName(name);
          if (g_delayed_fir_configuration_logger_level_set) {
            g_delayed_fir_configuration_logger_level_set = false;
            [[FIRConfiguration sharedInstance]
                setLoggerLevel:g_delayed_fir_configuration_logger_level];
          }
        } @catch (NSException* e) {
          LogError("Unable to configure Firebase app (%s)",
                   util::NSStringToString(e.reason).c_str());
          platform_app = nil;
        }
      };
      if ([NSThread isMainThread]) {
        closure();
      } else {
        dispatch_sync(dispatch_get_main_queue(), closure);
      }
    }
  }
  return platform_app;
}

static FIRApp* CreateOrGetPlatformApp(const AppOptions& options, const char* name) {
  FIRApp* platform_app = GetPlatformAppByName(name);
  if (platform_app) {
    AppOptions existing_options;
    PlatformOptionsToAppOptions(platform_app.options, &existing_options);
    if (options != existing_options) {
      LogWarning("Existing instance of App %s found and options do not match "
                 "the requested options. Deleting %s to attempt recreation "
                 "with requested options.",
                 name, name);
      dispatch_semaphore_t block_semaphore = dispatch_semaphore_create(0);
      [platform_app deleteApp:^(BOOL /*success*/) {
        dispatch_semaphore_signal(block_semaphore);
      }];
      dispatch_semaphore_wait(block_semaphore, DISPATCH_TIME_FOREVER);
      platform_app = nil;
    }
  }
  return platform_app ? platform_app : CreatePlatformApp(options, name);
}

}  // namespace

namespace internal {

void AppState::DestroyPlatformData() {
  AppInternal* platform = platform_data_.exchange(nullptr, std::memory_order_acq_rel);
  delete platform;
  if (owns_native_lease_) {
    owns_native_lease_ = false;
    ReleaseIosNativeLease(name_);
  }
}

}  // namespace internal

AppOptions* AppOptions::LoadDefault(AppOptions* app_options) {
  NSString* error_message;
  FIROptions* platform_options;
  @try {
    platform_options = [FIROptions defaultOptions];
  } @catch (NSException* e) {
    error_message = e.reason;
    platform_options = nil;
  }
  if (platform_options) {
    app_options = app_options ? app_options : new AppOptions();
    PlatformOptionsToAppOptions(platform_options, app_options);
  } else {
    LogError("Failed to read Firebase options from the app's resources (%s). "
             "Either make sure GoogleService-Info.plist is included in your build or specify "
             "options explicitly.",
             util::NSStringToString(error_message).c_str());
  }
  return app_options;
}

App App::Create() {
  AppOptions options;
  return AppOptions::LoadDefault(&options) ? Create(options) : App();
}

App App::Create(const AppOptions& options) { return Create(options, kDefaultAppName); }

App App::Create(const AppOptions& options, std::string_view name) {
  std::string app_name(name);
  if (!AcquireIosNativeLease(app_name)) {
    return App();
  }
  struct LeaseGuard {
    const std::string& lease_name;
    bool active = true;
    ~LeaseGuard() {
      if (active) ReleaseIosNativeLease(lease_name);
    }
  } lease_guard{app_name};

  app_common::RegisterSdkUsage(nullptr);

  LogDebug("Creating Firebase App %s for %s", app_name.c_str(), kFirebaseVersionString);

  // Finalize AppOptions BEFORE constructing/publishing AppState so concurrent
  // readers (e.g., FIRAppCheckProviderFactory via FindIosAppByName) never race
  // with options mutation.
  AppOptions resolved_options = options;
  FIRApp* existing_platform_app = GetPlatformAppByName(app_name.c_str());
  if (existing_platform_app) {
    AppOptions existing_options;
    PlatformOptionsToAppOptions(existing_platform_app.options, &existing_options);
    if (options == existing_options) {
      PlatformOptionsToAppOptions(existing_platform_app.options, &resolved_options);
    }
  }
  resolved_options.PopulateRequiredWithDefaults();
  if (!strlen(resolved_options.package_name())) {
    NSString* bundle_id = [[NSBundle mainBundle] bundleIdentifier];
    if (bundle_id.length) {
      resolved_options.set_package_name(bundle_id.UTF8String);
    }
  }

  auto state = std::make_shared<internal::AppState>(app_name, std::move(resolved_options));

  // Publish weakly before [FIRApp configureWithOptions:] so FIRAppCheck
  // provider factory callbacks can retrieve this App during configuration.
  PublishIosAppStateInLease(app_name, state, /*finished_creating=*/false);
  FIRApp* platform_app = CreateOrGetPlatformApp(options, app_name.c_str());
  if (!platform_app) {
    return App();
  }

  state->set_platform_data(new internal::AppInternal(platform_app));
  state->set_owns_native_lease(true);
  lease_guard.active = false;
  PublishIosAppStateInLease(app_name, state, /*finished_creating=*/true);

  App app = state->MakeAppHandle();
  AppCallback::NotifyAllAppCreated(app, &state->mutable_init_results());
  return app;
}

App App::CreateRegistered() {
  AppOptions options;
  return AppOptions::LoadDefault(&options) ? CreateRegistered(options) : App();
}

App App::CreateRegistered(const AppOptions& options) {
  return CreateRegistered(options, kDefaultAppName);
}

App App::CreateRegistered(const AppOptions& options, std::string_view name) {
  return app_common::CreateRegisteredApp(name, [&]() { return Create(options, name); });
}

void App::RegisterLibrary(const char* library, const char* version, void* /* platform_resource */) {
  [FIRApp registerLibrary:@(library) withVersion:@(version)];
  app_common::RegisterLibrariesFromUserAgent(
      util::NSStringToString([FIRApp firebaseUserAgent]).c_str());
}

void App::SetDefaultConfigPath(const char* /* path */) {}

void App::SetDataCollectionDefaultEnabled(bool enabled) {
  FIRApp* platform_app = GetPlatformApp();
  if (platform_app) {
    platform_app.dataCollectionDefaultEnabled = (enabled ? YES : NO);
  }
}

bool App::IsDataCollectionDefaultEnabled() const {
  FIRApp* platform_app = GetPlatformApp();
  return platform_app ? (platform_app.isDataCollectionDefaultEnabled ? true : false) : true;
}

FIRApp* App::GetPlatformApp() const {
  if (!state_) return nil;
  if (state_->platform_data()) {
    return state_->platform_data()->get();
  }
  // AppCheck initialization can depend on the FIRApp associated with an App
  // before platform_data_ has been attached.
  return GetPlatformAppByName(state_->name().c_str());
}

namespace internal {

void SetFirConfigurationLoggerLevel(FIRLoggerLevel level) {
  if (FIRApp.allApps != nil) {
    [[FIRConfiguration sharedInstance] setLoggerLevel:level];
  } else {
    g_delayed_fir_configuration_logger_level = level;
    g_delayed_fir_configuration_logger_level_set = true;
  }
}

std::optional<App> FindIosAppByName(std::string_view name) {
  IosNativeLeaseTable& table = GetIosNativeLeaseTable();
  std::shared_ptr<internal::AppState> state;
  {
    std::lock_guard<std::mutex> lock(table.mutex);
    auto it = table.slots.find(name);
    if (it == table.slots.end()) return std::nullopt;
    state = it->second.state.lock();
  }
  if (!state) return std::nullopt;
  return state->MakeAppHandle();
}

}  // namespace internal
}  // namespace firebase
