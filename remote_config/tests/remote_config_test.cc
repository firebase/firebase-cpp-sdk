// Copyright 2017 Google LLC
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

#if defined(FIREBASE_ANDROID_FOR_DESKTOP)
#define __ANDROID__
#include <jni.h>
#include "testing/run_all_tests.h"
#endif  // defined(FIREBASE_ANDROID_FOR_DESKTOP)

#include "app/src/include/firebase/app.h"
#include "app/tests/include/firebase/app_for_testing.h"
#include "remote_config/src/include/firebase/remote_config.h"

#if defined(FIREBASE_ANDROID_FOR_DESKTOP)
#undef __ANDROID__
#endif  // defined(FIREBASE_ANDROID_FOR_DESKTOP)

#include <string>
#include <vector>

#include "testing/config.h"
#include "testing/reporter.h"
#include "testing/ticker.h"
#include "firebase/variant.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"

namespace firebase {
namespace remote_config {

class RemoteConfigTest : public ::testing::Test {
 protected:
  void SetUp() override {
    firebase::testing::cppsdk::TickerReset();
    firebase::testing::cppsdk::ConfigSet("{}");
    reporter_.reset();
    InitializeRemoteConfig();
  }

  void TearDown() override {
    Terminate();
    delete firebase_app_;
    firebase_app_ = nullptr;

    EXPECT_THAT(reporter_.getFakeReports(),
                ::testing::Eq(reporter_.getExpectations()));
  }

  void InitializeRemoteConfig() {
    firebase_app_ = testing::CreateApp();
    EXPECT_NE(firebase_app_, nullptr) << "Init app failed";

    InitResult result = Initialize(*firebase_app_);
    EXPECT_NE(firebase_app_, nullptr) << "Init app failed";
    EXPECT_EQ(result, kInitResultSuccess);
  }

  App* firebase_app_ = nullptr;
  firebase::testing::cppsdk::Reporter reporter_;
};

#define REPORT_EXPECT(fake, result, ...)                                  \
  reporter_.addExpectation(fake, result, firebase::testing::cppsdk::kAny, \
                           __VA_ARGS__)

#define REPORT_EXPECT_PLATFORM(fake, result, platform, ...) \
  reporter_.addExpectation(fake, result, platform, __VA_ARGS__)

// Check SetUp and TearDown working well.
TEST_F(RemoteConfigTest, InitializeAndTerminate) {}

TEST_F(RemoteConfigTest, InitializeTwice) {
  InitResult result = Initialize(*firebase_app_);
  EXPECT_EQ(result, kInitResultSuccess);
}

#if defined(FIREBASE_ANDROID_FOR_DESKTOP)
TEST_F(RemoteConfigTest, SetDefaultsOnAndroid) {
  REPORT_EXPECT("FirebaseRemoteConfig.setDefaultsAsync", "", {"0"});
  SetDefaults(0);
}

#endif  // defined(FIREBASE_ANDROID_FOR_DESKTOP)

TEST_F(RemoteConfigTest, SetDefaultsWithNullConfigKeyValueVariant) {
  REPORT_EXPECT("FirebaseRemoteConfig.setDefaultsAsync", "", {"{}"});
  ConfigKeyValueVariant* keyvalues = nullptr;
  SetDefaults(keyvalues, 0);
}

TEST_F(RemoteConfigTest, SetDefaultsWithConfigKeyValueVariant) {
  REPORT_EXPECT("FirebaseRemoteConfig.setDefaultsAsync", "",
                {"{color=black, height=120}"});

  ConfigKeyValueVariant defaults[] = {
      ConfigKeyValueVariant{"color", Variant("black")},
      ConfigKeyValueVariant{"height", Variant(120)}};

  SetDefaults(defaults, 2);
}

TEST_F(RemoteConfigTest, SetDefaultsWithNullConfigKeyValue) {
  REPORT_EXPECT("FirebaseRemoteConfig.setDefaultsAsync", "", {"{}"});
  ConfigKeyValue* keyvalues = nullptr;
  SetDefaults(keyvalues, 0);
}

TEST_F(RemoteConfigTest, SetDefaultsWithConfigKeyValue) {
  REPORT_EXPECT("FirebaseRemoteConfig.setDefaultsAsync", "",
                {"{color=black, height=120, width=600.5}"});

  ConfigKeyValue defaults[] = {ConfigKeyValue{"color", "black"},
                               ConfigKeyValue{"height", "120"},
                               ConfigKeyValue{"width", "600.5"}};

  SetDefaults(defaults, 3);
}

TEST_F(RemoteConfigTest, GetConfigSettingTrue) {
  GTEST_SKIP(); // TODO(cynthiajiang): Re-implement the test in SetGet ConfigSetting V2 update
  REPORT_EXPECT("FirebaseRemoteConfig.getInfo", "", {});
  REPORT_EXPECT("FirebaseRemoteConfigInfo.getConfigSettings", "", {});
  REPORT_EXPECT("FirebaseRemoteConfigSettings.isDeveloperModeEnabled", "true",
                {});
  firebase::testing::cppsdk::ConfigSet(
      "{"
      "  config:["
      "    {fake:'FirebaseRemoteConfigSettings.isDeveloperModeEnabled',"
      "     returnvalue: {'tbool': true}}"
      "  ]"
      "}");
  EXPECT_EQ(GetConfigSetting(kConfigSettingDeveloperMode), "1");
}

TEST_F(RemoteConfigTest, GetConfigSettingFalse) {
  GTEST_SKIP(); // TODO(cynthiajiang): Re-implement the test in SetGet ConfigSetting V2 update
  REPORT_EXPECT("FirebaseRemoteConfig.getInfo", "", {});
  REPORT_EXPECT("FirebaseRemoteConfigInfo.getConfigSettings", "", {});
  REPORT_EXPECT("FirebaseRemoteConfigSettings.isDeveloperModeEnabled", "false",
                {});
  firebase::testing::cppsdk::ConfigSet(
      "{"
      "  config:["
      "    {fake:'FirebaseRemoteConfigSettings.isDeveloperModeEnabled',"
      "     returnvalue: {'tbool': false}}"
      "  ]"
      "}");
  EXPECT_EQ(GetConfigSetting(kConfigSettingDeveloperMode), "0");
}

TEST_F(RemoteConfigTest, SetConfigSettingTrue) {
  GTEST_SKIP(); // TODO(cynthiajiang): Re-implement the test in SetGet ConfigSetting V2 update
  REPORT_EXPECT("FirebaseRemoteConfig.setConfigSettings", "", {});
  REPORT_EXPECT("FirebaseRemoteConfigSettings.Builder.setDeveloperModeEnabled",
                "", {"true"});
  REPORT_EXPECT("FirebaseRemoteConfigSettings.Builder.build", "", {});
  SetConfigSetting(kConfigSettingDeveloperMode, "1");
}

TEST_F(RemoteConfigTest, SetConfigSettingFalse) {
  GTEST_SKIP(); // TODO(cynthiajiang): Re-implement the test in SetGet ConfigSetting V2 update
  REPORT_EXPECT("FirebaseRemoteConfig.setConfigSettings", "", {});
  REPORT_EXPECT("FirebaseRemoteConfigSettings.Builder.setDeveloperModeEnabled",
                "", {"false"});
  REPORT_EXPECT("FirebaseRemoteConfigSettings.Builder.build", "", {});
  SetConfigSetting(kConfigSettingDeveloperMode, "0");
}

// Start check GetBoolean functions
TEST_F(RemoteConfigTest, GetBooleanNullKey) {
  REPORT_EXPECT("FirebaseRemoteConfig.getBoolean", "false", {""});
  firebase::testing::cppsdk::ConfigSet(
      "{"
      "  config:["
      "    {fake:'FirebaseRemoteConfig.getBoolean',"
      "     returnvalue: {'tbool': false}}"
      "  ]"
      "}");
  const char* key = nullptr;
  EXPECT_FALSE(GetBoolean(key));
}

TEST_F(RemoteConfigTest, GetBooleanKey) {
  REPORT_EXPECT("FirebaseRemoteConfig.getBoolean", "true", {"give_prize"});
  firebase::testing::cppsdk::ConfigSet(
      "{"
      "  config:["
      "    {fake:'FirebaseRemoteConfig.getBoolean',"
      "     returnvalue: {'tbool': true}}"
      "  ]"
      "}");
  EXPECT_TRUE(GetBoolean("give_prize"));
}

TEST_F(RemoteConfigTest, GetBooleanKeyAndNullInfo) {
  REPORT_EXPECT("FirebaseRemoteConfig.getValue", "", {"give_prize"});
  REPORT_EXPECT("FirebaseRemoteConfigValue.asBoolean", "true", {});
  firebase::testing::cppsdk::ConfigSet(
      "{"
      "  config:["
      "    {fake:'FirebaseRemoteConfigValue.asBoolean',"
      "     returnvalue: {'tbool': true}}"
      "  ]"
      "}");
  ValueInfo* info = nullptr;
  EXPECT_TRUE(GetBoolean("give_prize", info));
}

TEST_F(RemoteConfigTest, GetBooleanKeyAndInfo) {
  REPORT_EXPECT("FirebaseRemoteConfig.getValue", "", {"give_prize"});
  REPORT_EXPECT("FirebaseRemoteConfigValue.asBoolean", "true", {});
  REPORT_EXPECT("FirebaseRemoteConfigValue.getSource", "1", {});
  firebase::testing::cppsdk::ConfigSet(
      "{"
      "  config:["
      "    {fake:'FirebaseRemoteConfigValue.asBoolean',"
      "     returnvalue: {'tbool': true}},"
      "    {fake:'FirebaseRemoteConfigValue.getSource',"
      "     returnvalue: {'tint': 1}}"
      "  ]"
      "}");
  ValueInfo info;
  EXPECT_TRUE(GetBoolean("give_prize", &info));
  EXPECT_EQ(info.source, kValueSourceDefaultValue);
  EXPECT_TRUE(info.conversion_successful);
}
// Finish check GetBoolean functions

// Start check GetLong functions
TEST_F(RemoteConfigTest, GetLongNullKey) {
  REPORT_EXPECT("FirebaseRemoteConfig.getLong", "1000", {""});
  firebase::testing::cppsdk::ConfigSet(
      "{"
      "  config:["
      "    {fake:'FirebaseRemoteConfig.getLong',"
      "     returnvalue: {'tlong': 1000}}"
      "  ]"
      "}");
  const char* key = nullptr;
  EXPECT_EQ(GetLong(key), 1000);
}

TEST_F(RemoteConfigTest, GetLongKey) {
  REPORT_EXPECT("FirebaseRemoteConfig.getLong", "1000000000", {"price"});
  firebase::testing::cppsdk::ConfigSet(
      "{"
      "  config:["
      "    {fake:'FirebaseRemoteConfig.getLong',"
      "     returnvalue: {'tlong': 1000000000}}"
      "  ]"
      "}");
  EXPECT_EQ(GetLong("price"), 1000000000);
}

TEST_F(RemoteConfigTest, GetLongKeyAndNullInfo) {
  REPORT_EXPECT("FirebaseRemoteConfig.getValue", "", {"wallet_cash"});
  REPORT_EXPECT("FirebaseRemoteConfigValue.asLong", "100", {});
  firebase::testing::cppsdk::ConfigSet(
      "{"
      "  config:["
      "    {fake:'FirebaseRemoteConfigValue.asLong',"
      "     returnvalue: {'tlong': 100}}"
      "  ]"
      "}");
  ValueInfo* info = nullptr;
  EXPECT_EQ(GetLong("wallet_cash", info), 100);
}

TEST_F(RemoteConfigTest, GetLongKeyAndInfo) {
  REPORT_EXPECT("FirebaseRemoteConfig.getValue", "", {"wallet_cash"});
  REPORT_EXPECT("FirebaseRemoteConfigValue.asLong", "7000000", {});
  REPORT_EXPECT("FirebaseRemoteConfigValue.getSource", "1", {});
  firebase::testing::cppsdk::ConfigSet(
      "{"
      "  config:["
      "    {fake:'FirebaseRemoteConfigValue.asLong',"
      "     returnvalue: {'tlong': 7000000}},"
      "    {fake:'FirebaseRemoteConfigValue.getSource',"
      "     returnvalue: {'tint': 1}}"
      "  ]"
      "}");
  ValueInfo info;
  EXPECT_EQ(GetLong("wallet_cash", &info), 7000000);
  EXPECT_EQ(info.source, kValueSourceDefaultValue);
  EXPECT_TRUE(info.conversion_successful);
}
// Finish check GetLong functions

// Start check GetDouble functions
TEST_F(RemoteConfigTest, GetDoubleNullKey) {
  REPORT_EXPECT("FirebaseRemoteConfig.getDouble", "1000.500", {""});
  firebase::testing::cppsdk::ConfigSet(
      "{"
      "  config:["
      "    {fake:'FirebaseRemoteConfig.getDouble',"
      "     returnvalue: {'tdouble': 1000.500}}"
      "  ]"
      "}");
  const char* key = nullptr;
  EXPECT_EQ(GetDouble(key), 1000.500);
}

TEST_F(RemoteConfigTest, GetDoubleKey) {
  REPORT_EXPECT("FirebaseRemoteConfig.getDouble", "1000000000.000", {"price"});
  firebase::testing::cppsdk::ConfigSet(
      "{"
      "  config:["
      "    {fake:'FirebaseRemoteConfig.getDouble',"
      "     returnvalue: {'tdouble': 1000000000.000}}"
      "  ]"
      "}");
  EXPECT_EQ(GetDouble("price"), 1000000000.000);
}

TEST_F(RemoteConfigTest, GetDoubleKeyAndNullInfo) {
  REPORT_EXPECT("FirebaseRemoteConfig.getValue", "", {"wallet_cash"});
  REPORT_EXPECT("FirebaseRemoteConfigValue.asDouble", "100.999", {});
  firebase::testing::cppsdk::ConfigSet(
      "{"
      "  config:["
      "    {fake:'FirebaseRemoteConfigValue.asDouble',"
      "     returnvalue: {'tdouble': 100.999}}"
      "  ]"
      "}");
  ValueInfo* info = nullptr;
  EXPECT_EQ(GetDouble("wallet_cash", info), 100.999);
}

TEST_F(RemoteConfigTest, GetDoubleKeyAndInfo) {
  REPORT_EXPECT("FirebaseRemoteConfig.getValue", "", {"wallet_cash"});
  REPORT_EXPECT("FirebaseRemoteConfigValue.asDouble", "7000000.000", {});
  REPORT_EXPECT("FirebaseRemoteConfigValue.getSource", "1", {});
  firebase::testing::cppsdk::ConfigSet(
      "{"
      "  config:["
      "    {fake:'FirebaseRemoteConfigValue.asDouble',"
      "     returnvalue: {'tdouble': 7000000.000}},"
      "    {fake:'FirebaseRemoteConfigValue.getSource',"
      "     returnvalue: {'tint': 1}}"
      "  ]"
      "}");
  ValueInfo info;
  EXPECT_EQ(GetDouble("wallet_cash", &info), 7000000.000);
  EXPECT_EQ(info.source, kValueSourceDefaultValue);
  EXPECT_TRUE(info.conversion_successful);
}
// Finish check GetDouble functions

// Start check GetString functions
TEST_F(RemoteConfigTest, GetStringNullKey) {
  REPORT_EXPECT("FirebaseRemoteConfig.getString", "I am fake", {""});
  firebase::testing::cppsdk::ConfigSet(
      "{"
      "  config:["
      "    {fake:'FirebaseRemoteConfig.getString',"
      "     returnvalue: {'tstring': 'I am fake'}}"
      "  ]"
      "}");
  const char* key = nullptr;
  EXPECT_EQ(GetString(key), "I am fake");
}

TEST_F(RemoteConfigTest, GetStringKey) {
  REPORT_EXPECT("FirebaseRemoteConfig.getString", "I am fake", {"price"});
  firebase::testing::cppsdk::ConfigSet(
      "{"
      "  config:["
      "    {fake:'FirebaseRemoteConfig.getString',"
      "     returnvalue: {'tstring': 'I am fake'}}"
      "  ]"
      "}");
  EXPECT_EQ(GetString("price"), "I am fake");
}

TEST_F(RemoteConfigTest, GetStringKeyAndNullInfo) {
  REPORT_EXPECT("FirebaseRemoteConfig.getValue", "", {"wallet_cash"});
  REPORT_EXPECT("FirebaseRemoteConfigValue.asString", "I am fake", {});
  firebase::testing::cppsdk::ConfigSet(
      "{"
      "  config:["
      "    {fake:'FirebaseRemoteConfigValue.asString',"
      "     returnvalue: {'tstring': 'I am fake'}}"
      "  ]"
      "}");
  ValueInfo* info = nullptr;
  EXPECT_EQ(GetString("wallet_cash", info), "I am fake");
}

TEST_F(RemoteConfigTest, GetStringKeyAndInfo) {
  REPORT_EXPECT("FirebaseRemoteConfig.getValue", "", {"wallet_cash"});
  REPORT_EXPECT("FirebaseRemoteConfigValue.asString", "I am fake", {});
  REPORT_EXPECT("FirebaseRemoteConfigValue.getSource", "1", {});
  firebase::testing::cppsdk::ConfigSet(
      "{"
      "  config:["
      "    {fake:'FirebaseRemoteConfigValue.asString',"
      "     returnvalue: {'tstring': 'I am fake'}},"
      "    {fake:'FirebaseRemoteConfigValue.getSource',"
      "     returnvalue: {'tint': 1}}"
      "  ]"
      "}");
  ValueInfo info;
  EXPECT_EQ(GetString("wallet_cash", &info), "I am fake");
  EXPECT_EQ(info.source, kValueSourceDefaultValue);
  EXPECT_TRUE(info.conversion_successful);
}
// Finish check GetString functions

// Start check GetData functions
TEST_F(RemoteConfigTest, GetDataNullKey) {
  GTEST_SKIP(); // TODO(cynthiajiang): Re-implement the test using GetData
  REPORT_EXPECT("FirebaseRemoteConfig.getByteArray", "abcd", {""});
  firebase::testing::cppsdk::ConfigSet(
      "{"
      "  config:["
      "    {fake:'FirebaseRemoteConfig.getByteArray',"
      "     returnvalue: {'tstring': 'abcd'}}"
      "  ]"
      "}");
  const char* key = nullptr;
  EXPECT_THAT(GetData(key),
              ::testing::Eq(std::vector<unsigned char>({'a', 'b', 'c', 'd'})));
}

TEST_F(RemoteConfigTest, GetDataKey) {
  GTEST_SKIP(); // TODO(cynthiajiang): Re-implement the test using GetData
  REPORT_EXPECT("FirebaseRemoteConfig.getByteArray", "abc", {"name"});
  firebase::testing::cppsdk::ConfigSet(
      "{"
      "  config:["
      "    {fake:'FirebaseRemoteConfig.getByteArray',"
      "     returnvalue: {'tstring': 'abc'}}"
      "  ]"
      "}");
  EXPECT_THAT(GetData("name"),
              ::testing::Eq(std::vector<unsigned char>({'a', 'b', 'c'})));
}

TEST_F(RemoteConfigTest, GetDataKeyAndNullInfo) {
  REPORT_EXPECT("FirebaseRemoteConfig.getValue", "", {"wallet_cash"});
  REPORT_EXPECT("FirebaseRemoteConfigValue.asByteArray", "xyz", {});
  firebase::testing::cppsdk::ConfigSet(
      "{"
      "  config:["
      "    {fake:'FirebaseRemoteConfigValue.asByteArray',"
      "     returnvalue: {'tstring': 'xyz'}}"
      "  ]"
      "}");
  ValueInfo* info = nullptr;
  EXPECT_THAT(GetData("wallet_cash", info),
              ::testing::Eq(std::vector<unsigned char>({'x', 'y', 'z'})));
}

TEST_F(RemoteConfigTest, GetDataKeyAndInfo) {
  REPORT_EXPECT("FirebaseRemoteConfig.getValue", "", {"wallet_cash"});
  REPORT_EXPECT("FirebaseRemoteConfigValue.asByteArray", "xyz", {});
  REPORT_EXPECT("FirebaseRemoteConfigValue.getSource", "1", {});
  firebase::testing::cppsdk::ConfigSet(
      "{"
      "  config:["
      "    {fake:'FirebaseRemoteConfigValue.asByteArray',"
      "     returnvalue: {'tstring': 'xyz'}},"
      "    {fake:'FirebaseRemoteConfigValue.getSource',"
      "     returnvalue: {'tint': 1}}"
      "  ]"
      "}");
  ValueInfo info;
  EXPECT_THAT(GetData("wallet_cash", &info),
              ::testing::Eq(std::vector<unsigned char>({'x', 'y', 'z'})));
  EXPECT_EQ(info.source, kValueSourceDefaultValue);
  EXPECT_TRUE(info.conversion_successful);
}
// Finish check GetData functions

// Start check GetKeysByPrefix functions
TEST_F(RemoteConfigTest, GetKeysByPrefix) {
  REPORT_EXPECT("FirebaseRemoteConfig.getKeysByPrefix", "[1, 2, 3, 4]",
                {"some_prefix"});
  firebase::testing::cppsdk::ConfigSet(
      "{"
      "  config:["
      "    {fake:'FirebaseRemoteConfig.getKeysByPrefix',"
      "     returnvalue: {'tstring': '[1, 2, 3, 4]'}}"
      "  ]"
      "}");
  EXPECT_THAT(GetKeysByPrefix("some_prefix"),
              ::testing::Eq(std::vector<std::string>({"1", "2", "3", "4"})));
}

TEST_F(RemoteConfigTest, GetKeysByPrefixEmptyResult) {
  REPORT_EXPECT("FirebaseRemoteConfig.getKeysByPrefix", "[]", {"some_prefix"});
  firebase::testing::cppsdk::ConfigSet(
      "{"
      "  config:["
      "    {fake:'FirebaseRemoteConfig.getKeysByPrefix',"
      "     returnvalue: {'tstring': '[]'}}"
      "  ]"
      "}");
  EXPECT_THAT(GetKeysByPrefix("some_prefix"),
              ::testing::Eq(std::vector<std::string>({})));
}

TEST_F(RemoteConfigTest, GetKeysByPrefixNullPrefix) {
  REPORT_EXPECT("FirebaseRemoteConfig.getKeysByPrefix", "[1, 2, 3, 4]", {""});
  firebase::testing::cppsdk::ConfigSet(
      "{"
      "  config:["
      "    {fake:'FirebaseRemoteConfig.getKeysByPrefix',"
      "     returnvalue: {'tstring': '[1, 2, 3, 4]'}}"
      "  ]"
      "}");
  const char* key = nullptr;
  EXPECT_THAT(GetKeysByPrefix(key),
              ::testing::Eq(std::vector<std::string>({"1", "2", "3", "4"})));
}
// Finish check GetKeysByPrefix functions

// Start check GetKeys functions
TEST_F(RemoteConfigTest, GetKeys) {
  REPORT_EXPECT("FirebaseRemoteConfig.getKeysByPrefix", "[1, 2, 3, 4]", {""});
  firebase::testing::cppsdk::ConfigSet(
      "{"
      "  config:["
      "    {fake:'FirebaseRemoteConfig.getKeysByPrefix',"
      "     returnvalue: {'tstring': '[1, 2, 3, 4]'}}"
      "  ]"
      "}");
  EXPECT_THAT(GetKeys(),
              ::testing::Eq(std::vector<std::string>({"1", "2", "3", "4"})));
}
// Finish check GetKeys functions

void Verify(const Future<void>& result) {
#if defined(FIREBASE_ANDROID_FOR_DESKTOP)
  EXPECT_EQ(firebase::kFutureStatusPending, result.status());
  firebase::testing::cppsdk::TickerElapse();
#endif  // defined(FIREBASE_ANDROID_FOR_DESKTOP)
  EXPECT_EQ(firebase::kFutureStatusComplete, result.status());
}

TEST_F(RemoteConfigTest, Fetch) {
  // Default value: 43200seconds = 12hours
  REPORT_EXPECT("FirebaseRemoteConfig.fetch", "", {"43200"});
  firebase::testing::cppsdk::ConfigSet(
      "{"
      "  config:["
      "    {fake:'FirebaseRemoteConfig.fetch',"
      "     futuregeneric:{ticker:1}}"
      "  ]"
      "}");
  Verify(Fetch());
}

TEST_F(RemoteConfigTest, FetchWithException) {
  // Default value: 43200seconds = 12hours
  REPORT_EXPECT("FirebaseRemoteConfig.fetch", "", {"43200"});
  firebase::testing::cppsdk::ConfigSet(
      "{"
      "  config:["
      "    {fake:'FirebaseRemoteConfig.fetch',"
      "     futuregeneric:{throwexception:true,"
      "                    exceptionmsg:'fetch failed',"
      "                    ticker:1}}"
      "  ]"
      "}");
  Verify(Fetch());
}

TEST_F(RemoteConfigTest, FetchWithExpiration) {
  REPORT_EXPECT("FirebaseRemoteConfig.fetch", "", {"3600"});
  firebase::testing::cppsdk::ConfigSet(
      "{"
      "  config:["
      "    {fake:'FirebaseRemoteConfig.fetch',"
      "     futuregeneric:{ticker:1}}"
      "  ]"
      "}");
  Verify(Fetch(3600));
}

TEST_F(RemoteConfigTest, FetchWithExpirationAndException) {
  REPORT_EXPECT("FirebaseRemoteConfig.fetch", "", {"3600"});
  firebase::testing::cppsdk::ConfigSet(
      "{"
      "  config:["
      "    {fake:'FirebaseRemoteConfig.fetch',"
      "     futuregeneric:{throwexception:true,"
      "                    exceptionmsg:'fetch failed',"
      "                    ticker:1}}"
      "  ]"
      "}");
  Verify(Fetch(3600));
}

TEST_F(RemoteConfigTest, FetchLastResult) {
  REPORT_EXPECT("FirebaseRemoteConfig.fetch", "", {"3600"});
  firebase::testing::cppsdk::ConfigSet(
      "{"
      "  config:["
      "    {fake:'FirebaseRemoteConfig.fetch',"
      "     futuregeneric:{ticker:1}}"
      "  ]"
      "}");
  Future<void> result = Fetch(3600);
#if defined(FIREBASE_ANDROID_FOR_DESKTOP)
  EXPECT_EQ(firebase::kFutureStatusPending, result.status());
  EXPECT_EQ(firebase::kFutureStatusPending, FetchLastResult().status());
  firebase::testing::cppsdk::TickerElapse();
#endif  // defined(FIREBASE_ANDROID_FOR_DESKTOP)
  EXPECT_EQ(firebase::kFutureStatusComplete, result.status());
  EXPECT_EQ(firebase::kFutureStatusComplete, FetchLastResult().status());
}

TEST_F(RemoteConfigTest, FetchLastResultWithCallFetchTwice) {
  REPORT_EXPECT("FirebaseRemoteConfig.fetch", "", {"3600"});
  firebase::testing::cppsdk::ConfigSet(
      "{"
      "  config:["
      "    {fake:'FirebaseRemoteConfig.fetch',"
      "     futuregeneric:{ticker:1}}"
      "  ]"
      "}");
  Future<void> result1 = Fetch(3600);
#if defined(FIREBASE_ANDROID_FOR_DESKTOP)
  EXPECT_EQ(firebase::kFutureStatusPending, result1.status());
  EXPECT_EQ(firebase::kFutureStatusPending, FetchLastResult().status());
  firebase::testing::cppsdk::TickerElapse();
#endif  // defined(FIREBASE_ANDROID_FOR_DESKTOP)
  EXPECT_EQ(firebase::kFutureStatusComplete, result1.status());
  EXPECT_EQ(firebase::kFutureStatusComplete, FetchLastResult().status());

  firebase::testing::cppsdk::TickerReset();

  Future<void> result2 = Fetch(3600);
#if defined(FIREBASE_ANDROID_FOR_DESKTOP)
  EXPECT_EQ(firebase::kFutureStatusPending, result2.status());
  EXPECT_EQ(firebase::kFutureStatusPending, FetchLastResult().status());
  firebase::testing::cppsdk::TickerElapse();
#endif  // defined(FIREBASE_ANDROID_FOR_DESKTOP)
  EXPECT_EQ(firebase::kFutureStatusComplete, result2.status());
  EXPECT_EQ(firebase::kFutureStatusComplete, FetchLastResult().status());
}

TEST_F(RemoteConfigTest, ActivateFetchedTrue) {
  GTEST_SKIP(); // TODO(cynthiajiang): Re-implement the test with actual activate
  REPORT_EXPECT("FirebaseRemoteConfig.activate", "true", {});
  firebase::testing::cppsdk::ConfigSet(
      "{"
      "  config:["
      "    {fake:'FirebaseRemoteConfig.activate',"
      "     futurebool:{value:True, ticker:1}}"
      "  ]"
      "}");
  EXPECT_TRUE(ActivateFetched());
}

TEST_F(RemoteConfigTest, ActivateFetchedFalse) {
  GTEST_SKIP(); // TODO(cynthiajiang): Re-implement the test with actual activate
  REPORT_EXPECT("FirebaseRemoteConfig.activate", "false", {});
  firebase::testing::cppsdk::ConfigSet(
      "{"
      "  config:["
      "    {fake:'FirebaseRemoteConfig.activate',"
      "     futurebool:{value:False, ticker:1}}"
      "  ]"
      "}");
  EXPECT_FALSE(ActivateFetched());
}

TEST_F(RemoteConfigTest, GetInfo) {
  REPORT_EXPECT("FirebaseRemoteConfig.getInfo", "", {});
  REPORT_EXPECT("FirebaseRemoteConfigInfo.getFetchTimeMillis", "1000", {});
  REPORT_EXPECT("FirebaseRemoteConfigInfo.getLastFetchStatus", "2", {});
  firebase::testing::cppsdk::ConfigSet(
      "{"
      "  config:["
      "    {fake:'FirebaseRemoteConfigInfo.getFetchTimeMillis',"
      "     returnvalue: {'tlong': 1000}},"
      "    {fake:'FirebaseRemoteConfigInfo.getLastFetchStatus',"
      "     returnvalue: {'tint': 2}},"
      "  ]"
      "}");
  const ConfigInfo info = GetInfo();
  EXPECT_EQ(info.fetch_time, 1000);
  EXPECT_EQ(info.last_fetch_status, kLastFetchStatusFailure);
  EXPECT_EQ(info.last_fetch_failure_reason, kFetchFailureReasonThrottled);
}
}  // namespace remote_config
}  // namespace firebase
