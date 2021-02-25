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

#include "remote_config/src/desktop/remote_config_desktop.h"

#include <chrono>  // NOLINT

#include "file/base/path.h"
#include "firebase/app.h"
#include "app/tests/include/firebase/app_for_testing.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "firebase/future.h"
#include "remote_config/src/common.h"
#include "remote_config/src/desktop/config_data.h"
#include "remote_config/src/desktop/file_manager.h"
#include "remote_config/src/desktop/metadata.h"

namespace firebase {
namespace remote_config {
namespace internal {

class RemoteConfigDesktopTest : public ::testing::Test {
 protected:
  void SetUp() override {
    app_ = testing::CreateApp();

    FutureData::Create();
    file_manager_ = new RemoteConfigFileManager(
        file::JoinPath(FLAGS_test_tmpdir, "remote_config_data"));
    SetUpInstance();
  }

  void TearDown() override {
    delete instance_;
    delete configs_;
    delete file_manager_;
    FutureData::Destroy();
    delete app_;
  }

  // Remove previous instance and create the new one. New instance will load
  // data from file, so we need to create file with data.
  //
  // After calling this function the `instance->configs_` must to be equal to
  // the `configs_`.
  void SetUpInstance() {
    // !!! Remove previous instance at first, because Client can save data in
    // background when you will rewriting the same file.
    delete instance_;
    SetupContent();
    EXPECT_TRUE(file_manager_->Save(*configs_));
    instance_ = new RemoteConfigInternal(*app_, *file_manager_);
  }

  // Remove previous content and create the new one.
  void SetupContent() {
    uint64_t milliseconds_since_epoch =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count();

    // Set this timestamp to guarantee passing fetching conditions.
    NamespacedConfigData fetched(
        NamespaceKeyValueMap(
            {{"namespace2", {{"key1", "value1"}, {"key2", "value2"}}}}),
        milliseconds_since_epoch - 2 * 1000 * kDefaultCacheExpiration);
    NamespacedConfigData active(
        NamespaceKeyValueMap({{RemoteConfigInternal::kDefaultNamespace,
                               {{"key_bool", "f"},
                                {"key_long", "55555"},
                                {"key_double", "100.5"},
                                {"key_string", "aaa"},
                                {"key_data", "zzz"}}}}),
        1234567);
    NamespacedConfigData defaults(
        NamespaceKeyValueMap({}),
        9999999);
    RemoteConfigMetadata metadata;
    metadata.set_info(ConfigInfo({1498757224, kLastFetchStatusPending,
                                  kFetchFailureReasonThrottled, 1498758888}));
    metadata.set_digest_by_namespace(
        MetaDigestMap({{"namespace1", "digest1"}, {"namespace2", "digest2"}}));
    metadata.AddSetting(kConfigSettingDeveloperMode, "1");

    delete configs_;
    configs_ = new LayeredConfigs(fetched, active, defaults, metadata);
  }

  firebase::App* app_ = nullptr;

  RemoteConfigInternal* instance_ = nullptr;
  LayeredConfigs* configs_ = nullptr;
  RemoteConfigFileManager* file_manager_ = nullptr;
};

// Can't load `configs_` from file without permissions.
TEST_F(RemoteConfigDesktopTest, FailedLoadFromFile) {
  RemoteConfigInternal instance(
      *app_, RemoteConfigFileManager(
                 file::JoinPath(FLAGS_test_tmpdir, "not_found_file")));
  EXPECT_EQ(LayeredConfigs(), instance.configs_);
}

TEST_F(RemoteConfigDesktopTest, SuccessLoadFromFile) {
  EXPECT_EQ(*configs_, instance_->configs_);
}

// Check async saving working well.
TEST_F(RemoteConfigDesktopTest, SuccessAsyncSaveToFile) {
  // Let change the `configs_` variable.
  instance_->configs_.fetched = NamespacedConfigData(
      NamespaceKeyValueMap(
          {{"new_namespace1",
            {{"new_key1", "new_value1"}, {"new_key2", "new_value2"}}}}),
      999999);

  instance_->save_channel_.Put();

  // Need to wait until background thread will save `configs_` to the file.
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  LayeredConfigs new_content;
  EXPECT_TRUE(file_manager_->Load(&new_content));
  EXPECT_EQ(new_content, instance_->configs_);
}

TEST_F(RemoteConfigDesktopTest, SetDefaultsKeyValueVariant) {
  {
    SetUpInstance();

    Variant vector_variant;
    std::vector<Variant>* std_vector_variant =
        new std::vector<Variant>(1, Variant::FromMutableBlob("123", 4));
    vector_variant.AssignVector(&std_vector_variant);

    ConfigKeyValueVariant defaults[] = {
        ConfigKeyValueVariant{"key_bool", Variant(true)},
        ConfigKeyValueVariant{"key_blob",
                              Variant::FromMutableBlob("123456789", 9)},
        ConfigKeyValueVariant{"key_string", Variant("black")},
        ConfigKeyValueVariant{"key_long", Variant(120)},
        ConfigKeyValueVariant{"key_double", Variant(600.5)},
        // Will be ignored, this type is not supported.
        ConfigKeyValueVariant{"key_vector_variant", vector_variant}};

    instance_->SetDefaults(defaults, 6);
    configs_->defaults.SetNamespace(
        {
            {"key_bool", "true"},
            {"key_blob", "123456789"},
            {"key_string", "black"},
            {"key_long", "120"},
            {"key_double", "600.5000000000000000"},
        },
        RemoteConfigInternal::kDefaultNamespace);
    EXPECT_EQ(*configs_, instance_->configs_);
  }
  {
    SetUpInstance();
    // `defaults` contains two keys `height`. The last one must to be applied.
    ConfigKeyValueVariant defaults[] = {
        ConfigKeyValueVariant{"height", Variant(100)},
        ConfigKeyValueVariant{"height", Variant(500)},
        ConfigKeyValueVariant{"width", Variant("120cm")}};
    instance_->SetDefaults(defaults, 3);
    configs_->defaults.SetNamespace({{"height", "500"}, {"width", "120cm"}},
                                    RemoteConfigInternal::kDefaultNamespace);
    EXPECT_EQ(*configs_, instance_->configs_);
  }
}

TEST_F(RemoteConfigDesktopTest, SetDefaultsKeyValue) {
  {
    SetUpInstance();
    ConfigKeyValue defaults[] = {ConfigKeyValue{"height", "100"},
                                 ConfigKeyValue{"height", "500"},
                                 ConfigKeyValue{"width", "120cm"}};
    instance_->SetDefaults(defaults, 3);
    configs_->defaults.SetNamespace({{"height", "500"}, {"width", "120cm"}},
                                    RemoteConfigInternal::kDefaultNamespace);
    EXPECT_EQ(*configs_, instance_->configs_);
  }
  {
    SetUpInstance();
    ConfigKeyValue defaults[] = {ConfigKeyValue{"height", "100"},
                                 ConfigKeyValue{"height", "500"},
                                 ConfigKeyValue{"width", "120cm"}};
    instance_->SetDefaults(defaults, 3);
    configs_->defaults.SetNamespace({{"height", "500"}, {"width", "120cm"}},
                                    RemoteConfigInternal::kDefaultNamespace);
    EXPECT_EQ(*configs_, instance_->configs_);
  }
}

TEST_F(RemoteConfigDesktopTest, GetAndSetConfigSetting) {
  EXPECT_EQ(instance_->GetConfigSetting(kConfigSettingDeveloperMode), "1");
  instance_->SetConfigSetting(kConfigSettingDeveloperMode, "0");
  EXPECT_EQ(instance_->GetConfigSetting(kConfigSettingDeveloperMode), "0");
}

TEST_F(RemoteConfigDesktopTest, GetBoolean) {
  { EXPECT_FALSE(instance_->GetBoolean("key_bool", nullptr)); }
  {
    ValueInfo info;
    EXPECT_FALSE(instance_->GetBoolean("key_bool", &info));
    EXPECT_TRUE(info.conversion_successful);
    EXPECT_EQ(info.source, kValueSourceRemoteValue);
  }
}

TEST_F(RemoteConfigDesktopTest, GetLong) {
  { EXPECT_EQ(instance_->GetLong("key_long", nullptr), 55555); }
  {
    ValueInfo info;
    EXPECT_EQ(instance_->GetLong("key_long", &info), 55555);
    EXPECT_TRUE(info.conversion_successful);
    EXPECT_EQ(info.source, kValueSourceRemoteValue);
  }
}

TEST_F(RemoteConfigDesktopTest, GetDouble) {
  { EXPECT_EQ(instance_->GetDouble("key_double", nullptr), 100.5); }
  {
    ValueInfo info;
    EXPECT_EQ(instance_->GetDouble("key_double", &info), 100.5);
    EXPECT_TRUE(info.conversion_successful);
    EXPECT_EQ(info.source, kValueSourceRemoteValue);
  }
}

TEST_F(RemoteConfigDesktopTest, GetString) {
  { EXPECT_EQ(instance_->GetString("key_string", nullptr), "aaa"); }
  {
    ValueInfo info;
    EXPECT_EQ(instance_->GetString("key_string", &info), "aaa");
    EXPECT_TRUE(info.conversion_successful);
    EXPECT_EQ(info.source, kValueSourceRemoteValue);
  }
}

TEST_F(RemoteConfigDesktopTest, GetData) {
  {
    EXPECT_THAT(instance_->GetData("key_data", nullptr),
                ::testing::Eq(std::vector<unsigned char>{'z', 'z', 'z'}));
  }
  {
    ValueInfo info;
    EXPECT_THAT(instance_->GetData("key_data", &info),
                ::testing::Eq(std::vector<unsigned char>{'z', 'z', 'z'}));
    EXPECT_TRUE(info.conversion_successful);
    EXPECT_EQ(info.source, kValueSourceRemoteValue);
  }
}

TEST_F(RemoteConfigDesktopTest, GetKeys) {
  {
    EXPECT_THAT(
        instance_->GetKeys(),
        ::testing::Eq(std::vector<std::string>{
            "key_bool", "key_data", "key_double", "key_long", "key_string"}));
  }
}

TEST_F(RemoteConfigDesktopTest, GetKeysByPrefix) {
  {
    EXPECT_THAT(
        instance_->GetKeysByPrefix("key"),
        ::testing::Eq(std::vector<std::string>{
            "key_bool", "key_data", "key_double", "key_long", "key_string"}));
  }
  {
    EXPECT_THAT(
        instance_->GetKeysByPrefix("key_d"),
        ::testing::Eq(std::vector<std::string>{"key_data", "key_double"}));
  }
}

TEST_F(RemoteConfigDesktopTest, GetInfo) {
  ConfigInfo info = instance_->GetInfo();
  EXPECT_EQ(info.fetch_time, 1498757224);
  EXPECT_EQ(info.last_fetch_status, kLastFetchStatusPending);
  EXPECT_EQ(info.last_fetch_failure_reason, kFetchFailureReasonThrottled);
  EXPECT_EQ(info.throttled_end_time, 1498758888);
}

TEST_F(RemoteConfigDesktopTest, ActivateFetched) {
  {
    SetUpInstance();

    instance_->configs_.fetched = NamespacedConfigData();
    instance_->configs_.active = NamespacedConfigData(
        NamespaceKeyValueMap({{"namespace:active", {{"key", "aaa"}}}}), 999999);

    // Will not activate, because the `fetched` configs is empty.
    EXPECT_FALSE(instance_->ActivateFetched());
  }
  {
    SetUpInstance();

    instance_->configs_.fetched = NamespacedConfigData(
        NamespaceKeyValueMap({{"namespace", {{"key", "aaa"}}}}), 999999);
    instance_->configs_.active = NamespacedConfigData(
        NamespaceKeyValueMap({{"namespace", {{"key", "aaa"}}}}), 999999);

    // Will not activate, because the `fetched` configs equal to the `active`
    // configs, they have the same timestamp.
    EXPECT_FALSE(instance_->ActivateFetched());
  }
  {
    SetUpInstance();
    instance_->configs_.fetched = NamespacedConfigData(
        NamespaceKeyValueMap({{"namespace:fetched", {{"key1", "aaa"}}}}),
        9999999999);
    instance_->configs_.active = NamespacedConfigData(
        NamespaceKeyValueMap({{"namespace:active", {{"key2", "zzz"}}}}),
        999999);

    // Will activate, because the `fetched` configs timestamp more than the
    // `active` configs timestamp.
    EXPECT_TRUE(instance_->ActivateFetched());
    EXPECT_EQ(instance_->configs_.fetched, instance_->configs_.active);
  }
}

TEST_F(RemoteConfigDesktopTest, Fetch) {
  // Use fake rest implementation. In fake we just return some other metadata
  // and fetched config and don't make HTTP requests. In this test case want
  // make sure that all updated values apply correctly.
  //
  // See rest_fake.cc for more details.
  {
    SetUpInstance();
    instance_->Fetch(0);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_EQ(instance_->configs_.fetched,
              NamespacedConfigData(
                  NamespaceKeyValueMap({{"namespace", {{"key", "value"}}}}),
                  1000000));

    EXPECT_EQ(instance_->configs_.metadata.digest_by_namespace(),
              MetaDigestMap({{"namespace", "digest"}}));

    ConfigInfo info = instance_->configs_.metadata.info();
    EXPECT_EQ(info.fetch_time, 0);
    EXPECT_EQ(info.last_fetch_status, kLastFetchStatusSuccess);
    EXPECT_EQ(info.last_fetch_failure_reason, kFetchFailureReasonError);
    EXPECT_EQ(info.throttled_end_time, 0);

    EXPECT_EQ(
        instance_->configs_.metadata.GetSetting(kConfigSettingDeveloperMode),
        "1");
  }
  {
    // Will fetch, because cache_expiration_in_seconds == 0.
    SetUpInstance();
    Future<void> future = instance_->Fetch(0);
    EXPECT_EQ(future.status(), firebase::kFutureStatusPending);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ(future.status(), firebase::kFutureStatusComplete);
  }
  {
    // Will fetch, because cache is older than cache_expiration_in_seconds.
    // We setup fetch.timestamp as
    // milliseconds_since_epoch - 2*1000*cache_expiration_in_seconds;
    SetUpInstance();
    Future<void> future = instance_->Fetch(kDefaultCacheExpiration);
    EXPECT_EQ(future.status(), firebase::kFutureStatusPending);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ(future.status(), firebase::kFutureStatusComplete);
  }
  {
    // Will NOT fetch, because cache is newer than kDefaultCacheExpiration
    SetUpInstance();
    Future<void> future = instance_->Fetch(10 * kDefaultCacheExpiration);
    EXPECT_EQ(future.status(), firebase::kFutureStatusComplete);
  }
}

TEST_F(RemoteConfigDesktopTest, TestIsBoolTrue) {
  // Confirm all the values that ARE BoolTrue.
  EXPECT_TRUE(RemoteConfigInternal::IsBoolTrue("1"));
  EXPECT_TRUE(RemoteConfigInternal::IsBoolTrue("true"));
  EXPECT_TRUE(RemoteConfigInternal::IsBoolTrue("t"));
  EXPECT_TRUE(RemoteConfigInternal::IsBoolTrue("on"));
  EXPECT_TRUE(RemoteConfigInternal::IsBoolTrue("yes"));
  EXPECT_TRUE(RemoteConfigInternal::IsBoolTrue("y"));

  // Ensure all the BoolFalse values are not BoolTrue.
  EXPECT_FALSE(RemoteConfigInternal::IsBoolTrue("0"));
  EXPECT_FALSE(RemoteConfigInternal::IsBoolTrue("false"));
  EXPECT_FALSE(RemoteConfigInternal::IsBoolTrue("f"));
  EXPECT_FALSE(RemoteConfigInternal::IsBoolTrue("no"));
  EXPECT_FALSE(RemoteConfigInternal::IsBoolTrue("n"));
  EXPECT_FALSE(RemoteConfigInternal::IsBoolTrue("off"));

  // Confirm a few random values.
  EXPECT_FALSE(RemoteConfigInternal::IsBoolTrue("apple"));
  EXPECT_FALSE(RemoteConfigInternal::IsBoolTrue("Yes"));  // lower case only
  EXPECT_FALSE(
      RemoteConfigInternal::IsBoolTrue("100"));  // only the number 1 exactly
  EXPECT_FALSE(
      RemoteConfigInternal::IsBoolTrue("-1"));  // only the number 1 exactly
  EXPECT_FALSE(RemoteConfigInternal::IsBoolTrue("1.0"));
  EXPECT_FALSE(RemoteConfigInternal::IsBoolTrue("True"));   // lower-case only
  EXPECT_FALSE(RemoteConfigInternal::IsBoolTrue("False"));  // lower-case only
  EXPECT_FALSE(RemoteConfigInternal::IsBoolTrue("N"));      // lower-case only
}

TEST_F(RemoteConfigDesktopTest, TestIsBoolFalse) {
  // Ensure all the BoolFalse values are not BoolTrue.
  EXPECT_TRUE(RemoteConfigInternal::IsBoolFalse("0"));
  EXPECT_TRUE(RemoteConfigInternal::IsBoolFalse("false"));
  EXPECT_TRUE(RemoteConfigInternal::IsBoolFalse("f"));
  EXPECT_TRUE(RemoteConfigInternal::IsBoolFalse("no"));
  EXPECT_TRUE(RemoteConfigInternal::IsBoolFalse("n"));
  EXPECT_TRUE(RemoteConfigInternal::IsBoolFalse("off"));

  // Confirm that the BoolTrue values are not BoolFalse.
  EXPECT_FALSE(RemoteConfigInternal::IsBoolFalse("1"));
  EXPECT_FALSE(RemoteConfigInternal::IsBoolFalse("true"));
  EXPECT_FALSE(RemoteConfigInternal::IsBoolFalse("t"));
  EXPECT_FALSE(RemoteConfigInternal::IsBoolFalse("on"));
  EXPECT_FALSE(RemoteConfigInternal::IsBoolFalse("yes"));
  EXPECT_FALSE(RemoteConfigInternal::IsBoolFalse("y"));

  // Confirm a few random values.
  EXPECT_FALSE(RemoteConfigInternal::IsBoolFalse("apple"));
  EXPECT_FALSE(RemoteConfigInternal::IsBoolFalse("Yes"));  // lower case only
  EXPECT_FALSE(
      RemoteConfigInternal::IsBoolFalse("100"));  // only the number 1 exactly
  EXPECT_FALSE(
      RemoteConfigInternal::IsBoolFalse("-1"));  // only the number 1 exactly
  EXPECT_FALSE(RemoteConfigInternal::IsBoolFalse("1.0"));
  EXPECT_FALSE(RemoteConfigInternal::IsBoolFalse("True"));   // lower-case only
  EXPECT_FALSE(RemoteConfigInternal::IsBoolFalse("False"));  // lower-case only
  EXPECT_FALSE(RemoteConfigInternal::IsBoolFalse("N"));      // lower-case only
}

TEST_F(RemoteConfigDesktopTest, TestIsLong) {
  EXPECT_TRUE(RemoteConfigInternal::IsLong("0"));
  EXPECT_TRUE(RemoteConfigInternal::IsLong("1"));
  EXPECT_TRUE(RemoteConfigInternal::IsLong("2"));
  EXPECT_TRUE(RemoteConfigInternal::IsLong("+0"));
  EXPECT_TRUE(RemoteConfigInternal::IsLong("+3"));
  EXPECT_TRUE(RemoteConfigInternal::IsLong("-5"));
  EXPECT_TRUE(RemoteConfigInternal::IsLong("8249"));
  EXPECT_TRUE(RemoteConfigInternal::IsLong("-718129"));
  EXPECT_TRUE(RemoteConfigInternal::IsLong("+9173923192819"));

  EXPECT_FALSE(RemoteConfigInternal::IsLong("0.0"));
  EXPECT_FALSE(RemoteConfigInternal::IsLong(" 5"));
  EXPECT_FALSE(RemoteConfigInternal::IsLong("9 "));
  EXPECT_FALSE(RemoteConfigInternal::IsLong("- 8"));
  EXPECT_FALSE(RemoteConfigInternal::IsLong("-0-"));
  EXPECT_FALSE(RemoteConfigInternal::IsLong("-+0"));
  EXPECT_FALSE(RemoteConfigInternal::IsLong("0-0"));
  EXPECT_FALSE(RemoteConfigInternal::IsLong("1-1"));
  EXPECT_FALSE(RemoteConfigInternal::IsLong("12345+"));
  EXPECT_FALSE(RemoteConfigInternal::IsLong("12345-"));
  EXPECT_FALSE(RemoteConfigInternal::IsLong("12345abc"));
  EXPECT_FALSE(RemoteConfigInternal::IsLong("++81020"));
  EXPECT_FALSE(RemoteConfigInternal::IsLong("--32391"));
  EXPECT_FALSE(RemoteConfigInternal::IsLong("2+2=4"));
  EXPECT_FALSE(RemoteConfigInternal::IsLong("234,456"));
  EXPECT_FALSE(RemoteConfigInternal::IsLong("234.1"));
  EXPECT_FALSE(RemoteConfigInternal::IsLong("829.0"));
  EXPECT_FALSE(RemoteConfigInternal::IsLong("1e100"));
  EXPECT_FALSE(RemoteConfigInternal::IsLong(""));
  EXPECT_FALSE(RemoteConfigInternal::IsLong(" "));
}

TEST_F(RemoteConfigDesktopTest, TestIsDouble) {
  EXPECT_TRUE(RemoteConfigInternal::IsDouble("0"));
  EXPECT_TRUE(RemoteConfigInternal::IsDouble("1"));
  EXPECT_TRUE(RemoteConfigInternal::IsDouble("2"));
  EXPECT_TRUE(RemoteConfigInternal::IsDouble("+0"));
  EXPECT_TRUE(RemoteConfigInternal::IsDouble("+3"));
  EXPECT_TRUE(RemoteConfigInternal::IsDouble("-5"));
  EXPECT_TRUE(RemoteConfigInternal::IsDouble("1."));
  EXPECT_TRUE(RemoteConfigInternal::IsDouble("8249"));
  EXPECT_TRUE(RemoteConfigInternal::IsDouble("-718129"));
  EXPECT_TRUE(RemoteConfigInternal::IsDouble("+9173923192819"));

  EXPECT_TRUE(RemoteConfigInternal::IsDouble("1e10"));
  EXPECT_TRUE(RemoteConfigInternal::IsDouble("1.2e9729"));
  EXPECT_TRUE(RemoteConfigInternal::IsDouble("48.3e-39"));
  EXPECT_TRUE(RemoteConfigInternal::IsDouble(".4e+9"));
  EXPECT_TRUE(RemoteConfigInternal::IsDouble("-.289e11"));
  EXPECT_TRUE(RemoteConfigInternal::IsDouble("-7293e+72"));
  EXPECT_TRUE(RemoteConfigInternal::IsDouble("+489e322"));
  EXPECT_TRUE(RemoteConfigInternal::IsDouble("10E10"));
  EXPECT_TRUE(RemoteConfigInternal::IsDouble("10E-10"));
  EXPECT_TRUE(RemoteConfigInternal::IsDouble("-10E+10"));
  EXPECT_TRUE(RemoteConfigInternal::IsDouble("+10E-10"));

  EXPECT_FALSE(RemoteConfigInternal::IsDouble("1.2e"));
  EXPECT_FALSE(RemoteConfigInternal::IsDouble("1.9.2"));
  EXPECT_FALSE(RemoteConfigInternal::IsDouble("1.3e8e2"));
  EXPECT_FALSE(RemoteConfigInternal::IsDouble("-13-e8"));
  EXPECT_FALSE(RemoteConfigInternal::IsDouble("98e4.3"));
  EXPECT_FALSE(RemoteConfigInternal::IsDouble(" 1"));
  EXPECT_FALSE(RemoteConfigInternal::IsDouble("8 "));
  EXPECT_FALSE(RemoteConfigInternal::IsDouble("56.8f-29"));
  EXPECT_FALSE(RemoteConfigInternal::IsDouble("-793e+89apple"));
  EXPECT_FALSE(RemoteConfigInternal::IsDouble("489EEE"));
  EXPECT_FALSE(RemoteConfigInternal::IsDouble("489EEE123"));
  EXPECT_FALSE(RemoteConfigInternal::IsDouble(""));
  EXPECT_FALSE(RemoteConfigInternal::IsDouble(" "));
  EXPECT_FALSE(RemoteConfigInternal::IsDouble("e"));
  EXPECT_FALSE(RemoteConfigInternal::IsDouble("."));
}

}  // namespace internal
}  // namespace remote_config
}  // namespace firebase
