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
  fbb.Map([&]() {
    fbb.Map("config_", [&]() {
      for (const auto& [ns_name, kv_map] : config_) {
        fbb.Map(ns_name.c_str(), [&]() {
          for (const auto& [key, val] : kv_map) fbb.String(key.c_str(), val);
        });
      }
    });
    fbb.UInt("timestamp_", timestamp_);
  });
  fbb.Finish();
  const auto& buffer = fbb.GetBuffer();
  return std::string(buffer.cbegin(), buffer.cend());
}
bool NamespacedConfigData::Deserialize(std::string_view buffer) {
  if (buffer.empty()) return false;
  const auto* data = reinterpret_cast<const uint8_t*>(buffer.data());
  if (!flexbuffers::VerifyBuffer(data, buffer.size())) return false;
  flexbuffers::Reference root = flexbuffers::GetRoot(data, buffer.size());
  if (!root.IsMap()) return false;
  auto struct_map = root.AsMap();
  config_.clear();
  flexbuffers::Map ns_config_map = struct_map["config_"].AsMap();
  const auto ns_keys = ns_config_map.Keys();
  const auto ns_values = ns_config_map.Values();
  for (size_t i = 0, in = ns_config_map.size(); i < in; ++i) {
    DeserializeMap(&config_[ns_keys[i].AsKey()], ns_values[i].AsMap());
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
  auto it = config_.find(std::string(name_space));
  return it != config_.end() &&
         it->second.find(std::string(key)) != it->second.end();
}
std::string NamespacedConfigData::GetValue(std::string_view key,
                                           std::string_view name_space) const {
  auto ns_it = config_.find(std::string(name_space));
  if (ns_it == config_.end()) return "";
  auto key_it = ns_it->second.find(std::string(key));
  return key_it != ns_it->second.end() ? key_it->second : "";
}
void NamespacedConfigData::GetKeysByPrefix(std::string_view prefix,
                                           std::string_view name_space,
                                           std::set<std::string>* keys) const {
  if (!keys) return;
  auto ns_it = config_.find(std::string(name_space));
  if (ns_it == config_.end()) return;
  for (auto it = ns_it->second.lower_bound(std::string(prefix));
       it != ns_it->second.end(); ++it) {
    if (std::string_view(it->first).substr(0, prefix.size()) != prefix) break;
    keys->insert(it->first);
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
  fbb.Map([&]() {
    fbb.String("fetched", fetched.Serialize());
    fbb.String("active", active.Serialize());
    fbb.String("defaults", defaults.Serialize());
    fbb.String("metadata", metadata.Serialize());
  });
  fbb.Finish();
  const auto& buffer = fbb.GetBuffer();
  return std::string(buffer.cbegin(), buffer.cend());
}
bool LayeredConfigs::Deserialize(std::string_view buffer) {
  if (buffer.empty()) return false;
  const auto* data = reinterpret_cast<const uint8_t*>(buffer.data());
  if (!flexbuffers::VerifyBuffer(data, buffer.size())) return false;
  flexbuffers::Reference root = flexbuffers::GetRoot(data, buffer.size());
  if (!root.IsMap()) return false;
  auto struct_map = root.AsMap();
  auto deserialize_optional = [](const flexbuffers::Reference& ref,
                                 auto* target) {
    if (ref.IsNull()) {
      *target = {};
      return true;
    }
    if (!ref.IsString()) return false;
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
