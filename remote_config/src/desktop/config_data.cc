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

#include <cstdint>
#include <string_view>
#include <utility>

#include "flatbuffers/flexbuffers.h"
#include "remote_config/src/desktop/metadata.h"

namespace firebase::remote_config::internal {

NamespacedConfigData::NamespacedConfigData() : timestamp_(0) {}

NamespacedConfigData::NamespacedConfigData(NamespaceKeyValueMap config,
                                           uint64_t timestamp)
    : config_(std::move(config)), timestamp_(timestamp) {}

std::string NamespacedConfigData::Serialize() const {
  flexbuffers::Builder fbb;
  // Write out the struct members as a map.
  fbb.Map([&]() {
    // Map of Namespace to KeyVal
    fbb.Map("config_", [&]() {
      for (const auto& [ns_name, kv_map] : config_) {
        // Map of Key to Val
        fbb.Map(ns_name.c_str(), [&]() {
          for (const auto& [key, val] : kv_map) {
            fbb.String(key.c_str(), val);
          }
        });
      }
    });
    fbb.UInt("timestamp_", timestamp_);
  });
  fbb.Finish();
  const std::vector<uint8_t>& buffer = fbb.GetBuffer();
  return std::string(buffer.cbegin(), buffer.cend());
}

bool NamespacedConfigData::Deserialize(std::string_view buffer) {
  if (buffer.empty()) {
    return false;
  }
  const uint8_t* data = reinterpret_cast<const uint8_t*>(buffer.data());
  size_t size = buffer.size();
  if (!flexbuffers::VerifyBuffer(data, size)) {
    return false;
  }
  flexbuffers::Reference root = flexbuffers::GetRoot(data, size);
  if (!root.IsMap()) {
    return false;
  }
  auto struct_map = root.AsMap();
  config_.clear();
  flexbuffers::Map ns_config_map = struct_map["config_"].AsMap();
  const auto ns_keys = ns_config_map.Keys();
  const auto ns_values = ns_config_map.Values();
  for (size_t i = 0, in = ns_config_map.size(); i < in; ++i) {
    const char* ns_key = ns_keys[i].AsKey();
    std::map<std::string, std::string>& kv_config = config_[ns_key];
    auto kv_map = ns_values[i].AsMap();
    DeserializeMap(&kv_config, kv_map);
  }
  timestamp_ = struct_map["timestamp_"].AsUInt64();
  return true;
}

void NamespacedConfigData::SetNamespace(
    const std::map<std::string, std::string>& map,
    const std::string& name_space) {
  config_[name_space] = map;
}

bool NamespacedConfigData::HasValue(std::string_view key,
                                    std::string_view name_space) const {
  auto name_space_iter = config_.find(std::string(name_space));
  if (name_space_iter == config_.end()) {
    return false;
  }
  return name_space_iter->second.find(std::string(key)) !=
         name_space_iter->second.end();
}

std::string NamespacedConfigData::GetValue(std::string_view key,
                                           std::string_view name_space) const {
  auto name_space_iter = config_.find(std::string(name_space));
  if (name_space_iter == config_.end()) {
    return "";
  }
  auto key_iter = name_space_iter->second.find(std::string(key));
  if (key_iter == name_space_iter->second.end()) {
    return "";
  }
  return key_iter->second;
}

void NamespacedConfigData::GetKeysByPrefix(std::string_view prefix,
                                           std::string_view name_space,
                                           std::set<std::string>* keys) const {
  if (!keys) {
    return;
  }
  auto name_space_iter = config_.find(std::string(name_space));
  if (name_space_iter != config_.end()) {
    for (auto it = name_space_iter->second.lower_bound(std::string(prefix));
         it != name_space_iter->second.end(); ++it) {
      if (std::string_view(it->first).substr(0, prefix.size()) != prefix) {
        break;
      }
      keys->insert(it->first);
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

LayeredConfigs::LayeredConfigs() = default;
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

bool LayeredConfigs::Deserialize(std::string_view buffer) {
  if (buffer.empty()) {
    return false;
  }
  const uint8_t* data = reinterpret_cast<const uint8_t*>(buffer.data());
  size_t size = buffer.size();
  if (!flexbuffers::VerifyBuffer(data, size)) {
    return false;
  }
  flexbuffers::Reference root = flexbuffers::GetRoot(data, size);
  if (!root.IsMap()) {
    return false;
  }
  auto struct_map = root.AsMap();
  auto deserialize_optional = [](const flexbuffers::Reference& ref,
                                 auto* target) {
    if (ref.IsNull()) {
      *target = {};
      return true;
    }
    if (!ref.IsString()) {
      return false;
    }
    flexbuffers::String str = ref.AsString();
    return target->Deserialize(std::string_view(str.c_str(), str.length()));
  };
  return deserialize_optional(struct_map["fetched"], &fetched) &&
         deserialize_optional(struct_map["active"], &active) &&
         deserialize_optional(struct_map["defaults"], &defaults) &&
         deserialize_optional(struct_map["metadata"], &metadata);
}

bool LayeredConfigs::operator==(const LayeredConfigs& right) const {
  return fetched == right.fetched && active == right.active &&
         defaults == right.defaults && metadata == right.metadata;
}

}  // namespace firebase::remote_config::internal
