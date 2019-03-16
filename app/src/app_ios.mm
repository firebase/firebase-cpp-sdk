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
// Methods for managing automatic data collection, not yet publicly exposed.
- (BOOL)isDataCollectionDefaultEnabled;
- (void)setDataCollectionDefaultEnabled:(BOOL)dataCollectionDefaultEnabled;
@end

namespace firebase {

DEFINE_FIREBASE_VERSION_STRING(Firebase);

const char* kDefaultAppName = "__FIRAPP_DEFAULT";

App::App() : data_(nullptr) {
  LogDebug("Creating firebase::App for %s", kFirebaseVersionString);
}

App::~App() {
  app_common::RemoveApp(this);
  if (data_) {
    delete static_cast<FIRAppPointer*>(data_);
    data_ = nullptr;
  }
}

App* App::Create() { return Create(AppOptions()); }

App* App::Create(const AppOptions& options) { return Create(options, kDefaultAppName); }

// Copy values of FIROptions into AppOptions.
static void FirOptionsToAppOptions(FIROptions* fir_options, AppOptions* app_options) {
  if (!strlen(app_options->app_id())) {
    const char* value = fir_options.googleAppID.UTF8String;
    if (value) app_options->set_app_id(value);
  }
  if (!strlen(app_options->api_key())) {
    const char* value = fir_options.APIKey.UTF8String;
    if (value) app_options->set_api_key(value);
  }
  if (!strlen(app_options->messaging_sender_id())) {
    const char* value = fir_options.GCMSenderID.UTF8String;
    if (value) app_options->set_messaging_sender_id(value);
  }
  if (!strlen(app_options->database_url())) {
    const char* value = fir_options.databaseURL.UTF8String;
    if (value) app_options->set_database_url(value);
  }
  if (!strlen(app_options->ga_tracking_id())) {
    const char* value = fir_options.trackingID.UTF8String;
    if (value) app_options->set_ga_tracking_id(value);
  }
  if (!strlen(app_options->storage_bucket())) {
    const char* value = fir_options.storageBucket.UTF8String;
    if (value) app_options->set_storage_bucket(value);
  }
  if (!strlen(app_options->project_id())) {
    NSString* value = fir_options.projectID;
    if (value) app_options->set_project_id(value.UTF8String);
  }
}

App* App::Create(const AppOptions& options, const char* name) {
  App* existing_app = GetInstance(name);
  if (existing_app) {
    LogError("firebase::App %s already created, options will not be applied.", name);
    return existing_app;
  }

  __block AppOptions options_in_use = options;
  __block FIRApp* fir_app;
  __block NSError* error = nil;
  bool default_app = strcmp(name, kDefaultAppName) == 0;
  void (^closure)(void) = ^{
    FIROptions* fir_options;
    @try {
      // Try to load / access default options.
      fir_options = [FIROptions defaultOptions];
    } @catch (NSException* e) {
      LogWarning("Unable to load default Firebase options, is GoogleServices-Info.plist "
                 "included in your project?");
      fir_options = nullptr;
    }
    if (fir_options) FirOptionsToAppOptions(fir_options, &options_in_use);
    FIROptions *new_fir_options = [[FIROptions alloc]
                                    initWithGoogleAppID:@(options_in_use.app_id())
                                            GCMSenderID:@(options_in_use.messaging_sender_id())];
    new_fir_options.APIKey = @(options_in_use.api_key());
    new_fir_options.clientID = fir_options ? fir_options.clientID : @"";
    new_fir_options.trackingID = @(options_in_use.ga_tracking_id());
    new_fir_options.androidClientID = fir_options ? fir_options.androidClientID : @"";
    new_fir_options.databaseURL = @(options_in_use.database_url());
    new_fir_options.deepLinkURLScheme = fir_options ? fir_options.deepLinkURLScheme : @"";
    new_fir_options.storageBucket = @(options_in_use.storage_bucket());
    new_fir_options.projectID = @(options_in_use.project_id());
    fir_options = new_fir_options;

    // Convert back to AppOptions again since it's possible for the initializer to mutate
    // some of the config values we provide.
    FirOptionsToAppOptions(fir_options, &options_in_use);

    @try {
      if (default_app) {
        FIRApp* current_default_app = nil;
        // If isDefaultAppConfigured is supported by the current version of the iOS SDK use it
        // to silently determine whether the default app has been configured otherwise fallback
        // to a query that could report an error.
        if ([[FIRApp class] respondsToSelector:@selector(isDefaultAppConfigured)]) {
          if ([FIRApp isDefaultAppConfigured]) {
            current_default_app = [FIRApp defaultApp];
          }
        } else {
          current_default_app = [FIRApp defaultApp];
          if (!current_default_app) {
            LogInfo("No default app present, ignore the previous FIRApp configuration warning "
                    "(I-COR000003).");
          }
        }
        // If a default app already exists we need to explicitly delete it then recreate it.
        if (current_default_app) {
          dispatch_semaphore_t block_semaphore = dispatch_semaphore_create(0);
          __block BOOL delete_succeeded = NO;
          LogInfo("Default app is already present, deleting the existing "
                  "default app and recreating");
          [current_default_app deleteApp:^(BOOL success) {
              delete_succeeded = success;
              dispatch_semaphore_signal(block_semaphore);
            }];
          dispatch_semaphore_wait(block_semaphore, DISPATCH_TIME_FOREVER);
          if (!delete_succeeded) {
            LogError("Failed to delete existing default app");
            fir_options = nil;
            fir_app = nil;
            return;
          }
        }
        if (fir_options) {
          [FIRApp configureWithOptions:fir_options];
        } else {
          [FIRApp configure];
        }
        fir_app = [FIRApp defaultApp];
      } else {
        [FIRApp configureWithName:@(name) options:fir_options];
        fir_app = [FIRApp appNamed:@(name)];
      }
    } @catch (NSException* e) {
      LogError("Unable to configure Firebase services: %s", [e.reason UTF8String]);
      error = [[NSError alloc] initWithDomain:e.reason code:0 userInfo:e.userInfo];
    }
  };

  // Workaround: App configuration needs to run from the main thread,
  // configure on the main thread, synchronously.
  if ([NSThread isMainThread]) {
    closure();
  } else {
    dispatch_sync(dispatch_get_main_queue(), closure);
  }

  if (error) return nullptr;

  App* new_app = new App();
  new_app->options_ = options_in_use;
  new_app->name_ = name;
  new_app->data_ = new FIRAppPointer(fir_app);
  return app_common::AddApp(new_app, default_app, &new_app->init_results_);
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
  FIRApp* app = static_cast<FIRAppPointer*>(data_)->ptr;
  if (![app respondsToSelector:@selector(setDataCollectionDefaultEnabled:)]) {
    LogError("App::SetDataCollectionDefaultEnabled() is not supported by this "
             "version of the Firebase iOS library. Please update your project's "
             "Firebase iOS dependencies to Firebase/Core 5.5.0 or higher and try "
             "again.");
    return;
  }
  [app setDataCollectionDefaultEnabled:enabled ? YES : NO];
}

bool App::IsDataCollectionDefaultEnabled() const {
  FIRApp* app = static_cast<FIRAppPointer*>(data_)->ptr;
  if (![app respondsToSelector:@selector(isDataCollectionDefaultEnabled)]) {
    // By default, if this feature isn't supported, data collection must be
    // enabled.
    return true;
  }
  return [app isDataCollectionDefaultEnabled] ? true : false;
}

}  // namespace firebase
