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

#include "remote_config/src/desktop/metadata.h"
#include "remote_config/src/include/firebase/remote_config.h"
#include "flatbuffers/flexbuffers.h"

namespace firebase {
namespace remote_config {
namespace internal {

RemoteConfigMetadata::RemoteConfigMetadata()
    : info_({0, kLastFetchStatusSuccess, kFetchFailureReasonInvalid, 0}) {}

std::string RemoteConfigMetadata::Serialize() const {
  flexbuffers::Builder fbb;
  // Write out the struct members as a map.
  fbb.Map([&]() {
    // ConfigInfo struct
    fbb.Map("info", [&]() {
      fbb.UInt("fetch_time", info_.fetch_time);
      // LastFetchStatus enum
      fbb.Int("last_fetch_status", static_cast<int>(info_.last_fetch_status));
      // FetchFailureReason enum
      fbb.Int("last_fetch_failure_reason",
              static_cast<int>(info_.last_fetch_failure_reason));
      fbb.UInt("throttled_end_time", info_.throttled_end_time);
    });

    fbb.Add("digest_by_namespace", digest_by_namespace_);

    fbb.Map("settings", [&]() {
      for (const auto& setting : settings_) {
        fbb.String(std::to_string(setting.first).c_str(), setting.second);
      }
    });
  });
  fbb.Finish();
  const std::vector<uint8_t>& buffer = fbb.GetBuffer();
  return std::string(buffer.cbegin(), buffer.cend());
}

void RemoteConfigMetadata::Deserialize(const std::string& buffer) {
  const uint8_t* data = reinterpret_cast<const uint8_t*>(buffer.data());
  size_t size = buffer.size();
  auto struct_map = flexbuffers::GetRoot(data, size).AsMap();

  flexbuffers::Map info = struct_map["info"].AsMap();
  info_.fetch_time = info["fetch_time"].AsUInt64();
  info_.last_fetch_status =
      static_cast<LastFetchStatus>(info["last_fetch_status"].AsInt32());
  info_.last_fetch_failure_reason = static_cast<FetchFailureReason>(
      info["last_fetch_failure_reason"].AsInt32());
  info_.throttled_end_time = info["throttled_end_time"].AsUInt64();

  digest_by_namespace_.clear();
  flexbuffers::Map digests = struct_map["digest_by_namespace"].AsMap();
  DeserializeMap(&digest_by_namespace_, digests);

  settings_.clear();
  flexbuffers::Map settings = struct_map["settings"].AsMap();
  for (int i = 0, n = settings.size(); i < n; ++i) {
    int int_key = std::stoi(settings.Keys()[i].AsKey());
    settings_[static_cast<ConfigSetting>(int_key)] =
        settings.Values()[i].AsString().c_str();
  }
}

void RemoteConfigMetadata::AddSetting(const ConfigSetting& setting,
                                      const std::string& value) {
  settings_[setting] = value;
}

// Now we use only kConfigSettingDeveloperMode for settings
// "0" means that developer mode is disable, "1" means developer mode is enable
std::string RemoteConfigMetadata::GetSetting(
    const ConfigSetting& setting) const {
  auto it = settings_.find(setting);
  return it == settings_.end() ? "0" : it->second;
}

bool RemoteConfigMetadata::operator==(const RemoteConfigMetadata& right) const {
  return digest_by_namespace_ == right.digest_by_namespace_ &&
         settings_ == right.settings_ &&
         info_.fetch_time == right.info_.fetch_time &&
         info_.last_fetch_status == right.info_.last_fetch_status &&
         info_.last_fetch_failure_reason ==
             right.info_.last_fetch_failure_reason &&
         info_.throttled_end_time == right.info_.throttled_end_time;
}

}  // namespace internal
}  // namespace remote_config
}  // namespace firebase
