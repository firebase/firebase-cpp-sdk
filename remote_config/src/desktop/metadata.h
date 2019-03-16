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

#ifndef FIREBASE_REMOTE_CONFIG_CLIENT_CPP_SRC_DESKTOP_METADATA_H_
#define FIREBASE_REMOTE_CONFIG_CLIENT_CPP_SRC_DESKTOP_METADATA_H_

#include <map>
#include <string>

#include "remote_config/src/include/firebase/remote_config.h"
#include "flatbuffers/flexbuffers.h"

namespace firebase {
namespace remote_config {
namespace internal {

typedef std::map<std::string, std::string> MetaDigestMap;
typedef std::map<ConfigSetting, std::string> MetaSettingsMap;

// Contains different data about Remote Config Client.
//
// `RemoteConfigMetadata` has `proto::Metadata` analogue to keep data in file.
// Has convertors on both sides.
// Receives data from the response via:
//  * ConfigInfo: Public struct with information about the result itself, like
//                timestamp, status, and potentially error info.
//  * settings map: corresponds to a single supported setting, "developer mode"
//  * digest map: Server computed digest (hash) of the config entries, stored
//                per config namespace.
class RemoteConfigMetadata {
 public:
  RemoteConfigMetadata();

  std::string Serialize() const;
  void Deserialize(const std::string& buffer);

  const ConfigInfo& info() const { return info_; }
  void set_info(const ConfigInfo& info) { info_ = info; }

  // Returns a map from namespace to digest (hash of last known server state).
  const MetaDigestMap& digest_by_namespace() const {
    return digest_by_namespace_;
  }
  void set_digest_by_namespace(const MetaDigestMap& digest_by_namespace) {
    digest_by_namespace_ = digest_by_namespace;
  }

  // Set setting with value.
  const MetaSettingsMap& settings() const { return settings_; }
  void AddSetting(const ConfigSetting& setting, const std::string& value);
  // Return setting value by setting. Return "0" if value does not given.
  std::string GetSetting(const ConfigSetting& setting) const;

  bool operator==(const RemoteConfigMetadata& right) const;

 private:
  // Informatio about last fetch: time, status. See more detail about ConfigInfo
  // in `remote_config.h` header.
  ConfigInfo info_;

  // HTTP response for fetching contains field digest for each namespace. Digest
  // it's some identifier for namespace, it can be hash. And we need to send
  // this field in HTTP request for fetching. Server need this field to optimize
  // response size (e.g. case when namespace doesn't have change).
  MetaDigestMap digest_by_namespace_;

  // Developers settings.
  //
  // For now it's only one key: kConfigSettingDeveloperMode. Set "1" to enable
  // and "0" to disable.
  MetaSettingsMap settings_;
};

// Helper to deserialize elements of a Flexbuffer Map to map or unordered map,
// of strings to strings.
template <typename T>
inline void DeserializeMap(T* map, const flexbuffers::Map& map_ref) {
  for (int i = 0, n = map_ref.size(); i < n; ++i) {
    (*map)[map_ref.Keys()[i].AsKey()] = map_ref.Values()[i].AsString().c_str();
  }
}

}  // namespace internal
}  // namespace remote_config
}  // namespace firebase

#endif  // FIREBASE_REMOTE_CONFIG_CLIENT_CPP_SRC_DESKTOP_METADATA_H_
