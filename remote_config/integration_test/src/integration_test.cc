// Copyright 2019 Google Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <inttypes.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <future>
#include <memory>

#include "app_framework.h"  // NOLINT
#include "firebase/app.h"
#include "firebase/internal/platform.h"
#include "firebase/log.h"
#include "firebase/remote_config.h"
#include "firebase/remote_config/config_update_listener_registration.h"
#include "firebase/util.h"
#include "firebase_test_framework.h"  // NOLINT

// The TO_STRING macro is useful for command line defined strings as the quotes
// get stripped.
#define TO_STRING_EXPAND(X) #X
#define TO_STRING(X) TO_STRING_EXPAND(X)

// Path to the Firebase config file to load.
#ifdef FIREBASE_CONFIG
#define FIREBASE_CONFIG_STRING TO_STRING(FIREBASE_CONFIG)
#else
#define FIREBASE_CONFIG_STRING ""
#endif  // FIREBASE_CONFIG

namespace firebase_testapp_automated {

using app_framework::LogDebug;
using app_framework::LogWarning;
using app_framework::ProcessEvents;
using firebase::Future;
using firebase::remote_config::RemoteConfig;
using firebase_test_framework::FirebaseTest;

using testing::UnorderedElementsAre;

class FirebaseRemoteConfigTest : public FirebaseTest {
 public:
  FirebaseRemoteConfigTest();
  ~FirebaseRemoteConfigTest() override;

  void SetUp() override;
  void TearDown() override;

 protected:
  // Initialize Firebase App and Firebase Remote Config.
  void Initialize();
  // Shut down Firebase Remote Config and Firebase App.
  void Terminate();

  bool initialized_ = false;
  RemoteConfig* rc_ = nullptr;
};

FirebaseRemoteConfigTest::FirebaseRemoteConfigTest() : initialized_(false) {
  FindFirebaseConfig(FIREBASE_CONFIG_STRING);
}

FirebaseRemoteConfigTest::~FirebaseRemoteConfigTest() {
  // Must be cleaned up on exit.
  assert(app_ == nullptr);
  assert(rc_ == nullptr);
}

void FirebaseRemoteConfigTest::SetUp() {
  FirebaseTest::SetUp();
  Initialize();
}

void FirebaseRemoteConfigTest::TearDown() {
  // Delete the shared path, if there is one.
  Terminate();
  FirebaseTest::TearDown();
}

void FirebaseRemoteConfigTest::Initialize() {
  if (initialized_) return;
  SetLogLevel(app_framework::kDebug);

  InitializeApp();

  LogDebug("Initializing Firebase Remote Config.");

  ::firebase::ModuleInitializer initializer;

  void* ptr = nullptr;
  ptr = &rc_;
  initializer.Initialize(app_, ptr, [](::firebase::App* app, void* target) {
    LogDebug("Try to initialize Firebase RemoteConfig");
    RemoteConfig** rc_ptr = reinterpret_cast<RemoteConfig**>(target);
    *rc_ptr = RemoteConfig::GetInstance(app);
    return firebase::kInitResultSuccess;
  });

  WaitForCompletion(initializer.InitializeLastResult(), "Initialize");

  ASSERT_EQ(initializer.InitializeLastResult().error(), 0)
      << initializer.InitializeLastResult().error_message();

  LogDebug("Successfully initialized Firebase RemoteConfig.");

  initialized_ = true;
}

void FirebaseRemoteConfigTest::Terminate() {
  if (!initialized_) return;

  LogDebug("Shutdown the Remote Config library.");
  if (rc_) {
    delete rc_;
    rc_ = nullptr;
  }
  LogDebug("Terminating.");
  TerminateApp();

  initialized_ = false;

  ProcessEvents(100);
}

static const char* ValueSourceToString(
    firebase::remote_config::ValueSource source) {
  static const char* kSourceToString[] = {
      "Static",   // kValueSourceStaticValue
      "Remote",   // kValueSourceRemoteValue
      "Default",  // kValueSourceDefaultValue
  };
  return kSourceToString[source];
}

static const unsigned char kBinaryDefaults[] = {6, 0, 0, 6, 7, 3};
// NOLINTNEXTLINE
static const firebase::remote_config::ConfigKeyValueVariant defaults[] = {
    {"TestBoolean", false},
    {"TestLong", 42},
    {"TestDouble", 3.14},
    {"TestString", "Hello World"},
    {"TestData", firebase::Variant::FromStaticBlob(kBinaryDefaults,
                                                   sizeof(kBinaryDefaults))},
    {"TestDefaultOnly", "Default value that won't be overridden"}};

//   TestData     4321
//   TestDouble   625.63
//   TestLong     119
//   TestBoolean  true
//   TestString   This is a string
// NOLINTNEXTLINE
static const firebase::remote_config::ConfigKeyValueVariant kServerValue[] = {
    {"TestBoolean", true},
    {"TestLong", 119},
    {"TestDouble", 625.63},
    {"TestString", firebase::Variant::FromMutableString("This is a string")},
    {"TestData", 4321},
    {"TestDefaultOnly", firebase::Variant::FromMutableString(
                            "Default value that won't be overridden")}};

static Future<void> SetDefaults(RemoteConfig* rc) {
  size_t default_count = FIREBASE_ARRAYSIZE(defaults);
  return rc->SetDefaults(defaults, default_count);
}

static Future<void> SetDefaultConfigSettings(RemoteConfig* rc) {
  firebase::remote_config::ConfigSettings defaultConfigSettings;
  return rc->SetConfigSettings(defaultConfigSettings);
}

static Future<void> SetZeroIntervalConfigSettings(RemoteConfig* rc) {
  firebase::remote_config::ConfigSettings zeroIntervalConfigSettings;
  zeroIntervalConfigSettings.minimum_fetch_interval_in_milliseconds = 0;
  return rc->SetConfigSettings(zeroIntervalConfigSettings);
}

// Test cases below.

TEST_F(FirebaseRemoteConfigTest, TestInitializeAndTerminate) {
  // Already tested via SetUp() and TearDown().
}

/// This test requires to be run on a device or simulator that does not have a
/// newer version of the config saved on disk from a previous test run.
TEST_F(FirebaseRemoteConfigTest, TestSetDefault) {
  ASSERT_NE(rc_, nullptr);

  EXPECT_TRUE(WaitForCompletion(SetDefaults(rc_), "SetDefaults"));

  bool validated_defaults = true;
  firebase::remote_config::ValueInfo value_info;
  bool bool_value = rc_->GetBoolean("TestBoolean", &value_info);
  if (value_info.source == firebase::remote_config::kValueSourceDefaultValue) {
    EXPECT_FALSE(bool_value);
  } else {
    validated_defaults = false;
  }
  int64_t int64_value = rc_->GetLong("TestLong", &value_info);
  if (value_info.source == firebase::remote_config::kValueSourceDefaultValue) {
    EXPECT_EQ(int64_value, 42);
  } else {
    validated_defaults = false;
  }
  double double_value = rc_->GetDouble("TestDouble", &value_info);
  if (value_info.source == firebase::remote_config::kValueSourceDefaultValue) {
    EXPECT_NEAR(double_value, 3.14, 0.0001);
  } else {
    validated_defaults = false;
  }
  std::string string_value = rc_->GetString("TestString", &value_info);
  if (value_info.source == firebase::remote_config::kValueSourceDefaultValue) {
    EXPECT_EQ(string_value, "Hello World");
  } else {
    validated_defaults = false;
  }
  std::vector<unsigned char> blob_value =
      rc_->GetData("TestData");  //, &value_info);
  if (value_info.source == firebase::remote_config::kValueSourceDefaultValue) {
    EXPECT_THAT(blob_value, testing::ElementsAreArray(kBinaryDefaults,
                                                      sizeof(kBinaryDefaults)));
  } else {
    validated_defaults = false;
  }

  if (!validated_defaults) {
    LogWarning(
        "Can't validate defaults, they've been overridden by server values.\n"
#if defined(__ANDROID__)
        "Delete the app's data and run this test again to test SetDefaults:\n"
        " adb shell pm clear [bundle ID]"
#elif defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
        "Uninstall and re-install the app and run this again to test "
        "SetDefaults."
#else  // Desktop
        "Delete the Remote Config cache and run this test again to test "
        "SetDefaults:\n"
#if defined(_WIN32)
        " del remote_config_data"
#else
        " rm remote_config_data"
#endif  // defined(_WIN32)
#endif  // defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
    );
  }
}

/// This test requires to be run on a device or simulator that does not have the
/// template version number stored on the disk or auto-fetch will be skipped.
TEST_F(FirebaseRemoteConfigTest, TestAddOnConfigUpdateListener) {
  ASSERT_NE(rc_, nullptr);

  // Check if the config has default values. If not, we have cached data
  // from a previous test run, and auto-fetch will not happen.
  EXPECT_TRUE(WaitForCompletion(SetDefaults(rc_), "SetDefaults"));
  bool validated_defaults = true;
  firebase::remote_config::ValueInfo value_info;
  bool bool_value = rc_->GetBoolean("TestBoolean", &value_info);
  bool has_cached_data =
      value_info.source != firebase::remote_config::kValueSourceDefaultValue;

  if (has_cached_data) {
    LogWarning(
        "Can't validate defaults, they've been overridden by server values.\n"
#if defined(__ANDROID__)
        "Delete the app's data and run this test again to test\n"
        " AddOnConfigUpdateListener: adb shell pm clear [bundle ID]"
#elif defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
        "Uninstall and re-install the app and run this again to test "
        "AddOnConfigUpdateListener."
#else  // Desktop
        "Delete the Remote Config cache and run this test again to test "
        "AddOnConfigUpdateListener:\n"
#if defined(_WIN32)
        " del remote_config_data"
#else
        " rm remote_config_data"
#endif  // defined(_WIN32)
#endif  // defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
    );
  }
// Realtime RC is not yet supported on desktop.
#if FIREBASE_PLATFORM_DESKTOP
  rc_->AddOnConfigUpdateListener(
      [](firebase::remote_config::ConfigUpdate&&,
         firebase::remote_config::RemoteConfigError) {});
#else
  auto config_update_promise = std::make_shared<std::promise<void> >();

  firebase::remote_config::ConfigUpdateListenerRegistration registration =
      rc_->AddOnConfigUpdateListener(
          [&, config_update_promise](
              firebase::remote_config::ConfigUpdate&& configUpdate,
              firebase::remote_config::RemoteConfigError remoteConfigError) {
            EXPECT_EQ(configUpdate.updated_keys.size(), 5);
            config_update_promise->set_value();
          });
  if (!has_cached_data) {
    auto config_update_future = config_update_promise->get_future();
    ASSERT_EQ(std::future_status::ready,
              config_update_future.wait_for(std::chrono::milliseconds(10000)));

    // On Android WaitForCompletion must be called from the main thread,
    // so Activate is called here outside of the listener.
    EXPECT_TRUE(WaitForCompletion(rc_->Activate(), "Activate"));
    LogDebug("Real-time Config Update keys retrieved.");

    std::map<std::string, firebase::Variant> key_values = rc_->GetAll();
    EXPECT_EQ(key_values.size(), 6);

    for (auto key_valur_pair : kServerValue) {
      firebase::Variant k_value = key_valur_pair.value;
      firebase::Variant fetched_value = key_values[key_valur_pair.key];
      EXPECT_EQ(k_value.type(), fetched_value.type());
      EXPECT_EQ(k_value, fetched_value);
    }
    registration.Remove();
  }
#endif  // !FIREBASE_PLATFORM_DESKTOP
}

TEST_F(FirebaseRemoteConfigTest, TestRemoveConfigUpdateListener) {
#if !FIREBASE_PLATFORM_DESKTOP

  firebase::remote_config::ConfigUpdateListenerRegistration registration =
      rc_->AddOnConfigUpdateListener(
          [](firebase::remote_config::ConfigUpdate&& configUpdate,
             firebase::remote_config::RemoteConfigError remoteConfigError) {});

  registration.Remove();

#endif  // !FIREBASE_PLATFORM_DESKTOP
}

TEST_F(FirebaseRemoteConfigTest, TestGetKeys) {
  ASSERT_NE(rc_, nullptr);

  EXPECT_TRUE(WaitForCompletion(SetDefaults(rc_), "SetDefaults"));

  std::vector<std::string> keys = rc_->GetKeys();
  EXPECT_THAT(
      keys, UnorderedElementsAre("TestBoolean", "TestLong", "TestDouble",
                                 "TestString", "TestData", "TestDefaultOnly"));
  std::vector<std::string> keys_subset = rc_->GetKeysByPrefix("TestD");
  EXPECT_THAT(keys_subset, UnorderedElementsAre("TestDouble", "TestData",
                                                "TestDefaultOnly"));
}

//   TestData     4321
//   TestDouble   625.63
//   TestLong     119
//   TestBoolean  true
//   TestString   This is a string
TEST_F(FirebaseRemoteConfigTest, TestGetAll) {
  ASSERT_NE(rc_, nullptr);

  EXPECT_TRUE(WaitForCompletion(SetDefaults(rc_), "SetDefaults"));
  EXPECT_TRUE(
      WaitForCompletion(RunWithRetry([&]() { return rc_->Fetch(); }), "Fetch"));
  EXPECT_TRUE(WaitForCompletion(rc_->Activate(), "Activate"));
  std::map<std::string, firebase::Variant> key_values = rc_->GetAll();
  EXPECT_EQ(key_values.size(), 6);

  for (auto key_valur_pair : kServerValue) {
    firebase::Variant k_value = key_valur_pair.value;
    firebase::Variant fetched_value = key_values[key_valur_pair.key];
    EXPECT_EQ(k_value.type(), fetched_value.type());
    EXPECT_EQ(k_value, fetched_value);
  }
}

/* The following test expects that you have your server values set to:
   TestData     4321
   TestDouble   625.63
   TestLong     119
   TestBoolean  true
   TestString   This is a string
 */
TEST_F(FirebaseRemoteConfigTest, TestFetch) {
  ASSERT_NE(rc_, nullptr);

  EXPECT_TRUE(WaitForCompletion(SetDefaults(rc_), "SetDefaults"));
  EXPECT_TRUE(
      WaitForCompletion(RunWithRetry([&]() { return rc_->Fetch(); }), "Fetch"));
  EXPECT_TRUE(WaitForCompletion(rc_->Activate(), "Activate"));
  LogDebug("Fetch time: %lld", rc_->GetInfo().fetch_time);
  firebase::remote_config::ValueInfo value_info;
  bool bool_value = rc_->GetBoolean("TestBoolean", &value_info);
  EXPECT_EQ(value_info.source, firebase::remote_config::kValueSourceRemoteValue)
      << "TestBoolean source is " << ValueSourceToString(value_info.source)
      << ", expected Remote";
  EXPECT_TRUE(bool_value);

  int64_t int64_value = rc_->GetLong("TestLong", &value_info);
  EXPECT_EQ(value_info.source, firebase::remote_config::kValueSourceRemoteValue)
      << "TestLong source is " << ValueSourceToString(value_info.source)
      << ", expected Remote";
  EXPECT_EQ(int64_value, 119);

  double double_value = rc_->GetDouble("TestDouble", &value_info);
  EXPECT_EQ(value_info.source, firebase::remote_config::kValueSourceRemoteValue)
      << "TestDouble source is " << ValueSourceToString(value_info.source)
      << ", expected Remote";
  EXPECT_NEAR(double_value, 625.63, 0.0001);

  std::string string_value = rc_->GetString("TestString", &value_info);
  EXPECT_EQ(value_info.source, firebase::remote_config::kValueSourceRemoteValue)
      << "TestString source is " << ValueSourceToString(value_info.source)
      << ", expected Remote";
  EXPECT_EQ(string_value, "This is a string");

  std::vector<unsigned char> blob_value =
      rc_->GetData("TestData");  //, &value_info);
  EXPECT_EQ(value_info.source, firebase::remote_config::kValueSourceRemoteValue)
      << "TestData source is " << ValueSourceToString(value_info.source)
      << ", expected Remote";

  const unsigned char kExpectedBlobServerValue[] = {'4', '3', '2', '1'};
  EXPECT_THAT(blob_value,
              testing::ElementsAreArray(kExpectedBlobServerValue,
                                        sizeof(kExpectedBlobServerValue)));
}

TEST_F(FirebaseRemoteConfigTest, TestFetchInterval) {
  ASSERT_NE(rc_, nullptr);
  EXPECT_TRUE(
      WaitForCompletion(RunWithRetry([&]() { return rc_->FetchAndActivate(); }),
                        "FetchAndActivate"));
  uint64_t current_fetch_time = rc_->GetInfo().fetch_time;
  // Making sure the config settings's fetch interval is 12 hours
  EXPECT_TRUE(WaitForCompletion(SetDefaultConfigSettings(rc_),
                                "SetDefaultConfigSettings"));
  // Second fetch, should respect fetch interval and don't change data.
  EXPECT_TRUE(
      WaitForCompletion(RunWithRetry([&]() { return rc_->Fetch(); }), "Fetch"));
  EXPECT_EQ(current_fetch_time, rc_->GetInfo().fetch_time);
  // Update fetch interval to 0
  EXPECT_TRUE(WaitForCompletion(SetZeroIntervalConfigSettings(rc_),
                                "SetZeroIntervalConfigSettings"));
  EXPECT_EQ(0, rc_->GetConfigSettings().minimum_fetch_interval_in_milliseconds);
  // Third fetch, this should operate the real fetch and update the fetch time.
  EXPECT_TRUE(
      WaitForCompletion(RunWithRetry([&]() { return rc_->Fetch(); }), "Fetch"));
  EXPECT_NE(current_fetch_time, rc_->GetInfo().fetch_time);
}

TEST_F(FirebaseRemoteConfigTest, TestFetchSecondsParameter) {
  ASSERT_NE(rc_, nullptr);

  EXPECT_TRUE(
      WaitForCompletion(RunWithRetry([&]() { return rc_->FetchAndActivate(); }),
                        "FetchAndActivate"));
  uint64_t current_fetch_time = rc_->GetInfo().fetch_time;
  // Making sure the config settings's fetch interval is 12 hours
  EXPECT_TRUE(WaitForCompletion(SetDefaultConfigSettings(rc_),
                                "SetDefaultConfigSettings"));
  // Test Fetch() without specifying an interval; it should not fetch.
  EXPECT_TRUE(WaitForCompletion(RunWithRetry([&]() { return rc_->Fetch(); }),
                                "Fetch() [should not fetch]"));
  EXPECT_EQ(current_fetch_time, rc_->GetInfo().fetch_time);

  FLAKY_TEST_SECTION_BEGIN();

  // Call Fetch(0), forcing a fetch.
  EXPECT_TRUE(WaitForCompletion(RunWithRetry([&]() { return rc_->Fetch(0); }),
                                "Fetch(0) [should fetch]"));
  EXPECT_NE(current_fetch_time, rc_->GetInfo().fetch_time);

  current_fetch_time = rc_->GetInfo().fetch_time;
  // Call Fetch(30), which shouldn't fetch yet.
  EXPECT_TRUE(WaitForCompletion(RunWithRetry([&]() { return rc_->Fetch(30); }),
                                "Fetch(30) [should not fetch]"));
  EXPECT_EQ(current_fetch_time, rc_->GetInfo().fetch_time);

  LogDebug("Pausing 45 seconds before re-running Fetch");
  for (int i = 0; i < 45; i++) {
    ProcessEvents(1000);
  }
  // After waiting 45 seconds, Fetch(30) should now fetch.
  EXPECT_TRUE(WaitForCompletion(RunWithRetry([&]() { return rc_->Fetch(30); }),
                                "Fetch(30) [should fetch]"));
  EXPECT_NE(current_fetch_time, rc_->GetInfo().fetch_time);

  FLAKY_TEST_SECTION_END();
}
}  // namespace firebase_testapp_automated
