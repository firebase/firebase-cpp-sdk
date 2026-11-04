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

#include "remote_config/src/desktop/file_manager.h"

#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <system_error>

#include "gtest/gtest.h"
#include "remote_config/src/desktop/config_data.h"
#include "remote_config/src/desktop/metadata.h"

namespace firebase {
namespace remote_config {
namespace internal {

TEST(RemoteConfigFileManagerTest, SaveAndLoadSuccess) {
  std::filesystem::path file_path =
      std::filesystem::path(::testing::TempDir()) /
      "remote_config_file_manager_test_data";
  std::error_code ec;
  std::filesystem::remove(file_path, ec);

  RemoteConfigFileManager file_manager(file_path.string());
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

  EXPECT_TRUE(file_manager.Save(configs));

  LayeredConfigs new_configs;
  EXPECT_TRUE(file_manager.Load(&new_configs));
  EXPECT_EQ(configs, new_configs);

  std::filesystem::remove(file_path, ec);
}

TEST(RemoteConfigFileManagerTest, LoadMissingOrCorruptFileFails) {
  std::filesystem::path file_path =
      std::filesystem::path(::testing::TempDir()) /
      "remote_config_file_manager_corrupt_test";
  std::error_code ec;
  std::filesystem::remove(file_path, ec);

  RemoteConfigFileManager file_manager(file_path.string());
  LayeredConfigs configs;
  EXPECT_FALSE(file_manager.Load(&configs));
  EXPECT_FALSE(file_manager.Load(nullptr));

  {
    std::ofstream out(file_path, std::ios::binary | std::ios::trunc);
    out << "";
  }
  EXPECT_FALSE(file_manager.Load(&configs));

  std::filesystem::remove(file_path, ec);
}

}  // namespace internal
}  // namespace remote_config
}  // namespace firebase
