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

#include <map>
#include <string>

#include "testing/base/public/googletest.h"
#include "gtest/gtest.h"

#include "file/base/path.h"
#include "remote_config/src/desktop/config_data.h"
#include "remote_config/src/desktop/file_manager.h"
#include "remote_config/src/desktop/metadata.h"

namespace firebase {
namespace remote_config {
namespace internal {

TEST(RemoteConfigFileManagerTest, SaveAndLoadSuccess) {
  std::string file_path =
      file::JoinPath(FLAGS_test_tmpdir, "remote_config_data");

  RemoteConfigFileManager file_manager(file_path);
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
}

}  // namespace internal
}  // namespace remote_config
}  // namespace firebase
