// Copyright 2018 Google LLC
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

#include "remote_config/src/desktop/config_data.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"

namespace firebase {
namespace remote_config {
namespace internal {

TEST(LayeredConfigsTest, Convertation) {
  NamespacedConfigData fetched(
      NamespaceKeyValueMap(
          {{"namespace1", {{"key1", "value1"}, {"key2", "value2"}}}}),
      1234567);
  NamespacedConfigData active(
      NamespaceKeyValueMap(
          {{"namespace2", {{"key1", "value1"}, {"key2", "value2"}}}}),
      5555555);
  NamespacedConfigData defaults(
      NamespaceKeyValueMap(
          {{"namespace3", {{"key1", "value1"}, {"key2", "value2"}}}}),
      9999999);

  RemoteConfigMetadata metadata;
  metadata.set_info(ConfigInfo({1498757224, kLastFetchStatusPending,
                                kFetchFailureReasonThrottled, 1498758888}));
  metadata.set_digest_by_namespace(
      MetaDigestMap({{"namespace1", "digest1"}, {"namespace2", "digest2"}}));
  metadata.AddSetting(kConfigSettingDeveloperMode, "0");

  LayeredConfigs configs(fetched, active, defaults, metadata);
  std::string buffer = configs.Serialize();
  LayeredConfigs new_configs;
  new_configs.Deserialize(buffer);

  EXPECT_EQ(configs, new_configs);
}

TEST(NamespacedConfigDataTest, ConversionToFlexbuffer) {
  NamespacedConfigData config_data(
      NamespaceKeyValueMap(
          {{"namespace1", {{"key1", "value1"}, {"key2", "value2"}}}}),
      1234567);

  // Serialize the data to a string
  std::string buffer = config_data.Serialize();

  // Make a new config and deserialize it with the string.
  NamespacedConfigData new_config_data;
  new_config_data.Deserialize(buffer);

  EXPECT_EQ(config_data, new_config_data);
}

TEST(NamespacedConfigDataTest, DefaultConstructor) {
  NamespacedConfigData holder1;
  NamespacedConfigData holder2(NamespaceKeyValueMap(), 0);
  EXPECT_EQ(holder1, holder2);
}

TEST(NamespacedConfigDataTest, SetNamespace) {
  NamespaceKeyValueMap m({{"namespace1", {{"key1", "value1"}}}});
  NamespacedConfigData holder(m, 0);
  EXPECT_EQ(holder.GetValue("key1", "namespace1"), "value1");

  holder.SetNamespace(std::map<std::string, std::string>({{"key2", "value2"}}),
                      "namespace1");

  EXPECT_FALSE(holder.HasValue("key1", "namespace1"));
  EXPECT_EQ(holder.GetValue("key2", "namespace1"), "value2");
}

TEST(NamespacedConfigDataTest, HasValue) {
  NamespaceKeyValueMap m({{"namespace1", {{"key1", "value1"}}}});
  NamespacedConfigData holder(m, 0);
  EXPECT_TRUE(holder.HasValue("key1", "namespace1"));
  EXPECT_FALSE(holder.HasValue("key2", "namespace1"));
  EXPECT_FALSE(holder.HasValue("key3", "namespace2"));
}

TEST(NamespacedConfigDataTest, HasValueEmpty) {
  NamespacedConfigData holder(NamespaceKeyValueMap(), 0);
  EXPECT_FALSE(holder.HasValue("key1", "namespace1"));
  EXPECT_FALSE(holder.HasValue("key2", "namespace1"));
  EXPECT_FALSE(holder.HasValue("key1", "namespace2"));
  EXPECT_FALSE(holder.HasValue("key3", "namespace3"));
}

TEST(NamespacedConfigDataTest, GetValue) {
  NamespaceKeyValueMap m({{"namespace1", {{"key1", "value1"}}}});
  NamespacedConfigData holder(m, 0);
  EXPECT_EQ(holder.GetValue("key1", "namespace1"), "value1");
  EXPECT_EQ(holder.GetValue("key2", "namespace1"), "");
  EXPECT_EQ(holder.GetValue("key3", "namespace2"), "");
  EXPECT_EQ(holder.GetValue("key4", "namespace2"), "");
}

TEST(NamespacedConfigDataTest, GetValueEmpty) {
  NamespacedConfigData holder(NamespaceKeyValueMap(), 0);
  EXPECT_EQ(holder.GetValue("key1", "namespace1"), "");
  EXPECT_EQ(holder.GetValue("key2", "namespace2"), "");
}

TEST(NamespacedConfigDataTest, GetKeysByPrefix) {
  NamespaceKeyValueMap m(
      {{"namespace1",
        {{"key1", "value1"}, {"key2", "value2"}, {"key3", "value3"}}}});
  NamespacedConfigData holder(m, 0);
  std::set<std::string> keys;
  holder.GetKeysByPrefix("key", "namespace1", &keys);
  EXPECT_THAT(keys, ::testing::UnorderedElementsAre("key1", "key2", "key3"));
  keys.clear();

  holder.GetKeysByPrefix("", "namespace1", &keys);
  EXPECT_THAT(keys, ::testing::UnorderedElementsAre("key1", "key2", "key3"));
  keys.clear();

  holder.GetKeysByPrefix("some_other_key", "namespace1", &keys);
  EXPECT_THAT(keys, ::testing::UnorderedElementsAre());
  keys.clear();

  holder.GetKeysByPrefix("some_prefix", "namespace2", &keys);
  EXPECT_THAT(keys, ::testing::UnorderedElementsAre());
  keys.clear();
}

TEST(NamespacedConfigDataTest, GetConfig) {
  NamespaceKeyValueMap m(
      {{"namespace1",
        {{"key1", "value1"}, {"key2", "value2"}, {"key3", "value3"}}}});
  NamespacedConfigData holder(m, 1498757224);
  EXPECT_EQ(holder.config(), m);
}

TEST(NamespacedConfigDataTest, GetTimestamp) {
  NamespaceKeyValueMap m({{"namespace1", {{"key1", "value1"}}}});
  NamespacedConfigData holder(m, 1498757224);
  EXPECT_EQ(holder.timestamp(), 1498757224);
}

}  // namespace internal
}  // namespace remote_config
}  // namespace firebase
