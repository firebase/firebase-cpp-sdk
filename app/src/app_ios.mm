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

#include "app/src/include/firebase/app.h"

#include <dispatch/dispatch.h>
#include <string>

#include "app/src/include/firebase/version.h"
#include "app/src/app_common.h"
#include "app/src/app_ios.h"
#include "app/src/assert.h"
#include "app/src/log.h"
#include "app/src/mutex.h"
#include "app/src/util.h"
#include "app/src/util_ios.h"

#include "FIROptions.h"

@interface FIRApp ()
// Optionally use the private method isDefaultAppConfigured to determine whether a default
// application is present.  We do this as [FIRApp defaultApp] reports an error if the default
// app has not been configured.
+ (BOOL)isDefaultAppConfigured;
// Internal method (should be part of the public API *soon*) that allows registration of a library
// at the specified version which is added to the firebaseUserAgent string.
+ (void)registerLibrary:(NSString *)library
            withVersion:(NSString *)version;
// Use the private method firebaseUserAgent to get propagate the set of register libraries into
// the C++ SDK.
+ (NSString *)firebaseUserAgent;
@end

namespace firebase {

DEFINE_FIREBASE_VERSION_STRING(Firebase);

namespace {

// Copy values of FIROptions into AppOptions.
static void PlatformOptionsToAppOptions(FIROptions* platform_options,
                                        AppOptions* app_options) {
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
  if (!strlen(app_options->ga_tracking_id())) {
    const char* value = platform_options.trackingID.UTF8String;
    if (value) app_options->set_ga_tracking_id(value);
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

// Copy AppOptions into a FIROptions instance.
static FIROptions* AppOptionsToPlatformOptions(const AppOptions& app_options) {
  FIROptions* platform_options = [[FIROptions alloc] initWithGoogleAppID:@""
                                                        GCMSenderID:@""];
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
  if (strlen(app_options.ga_tracking_id())) {
    platform_options.trackingID = @(app_options.ga_tracking_id());
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

// Find an iOS SDK FIRApp instance by name.
static FIRApp* GetPlatformAppByName(const char* name) {
  // Silence iOS SDKs warnings about FIRApp instances being missing.
  LogLevel log_level = GetLogLevel();
  SetLogLevel(kLogLevelAssert);
  FIRApp *platform_app = app_common::IsDefaultAppName(name) ?
                         [FIRApp defaultApp] : [FIRApp appNamed:@(name)];
  SetLogLevel(log_level);
  return platform_app;
}

// Create an iOS FIRApp instance.
static FIRApp* CreatePlatformApp(const AppOptions& options, const char* name) {
  __block FIRApp* platform_app = nil;
  AppOptions options_with_defaults = options;
  if (options_with_defaults.PopulateRequiredWithDefaults()) {
    FIROptions* platform_options = AppOptionsToPlatformOptions(options_with_defaults);
    // FIRApp will fail configuration if the bundle ID isn't set so fallback
    // to the main bundle ID.
    if (!platform_options.bundleID.length) {
      platform_options.bundleID = [[NSBundle mainBundle] bundleIdentifier];
    }
    if (platform_options) {
      // TODO: Re-evaluate this workaround
      // Workaround: App configuration needs to run from the main thread.
      void (^closure)(void) = ^{
        @try {
          if (app_common::IsDefaultAppName(name)) {
            [FIRApp configureWithOptions:platform_options];
          } else {
            [FIRApp configureWithName:@(name) options:platform_options];
          }
          platform_app = GetPlatformAppByName(name);
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

// Create or get a iOS SDK FIRApp instance.
static FIRApp* CreateOrGetPlatformApp(const AppOptions& options,
                                      const char* name) {
  FIRApp* platform_app = GetPlatformAppByName(name);
  if (platform_app) {
    // If a FIRApp exists, make sure it has the requested options.
    AppOptions existing_options;
    PlatformOptionsToAppOptions(platform_app.options, &existing_options);
    if (options != existing_options) {
      LogWarning("Existing instance of App %s found and options do not match "
                 "the requested options.  Deleting %s to attempt recreation "
                 "with requested options.", name, name);
      dispatch_semaphore_t block_semaphore = dispatch_semaphore_create(0);
      [platform_app deleteApp:^(BOOL /*success*/) {
          // NOTE: The delete will always succeed if the app previously existed.
          dispatch_semaphore_signal(block_semaphore);
        }];
      dispatch_semaphore_wait(block_semaphore, DISPATCH_TIME_FOREVER);
      platform_app = nil;
    }
  }
  return platform_app ? platform_app : CreatePlatformApp(options, name);;
}

}  // namespace

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
             "options explicitly.", util::NSStringToString(error_message).c_str());
  }
  return app_options;
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
  App* app = GetInstance(name);
  if (app) {
    LogError("App %s already created, options will not be applied.", name);
    return app;
  }
  LogDebug("Creating Firebase App %s for %s", name, kFirebaseVersionString);
  FIRApp* platform_app = CreateOrGetPlatformApp(options, name);
  if (platform_app) {
    app = new App();
    app->options_ = options;
    app->name_ = name;
    app->internal_ = new internal::AppInternal(platform_app);
    app_common::AddApp(app, &app->init_results_);
  }
  return app;
}

App* App::GetInstance() { return app_common::GetDefaultApp(); }

App* App::GetInstance(const char* name) { return app_common::FindAppByName(name); }

void App::RegisterLibrary(const char* library, const char* version) {
  [FIRApp registerLibrary:@(library) withVersion:@(version)];
  app_common::RegisterLibrariesFromUserAgent(
      util::NSStringToString([FIRApp firebaseUserAgent]).c_str());
}

const char* App::GetUserAgent() {
  return app_common::GetUserAgent();
}

void App::SetDefaultConfigPath(const char* path) { }

void App::SetDataCollectionDefaultEnabled(bool enabled) {
  GetPlatformApp().dataCollectionDefaultEnabled = (enabled ? YES : NO);
}

bool App::IsDataCollectionDefaultEnabled() const {
  return GetPlatformApp().isDataCollectionDefaultEnabled ? true : false;
}

FIRApp* App::GetPlatformApp() const {
  return internal_->get();
}

}  // namespace firebase
