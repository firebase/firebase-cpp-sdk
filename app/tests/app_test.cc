/*
 * Copyright 2017 Google LLC
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

#include "absl/flags/flag.h"
#if defined(FIREBASE_ANDROID_FOR_DESKTOP)
#ifndef __ANDROID__
#define __ANDROID__
#endif  // __ANDROID__
#include <jni.h>
#include "testing/run_all_tests.h"
#endif  // defined(FIREBASE_ANDROID_FOR_DESKTOP)

#include "app/src/app_common.h"
#include "app/src/app_identifier.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/version.h"
#include "app/src/include/firebase/internal/platform.h"

#if defined(FIREBASE_ANDROID_FOR_DESKTOP)
#undef __ANDROID__
#endif  // defined(FIREBASE_ANDROID_FOR_DESKTOP)

#include <stdio.h>
#include <cstring>
#include <memory>
#include <string>

#include "flatbuffers/util.h"

#if defined(_WIN32)
#include <direct.h>
#define getcwd _getcwd
#define chdir _chdir
#else
#include <unistd.h>
#endif  // defined(_WIN32)

#include "testing/config.h"
#include "testing/ticker.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"

#ifdef __APPLE__
#include "TargetConditionals.h"
#endif  // __APPLE__

#if FIREBASE_PLATFORM_IOS
// Declared in the Obj-C header fake/FIRApp.h.
extern "C" {
void FIRAppCreateUsingDefaultOptions(const char* name);
void FIRAppResetApps();
}
#endif  // FIREBASE_PLATFORM_IOS

// FLAGS_test_srcdir is not defined on Android and iOS so we can't read
// from test resources.
#if ((defined(__APPLE__) && TARGET_OS_IOS) || defined(__ANDROID__) || \
     defined(FIREBASE_ANDROID_FOR_DESKTOP))
#define TEST_RESOURCES_AVAILABLE 0
#else
#define TEST_RESOURCES_AVAILABLE 1
#endif  // MOBILE

using testing::ContainsRegex;
using testing::HasSubstr;
using testing::Not;

namespace firebase {

class AppTest : public ::testing::Test {
 protected:
  AppTest() : current_path_buffer_(nullptr) {
#if TEST_RESOURCES_AVAILABLE
    test_data_dir_ = absl::GetFlag(FLAGS_test_srcdir) +
                     "/google3/firebase/app/client/cpp/testdata";
    broken_test_data_dir_ = test_data_dir_ + "/broken";
#endif  // TEST_RESOURCES_AVAILABLE
  }

  void SetUp() override {
    SaveCurrentDirectory();
#if TEST_RESOURCES_AVAILABLE
    EXPECT_EQ(chdir(test_data_dir_.c_str()), 0);
#endif  // TEST_RESOURCES_AVAILABLE
  }

  void TearDown() override {
    RestoreCurrentDirectory();
    ClearAppInstances();
  }

  // Create a mobile app instance using the fake options from resources.
  void CreateMobileApp(const char* name) {
#if FIREBASE_PLATFORM_IOS
    FIRAppCreateUsingDefaultOptions(name ? name : "__FIRAPP_DEFAULT");
#endif  // FIREBASE_PLATFORM_IOS
#if FIREBASE_ANDROID_FOR_DESKTOP
    JNIEnv *env = firebase::testing::cppsdk::GetTestJniEnv();
    jclass firebase_app_class =
        env->FindClass("com/google/firebase/FirebaseApp");
    env->ExceptionCheck();
    jclass firebase_options_class =
        env->FindClass("com/google/firebase/FirebaseOptions");
    env->ExceptionCheck();
    jobject options = env->CallStaticObjectMethod(
        firebase_options_class,
        env->GetStaticMethodID(
            firebase_options_class, "fromResource",
            "(Landroid/content/Context;)"
            "Lcom/google/firebase/FirebaseOptions;"),
        firebase::testing::cppsdk::GetTestActivity());
    env->ExceptionCheck();
    jobject app_name = env->NewStringUTF(name ? name : "[DEFAULT]");
    jobject app = env->CallStaticObjectMethod(
        firebase_app_class,
        env->GetStaticMethodID(
            firebase_app_class, "initializeApp",
            "(Landroid/content/Context;"
            "Lcom/google/firebase/FirebaseOptions;"
            "Ljava/lang/String;)Lcom/google/firebase/FirebaseApp;"),
        firebase::testing::cppsdk::GetTestActivity(),
        options,
        app_name);
    env->ExceptionCheck();
    env->DeleteLocalRef(app);
    env->DeleteLocalRef(app_name);
    env->DeleteLocalRef(options);
    env->DeleteLocalRef(firebase_options_class);
    env->DeleteLocalRef(firebase_app_class);
#endif  // FIREBASE_ANDROID_FOR_DESKTOP
  }

 private:
  // Clear all C++ firebase::App objects and any mobile SDK instances.
  void ClearAppInstances() {
    app_common::DestroyAllApps();
#if FIREBASE_PLATFORM_IOS
    FIRAppResetApps();
#endif  // FIREBASE_PLATFORM_IOS
#if FIREBASE_ANDROID_FOR_DESKTOP
    JNIEnv *env = firebase::testing::cppsdk::GetTestJniEnv();
    jclass firebase_app_class =
        env->FindClass("com/google/firebase/FirebaseApp");
    env->ExceptionCheck();
    env->CallStaticVoidMethod(
        firebase_app_class,
        env->GetStaticMethodID(firebase_app_class, "reset", "()V"));
    env->ExceptionCheck();
    env->DeleteLocalRef(firebase_app_class);
#endif  // FIREBASE_ANDROID_FOR_DESKTOP
  }

  void SaveCurrentDirectory() {
    assert(current_path_buffer_ == nullptr);
    current_path_buffer_ = new char[FILENAME_MAX];
    getcwd(current_path_buffer_, FILENAME_MAX);
  }

  void RestoreCurrentDirectory() {
    assert(current_path_buffer_ != nullptr);
    EXPECT_EQ(chdir(current_path_buffer_), 0);
    delete[] current_path_buffer_;
    current_path_buffer_ = nullptr;
  }

 protected:
  char* current_path_buffer_;
  std::string test_data_dir_;
  std::string broken_test_data_dir_;
};

// The following few tests are testing the setter and getter of AppOptions.

TEST_F(AppTest, TestSetAppId) {
  AppOptions options;
  options.set_app_id("abc");
  EXPECT_STREQ("abc", options.app_id());
}

TEST_F(AppTest, TestSetApiKey) {
  AppOptions options;
  options.set_api_key("AIzaSyDdVgKwhZl0sTTTLZ7iTmt1r3N2cJLnaDk");
  EXPECT_STREQ("AIzaSyDdVgKwhZl0sTTTLZ7iTmt1r3N2cJLnaDk", options.api_key());
}

TEST_F(AppTest, TestSetMessagingSenderId) {
  AppOptions options;
  options.set_messaging_sender_id("012345678901");
  EXPECT_STREQ("012345678901", options.messaging_sender_id());
}

TEST_F(AppTest, TestSetDatabaseUrl) {
  AppOptions options;
  options.set_database_url("http://abc-xyz-123.firebaseio.com");
  EXPECT_STREQ("http://abc-xyz-123.firebaseio.com", options.database_url());
}

TEST_F(AppTest, TestSetGaTrackingId) {
  AppOptions options;
  options.set_ga_tracking_id("UA-12345678-1");
  EXPECT_STREQ("UA-12345678-1", options.ga_tracking_id());
}

TEST_F(AppTest, TestSetStorageBucket) {
  AppOptions options;
  options.set_storage_bucket("abc-xyz-123.storage.firebase.com");
  EXPECT_STREQ("abc-xyz-123.storage.firebase.com", options.storage_bucket());
}

TEST_F(AppTest, TestSetProjectId) {
  AppOptions options;
  options.set_project_id("myproject-123");
  EXPECT_STREQ("myproject-123", options.project_id());
}

TEST_F(AppTest, LoadDefault) {
  AppOptions options;
  EXPECT_EQ(&options,
            AppOptions::LoadDefault(
                &options
#if defined(FIREBASE_ANDROID_FOR_DESKTOP)
                // Additional parameters are required for Android.
                ,
                firebase::testing::cppsdk::GetTestJniEnv(),
                firebase::testing::cppsdk::GetTestActivity()
#endif  // defined(FIREBASE_ANDROID_FOR_DESKTOP)
                                    ));
  EXPECT_STREQ("fake app id from resource", options.app_id());
  EXPECT_STREQ("fake api key from resource", options.api_key());
  EXPECT_STREQ("fake messaging sender id from resource",
               options.messaging_sender_id());
  EXPECT_STREQ("fake database url from resource", options.database_url());
#if FIREBASE_PLATFORM_IOS
  // GA tracking ID can currently only be configured on iOS.
  EXPECT_STREQ("fake ga tracking id from resource", options.ga_tracking_id());
#endif  // FIREBASE_PLATFORM_IOS
  EXPECT_STREQ("fake storage bucket from resource", options.storage_bucket());
  EXPECT_STREQ("fake project id from resource", options.project_id());
#if !FIREBASE_PLATFORM_IOS
  // The application bundle ID isn't available in iOS tests.
  EXPECT_STRNE("", options.package_name());
#endif  // !FIREBASE_PLATFORM_IOS
}

TEST_F(AppTest, PopulateRequiredWithDefaults) {
  AppOptions options;
  EXPECT_STREQ("", options.app_id());
  EXPECT_STREQ("", options.api_key());
  EXPECT_STREQ("", options.project_id());
  EXPECT_TRUE(
      options.PopulateRequiredWithDefaults(
#if defined(FIREBASE_ANDROID_FOR_DESKTOP)
          // Additional parameters are required for Android.
          firebase::testing::cppsdk::GetTestJniEnv(),
          firebase::testing::cppsdk::GetTestActivity()
#endif  // defined(FIREBASE_ANDROID_FOR_DESKTOP)
                                           ));
  EXPECT_STREQ("fake app id from resource", options.app_id());
  EXPECT_STREQ("fake api key from resource", options.api_key());
  EXPECT_STREQ("fake project id from resource", options.project_id());
}

// The following tests create Firebase App instances.

// Helper functions to create test instance.
std::unique_ptr<App> CreateFirebaseApp() {
  return std::unique_ptr<App>(App::Create(
#if defined(FIREBASE_ANDROID_FOR_DESKTOP)
      // Additional parameters are required for Android.
      firebase::testing::cppsdk::GetTestJniEnv(),
      firebase::testing::cppsdk::GetTestActivity()
#endif  // defined(FIREBASE_ANDROID_FOR_DESKTOP)
          ));
}

std::unique_ptr<App> CreateFirebaseApp(const char* name) {
  return std::unique_ptr<App>(App::Create(
      AppOptions(), name
#if defined(FIREBASE_ANDROID_FOR_DESKTOP)
      // Additional parameters are required for Android.
      ,
      firebase::testing::cppsdk::GetTestJniEnv(),
      firebase::testing::cppsdk::GetTestActivity()
#endif  // defined(FIREBASE_ANDROID_FOR_DESKTOP)
          ));
}

std::unique_ptr<App> CreateFirebaseApp(const AppOptions& options) {
  return std::unique_ptr<App>(App::Create(
      options
#if defined(FIREBASE_ANDROID_FOR_DESKTOP)
      // Additional parameters are required for Android.
      ,
      firebase::testing::cppsdk::GetTestJniEnv(),
      firebase::testing::cppsdk::GetTestActivity()
#endif  // defined(FIREBASE_ANDROID_FOR_DESKTOP)
          ));
}

std::unique_ptr<App> CreateFirebaseApp(const AppOptions& options,
                                       const char* name) {
  return std::unique_ptr<App>(App::Create(
      options,
      name
#if defined(FIREBASE_ANDROID_FOR_DESKTOP)
      // Additional parameters are required for Android.
      ,
      firebase::testing::cppsdk::GetTestJniEnv(),
      firebase::testing::cppsdk::GetTestActivity()
#endif  // defined(FIREBASE_ANDROID_FOR_DESKTOP)
          ));
}

TEST_F(AppTest, TestCreateDefault) {
  // Created with default options.
  std::unique_ptr<App> firebase_app = CreateFirebaseApp();
  EXPECT_NE(nullptr, firebase_app);
  EXPECT_STREQ(firebase::kDefaultAppName, firebase_app->name());
}

TEST_F(AppTest, TestCreateDefaultWithExistingApp) {
#if FIREBASE_PLATFORM_MOBILE
  // Create a default mobile app that will be proxied by a C++ app object.
  CreateMobileApp(nullptr);
  // Create the C++ proxy object, since we've specified no options this should
  // return a proxy to the previously created object.
  std::unique_ptr<App> firebase_app = CreateFirebaseApp();
  EXPECT_NE(nullptr, firebase_app);
  EXPECT_STREQ(firebase::kDefaultAppName, firebase_app->name());
  // Make sure the options loaded from the fake resource are present.
  EXPECT_STREQ("fake project id from resource",
               firebase_app->options().project_id());
#endif  // FIREBASE_PLATFORM_MOBILE
}

TEST_F(AppTest, TestCreateNamedWithExistingApp) {
#if FIREBASE_PLATFORM_MOBILE
  // Create a default mobile app that will be proxied by a C++ app object.
  CreateMobileApp("a named app");
  // Create the C++ proxy object, since we've specified no options this should
  // return a proxy to the previously created object.
  std::unique_ptr<App> firebase_app = CreateFirebaseApp("a named app");
  EXPECT_NE(nullptr, firebase_app);
  EXPECT_STREQ("a named app", firebase_app->name());
#endif  // FIREBASE_PLATFORM_MOBILE
}

TEST_F(AppTest, TestCreateWithOptions) {
  // Created with options as well as name.
  std::unique_ptr<App> firebase_app = CreateFirebaseApp("my_apps_name");
  EXPECT_NE(nullptr, firebase_app);
  EXPECT_STREQ("my_apps_name", firebase_app->name());
}

TEST_F(AppTest, TestCreateDefaultWithDifferentOptionsToExistingApp) {
#if FIREBASE_PLATFORM_MOBILE
  // Create a default mobile app that will be proxied by a C++ app object.
  CreateMobileApp(nullptr);
  // Create the C++ proxy object, this should delete the previously created
  // object returning a new object with the specified options.
  AppOptions options;
  options.set_api_key("an api key");
  options.set_app_id("a different app id");
  options.set_project_id("a project id");
  std::unique_ptr<App> firebase_app = CreateFirebaseApp(options);
  EXPECT_NE(nullptr, firebase_app);
  EXPECT_STREQ("__FIRAPP_DEFAULT", firebase_app->name());
  EXPECT_STREQ("an api key", firebase_app->options().api_key());
  EXPECT_STREQ("a different app id", firebase_app->options().app_id());
  EXPECT_STREQ("a project id", firebase_app->options().project_id());
#endif  // FIREBASE_PLATFORM_MOBILE
}

TEST_F(AppTest, TestCreateNamedWithDifferentOptionsToExistingApp) {
#if FIREBASE_PLATFORM_MOBILE
  // Create a default mobile app that will be proxied by a C++ app object.
  CreateMobileApp("a named app");
  // Create the C++ proxy object, this should delete the previously created
  // object returning a new object with the specified options.
  AppOptions options;
  options.set_api_key("an api key");
  options.set_app_id("a different app id");
  std::unique_ptr<App> firebase_app = CreateFirebaseApp(
      options, "a named app");
  EXPECT_NE(nullptr, firebase_app);
  EXPECT_STREQ("a named app", firebase_app->name());
  EXPECT_STREQ("a different app id", firebase_app->options().app_id());
  EXPECT_STREQ("an api key", firebase_app->options().api_key());
#endif  // FIREBASE_PLATFORM_MOBILE
}

TEST_F(AppTest, TestCreateMultipleTimes) {
  // Created two apps with the same default name; the two are actually the same.
  // We cannot use unique_ptr for this as the two will point to the same app.
  App* firebase_app[2];
  for (int i = 0; i < 2; ++i) {
    firebase_app[i] = App::Create(
#if defined(FIREBASE_ANDROID_FOR_DESKTOP)
        // Additional parameters are required for Android.
        firebase::testing::cppsdk::GetTestJniEnv(),
        firebase::testing::cppsdk::GetTestActivity()
#endif  // defined(FIREBASE_ANDROID_FOR_DESKTOP)
    );
  }
  // There is only one app with the same name.
  EXPECT_NE(nullptr, firebase_app[0]);
  EXPECT_EQ(firebase_app[0], firebase_app[1]);
  delete firebase_app[0];
}

// The following tests call GetInstance().

TEST_F(AppTest, TestGetDefaultInstance) {
  // Nothing is created yet. We get nullptr.
  EXPECT_EQ(nullptr, App::GetInstance());

  // Now we create one.
  std::unique_ptr<App> firebase_app = CreateFirebaseApp();
  // We should get a non-nullptr pointer, which is what we created above.
  EXPECT_NE(nullptr, App::GetInstance());
  EXPECT_EQ(firebase_app.get(), App::GetInstance());

  // But there is one app for each distinct name.
  EXPECT_EQ(nullptr, App::GetInstance("thing_one"));
  EXPECT_EQ(nullptr, App::GetInstance("thing_two"));
}

TEST_F(AppTest, TestGetInstanceMultipleApps) {
  // Nothing is created yet.
  EXPECT_EQ(nullptr, App::GetInstance());
  EXPECT_EQ(nullptr, App::GetInstance("thing_one"));
  EXPECT_EQ(nullptr, App::GetInstance("thing_two"));

  // Now we create named app.
  std::unique_ptr<App> firebase_app = CreateFirebaseApp("thing_one");
  EXPECT_EQ(nullptr, App::GetInstance());
  EXPECT_NE(nullptr, App::GetInstance("thing_one"));
  EXPECT_EQ(firebase_app.get(), App::GetInstance("thing_one"));
  EXPECT_EQ(nullptr, App::GetInstance("thing_two"));

  // We again create a default app.
  std::unique_ptr<App> firebase_app_default = CreateFirebaseApp();
  EXPECT_NE(nullptr, App::GetInstance());
  EXPECT_EQ(firebase_app_default.get(), App::GetInstance());
  EXPECT_NE(nullptr, App::GetInstance("thing_one"));
  EXPECT_EQ(firebase_app.get(), App::GetInstance("thing_one"));
  EXPECT_NE(firebase_app, firebase_app_default);
  EXPECT_EQ(nullptr, App::GetInstance("thing_two"));
}

TEST_F(AppTest, TestParseUserAgent) {
  app_common::RegisterLibrariesFromUserAgent("test/1 check/2 check/3");
  EXPECT_EQ(std::string(app_common::GetUserAgent()),
            std::string("check/3 test/1"));
}

TEST_F(AppTest, TestRegisterAndGetLibraryVersion) {
  app_common::RegisterLibrary("a_library", "3.4.5");
  EXPECT_EQ("3.4.5", app_common::GetLibraryVersion("a_library"));
  EXPECT_EQ("", app_common::GetLibraryVersion("a_non_existent_library"));
}

TEST_F(AppTest, TestGetOuterMostSdkAndVersion) {
  std::unique_ptr<App> firebase_app_default = CreateFirebaseApp();
  std::string sdk;
  std::string version;
  app_common::GetOuterMostSdkAndVersion(&sdk, &version);
  EXPECT_EQ(sdk, "fire-cpp");
  EXPECT_EQ(version, FIREBASE_VERSION_NUMBER_STRING);
  app_common::RegisterLibrary("fire-mono", "4.5.6");
  app_common::GetOuterMostSdkAndVersion(&sdk, &version);
  EXPECT_EQ(sdk, "fire-mono");
  EXPECT_EQ(version, "4.5.6");
  app_common::RegisterLibrary("fire-unity", "3.2.1");
  app_common::GetOuterMostSdkAndVersion(&sdk, &version);
  EXPECT_EQ(sdk, "fire-unity");
  EXPECT_EQ(version, "3.2.1");
}

TEST_F(AppTest, TestRegisterLibrary) {
  std::string firebase_version(std::string("fire-cpp/") +
                               std::string(FIREBASE_VERSION_NUMBER_STRING));
  std::unique_ptr<App> firebase_app_default = CreateFirebaseApp();
  EXPECT_THAT(std::string(App::GetUserAgent()), HasSubstr(firebase_version));
  EXPECT_THAT(std::string(App::GetUserAgent()),
              ContainsRegex("fire-cpp-os/(windows|darwin|linux|ios|android)"));
  EXPECT_THAT(std::string(App::GetUserAgent()),
              ContainsRegex("fire-cpp-arch/[^ ]+"));
  EXPECT_THAT(std::string(App::GetUserAgent()),
              ContainsRegex("fire-cpp-stl/[^ ]+"));
  App::RegisterLibrary("fire-testing", "1.2.3");
  EXPECT_THAT(std::string(App::GetUserAgent()),
              HasSubstr("fire-testing/1.2.3"));
  firebase_app_default.reset(nullptr);
  EXPECT_THAT(std::string(App::GetUserAgent()),
              Not(HasSubstr("fire-testing/1.2.3")));
}

#if TEST_RESOURCES_AVAILABLE
TEST_F(AppTest, TestDefaultOptions) {
  std::unique_ptr<App> firebase_app = CreateFirebaseApp(AppOptions());

  const AppOptions& options = firebase_app->options();
  EXPECT_STREQ("fake app id from resource", options.app_id());
  EXPECT_STREQ("fake api key from resource", options.api_key());
  EXPECT_STREQ("", options.messaging_sender_id());
  EXPECT_STREQ("", options.database_url());
  EXPECT_STREQ("", options.ga_tracking_id());
  EXPECT_STREQ("", options.storage_bucket());
  EXPECT_STREQ("fake project id from resource", options.project_id());
}

TEST_F(AppTest, TestReadOptionsFromResource) {
  AppOptions app_options;
  std::string json_file = test_data_dir_ + "/google-services.json";
  std::string config;
  EXPECT_TRUE(flatbuffers::LoadFile(json_file.c_str(), false, &config));
  AppOptions::LoadFromJsonConfig(config.c_str(), &app_options);
  std::unique_ptr<App> firebase_app = CreateFirebaseApp(app_options);

  const AppOptions& options = firebase_app->options();
  // Check for the various fake options.
  EXPECT_STREQ("fake mobilesdk app id", options.app_id());
  EXPECT_STREQ("fake api key", options.api_key());
  EXPECT_STREQ("fake project number", options.messaging_sender_id());
  EXPECT_STREQ("fake firebase url", options.database_url());
  // None of Firebase sample apps contain GA tracking_id. Looks like the field
  // is either deprecated or not important.
  EXPECT_STREQ("", options.ga_tracking_id());
  // Firebase auth sample app does not contain storage_bucket field. This could
  // change and we should update here accordingly.
  EXPECT_STREQ("", options.storage_bucket());
  EXPECT_STREQ("fake project id", options.project_id());
}

// Test that calling app.create() with no options tries to load from the local
// file google-services-desktop.json, before giving up.
TEST_F(AppTest, TestDefaultStart) {
  // With no arguments, this will attempt to load a config from a file.
  auto app = std::unique_ptr<App>(App::Create());
  const AppOptions& options = app->options();
  EXPECT_STREQ(options.api_key(), "fake api key from resource");
  EXPECT_STREQ(options.storage_bucket(), "fake storage bucket from resource");
  EXPECT_STREQ(options.project_id(), "fake project id from resource");
  EXPECT_STREQ(options.database_url(), "fake database url from resource");
  EXPECT_STREQ(options.messaging_sender_id(),
               "fake messaging sender id from resource");
}

TEST_F(AppTest, TestDefaultStartBrokenOptions) {
  // Need to change the directory here to make sure we are in the same place
  // as the broken google-services-desktop.json file.
  EXPECT_EQ(chdir(broken_test_data_dir_.c_str()), 0);
  // With no arguments, this will attempt to load a config from a file.
  // This should fail as the file's format is invalid.
  auto app = std::unique_ptr<App>(App::Create());
  EXPECT_EQ(app.get(), nullptr);
}

TEST_F(AppTest, TestCreateIdentifierFromOptions) {
  {
    AppOptions options;
    EXPECT_STREQ(internal::CreateAppIdentifierFromOptions(options).c_str(), "");
  }
  {
    AppOptions options;
    options.set_package_name("org.foo.bar");
    EXPECT_STREQ(internal::CreateAppIdentifierFromOptions(options).c_str(),
                 "org.foo.bar");
  }
  {
    AppOptions options;
    options.set_project_id("cpp-sample-app-14e43");
    EXPECT_STREQ(internal::CreateAppIdentifierFromOptions(options).c_str(),
                 "cpp-sample-app-14e43");
  }
  {
    AppOptions options;
    options.set_project_id("cpp-sample-app-14e43");
    options.set_package_name("org.foo.bar");
    EXPECT_STREQ(internal::CreateAppIdentifierFromOptions(options).c_str(),
                 "org.foo.bar.cpp-sample-app-14e43");
  }
}

#endif  // TEST_RESOURCES_AVAILABLE
}  // namespace firebase
