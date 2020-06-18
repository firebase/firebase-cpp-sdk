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

#include "gtest/gtest.h"

#include "remote_config/src/desktop/metadata.h"
#include "remote_config/src/include/firebase/remote_config.h"

namespace firebase {
namespace remote_config {
namespace internal {

void ExpectEqualConfigInfo(const ConfigInfo& l, const ConfigInfo& r) {
  EXPECT_EQ(l.fetch_time, r.fetch_time);
  EXPECT_EQ(l.last_fetch_status, r.last_fetch_status);
  EXPECT_EQ(l.last_fetch_failure_reason, r.last_fetch_failure_reason);
  EXPECT_EQ(l.throttled_end_time, r.throttled_end_time);
}

TEST(RemoteConfigMetadataTest, Serialization) {
  RemoteConfigMetadata remote_config_metadata;
  remote_config_metadata.set_info(
      ConfigInfo({1498757224, kLastFetchStatusPending,
                  kFetchFailureReasonThrottled, 1498758888}));
  remote_config_metadata.set_digest_by_namespace(
      MetaDigestMap({{"namespace1", "digest1"}, {"namespace2", "digest2"}}));
  remote_config_metadata.AddSetting(kConfigSettingDeveloperMode, "0");

  std::string buffer = remote_config_metadata.Serialize();
  RemoteConfigMetadata new_remote_config_metadata;
  new_remote_config_metadata.Deserialize(buffer);

  EXPECT_EQ(remote_config_metadata, new_remote_config_metadata);
}

TEST(RemoteConfigMetadataTest, GetInfoDefaultValues) {
  RemoteConfigMetadata m;
  ExpectEqualConfigInfo(m.info(), ConfigInfo({0, kLastFetchStatusSuccess,
                                              kFetchFailureReasonInvalid, 0}));
}

TEST(RemoteConfigMetadataTest, SetAndGetInfo) {
  ConfigInfo info = {1498757224, kLastFetchStatusPending,
                     kFetchFailureReasonThrottled, 1498758888};
  RemoteConfigMetadata m;
  m.set_info(info);
  ExpectEqualConfigInfo(m.info(), info);
}

TEST(RemoteConfigMetadataTest, SetAndGetDigest) {
  MetaDigestMap digest({{"namespace1", "digest1"}, {"namespace2", "digest2"}});

  RemoteConfigMetadata m;
  m.set_digest_by_namespace(digest);

  EXPECT_EQ(m.digest_by_namespace(), digest);
}

TEST(RemoteConfigMetadataTest, SetAndGetSetting) {
  RemoteConfigMetadata m;
  EXPECT_EQ(m.GetSetting(kConfigSettingDeveloperMode), "0");

  m.AddSetting(kConfigSettingDeveloperMode, "0");
  EXPECT_EQ(m.GetSetting(kConfigSettingDeveloperMode), "0");

  m.AddSetting(kConfigSettingDeveloperMode, "1");
  EXPECT_EQ(m.GetSetting(kConfigSettingDeveloperMode), "1");
}

TEST(RemoteConfigMetadataTest, SetAndsettings) {
  RemoteConfigMetadata m;

  std::map<ConfigSetting, std::string> map;
  EXPECT_EQ(m.settings(), map);

  m.AddSetting(kConfigSettingDeveloperMode, "0");
  map[kConfigSettingDeveloperMode] = "0";
  EXPECT_EQ(m.settings(), map);

  m.AddSetting(kConfigSettingDeveloperMode, "1");
  map[kConfigSettingDeveloperMode] = "1";
  EXPECT_EQ(m.settings(), map);
}

}  // namespace internal
}  // namespace remote_config
}  // namespace firebase
