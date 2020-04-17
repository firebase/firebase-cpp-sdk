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
#include "remote_config/src/desktop/metadata.h"
#include "flatbuffers/flexbuffers.h"

namespace firebase {
namespace remote_config {
namespace internal {

NamespacedConfigData::NamespacedConfigData()
    : config_(NamespaceKeyValueMap()), timestamp_(0) {}

NamespacedConfigData::NamespacedConfigData(const NamespaceKeyValueMap& config,
                                           uint64_t timestamp)
    : config_(config), timestamp_(timestamp) {}

std::string NamespacedConfigData::Serialize() const {
  flexbuffers::Builder fbb;
  // Write out the struct members as a map.
  fbb.Map([&]() {
    // Map of Namespace to KeyVal
    fbb.Map("config_", [&]() {
      for (const auto& key_to_map : config_) {
        // Map of Key to Val
        fbb.Add(key_to_map.first.c_str(), key_to_map.second);
      }
    });
    fbb.UInt("timestamp_", timestamp_);
  });
  fbb.Finish();
  const std::vector<uint8_t>& buffer = fbb.GetBuffer();
  return std::string(buffer.cbegin(), buffer.cend());
}

void NamespacedConfigData::Deserialize(const std::string& buffer) {
  const uint8_t* data = reinterpret_cast<const uint8_t*>(buffer.data());
  size_t size = buffer.size();
  auto struct_map = flexbuffers::GetRoot(data, size).AsMap();
  flexbuffers::Map ns_config_map = struct_map["config_"].AsMap();
  for (int i = 0, in = ns_config_map.size(); i < in; ++i) {
    const char* ns_key = ns_config_map.Keys()[i].AsKey();
    std::map<std::string, std::string>& kv_config = config_[ns_key];
    auto kv_map = ns_config_map.Values()[i].AsMap();
    DeserializeMap(&kv_config, kv_map);
  }
  timestamp_ = struct_map["timestamp_"].AsUInt64();
}

void NamespacedConfigData::SetNamespace(
    const std::map<std::string, std::string>& map,
    const std::string& name_space) {
  config_[name_space] = map;
}

bool NamespacedConfigData::HasValue(const std::string& key,
                                    const std::string& name_space) const {
  auto name_space_iter = config_.find(name_space);
  if (name_space_iter == config_.end()) {
    return false;
  }
  auto key_iter = name_space_iter->second.find(key);
  if (key_iter == name_space_iter->second.end()) {
    return false;
  }
  return true;
}

std::string NamespacedConfigData::GetValue(
    const std::string& key, const std::string& name_space) const {
  auto name_space_iter = config_.find(name_space);
  if (name_space_iter == config_.end()) {
    return "";
  }
  auto key_iter = name_space_iter->second.find(key);
  if (key_iter == name_space_iter->second.end()) {
    return "";
  }
  return key_iter->second;
}

void NamespacedConfigData::GetKeysByPrefix(const std::string& prefix,
                                           const std::string& name_space,
                                           std::set<std::string>* keys) const {
  auto name_space_iter = config_.find(name_space);
  if (name_space_iter != config_.end()) {
    for (const auto& key_iter : name_space_iter->second) {
      if (key_iter.first.substr(0, prefix.length()) == prefix) {
        keys->insert(key_iter.first);
      }
    }
  }
}

const NamespaceKeyValueMap& NamespacedConfigData::config() const {
  return config_;
}

uint64_t NamespacedConfigData::timestamp() const { return timestamp_; }

bool NamespacedConfigData::operator==(const NamespacedConfigData& right) const {
  return config_ == right.config_ && timestamp_ == right.timestamp_;
}

LayeredConfigs::LayeredConfigs() {}
LayeredConfigs::LayeredConfigs(const NamespacedConfigData& config_fetched,
                               const NamespacedConfigData& config_active,
                               const NamespacedConfigData& config_default,
                               const RemoteConfigMetadata& fetch_metadata)
    : fetched(config_fetched),
      active(config_active),
      defaults(config_default),
      metadata(fetch_metadata) {}

std::string LayeredConfigs::Serialize() const {
  flexbuffers::Builder fbb;
  // Write out the struct members as a map.
  fbb.Map([&]() {
    fbb.String("fetched", fetched.Serialize());
    fbb.String("active", active.Serialize());
    fbb.String("defaults", defaults.Serialize());
    fbb.String("metadata", metadata.Serialize());
  });
  fbb.Finish();
  const std::vector<uint8_t>& buffer = fbb.GetBuffer();
  return std::string(buffer.cbegin(), buffer.cend());
}

void LayeredConfigs::Deserialize(const std::string& buffer) {
  const uint8_t* data = reinterpret_cast<const uint8_t*>(buffer.data());
  size_t size = buffer.size();
  auto struct_map = flexbuffers::GetRoot(data, size).AsMap();
  fetched.Deserialize(struct_map["fetched"].AsString().str());
  active.Deserialize(struct_map["active"].AsString().str());
  defaults.Deserialize(struct_map["defaults"].AsString().str());
  metadata.Deserialize(struct_map["metadata"].AsString().str());
}

bool LayeredConfigs::operator==(const LayeredConfigs& right) const {
  return fetched == right.fetched && active == right.active &&
         defaults == right.defaults && metadata == right.metadata;
}

}  // namespace internal
}  // namespace remote_config
}  // namespace firebase
