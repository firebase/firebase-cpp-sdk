#ifndef FIREBASE_REMOTE_CONFIG_SRC_DESKTOP_METADATA_H_
#define FIREBASE_REMOTE_CONFIG_SRC_DESKTOP_METADATA_H_
#include <map>
#include <string>
#include <string_view>
#include "firebase/variant.h"
#include "flatbuffers/flexbuffers.h"
#include "remote_config/src/include/firebase/remote_config.h"
namespace firebase {
namespace remote_config {
namespace internal {
using MetaDigestMap = std::map<std::string, std::string>;
using MetaSettingsMap = std::map<ConfigSetting, std::string>;
using MetaCustomSignalsMap = std::map<std::string, Variant>;
class RemoteConfigMetadata {
 public:
  RemoteConfigMetadata();
  std::string Serialize() const;
  bool Deserialize(std::string_view buffer);
  const ConfigInfo& info() const { return info_; }
  void set_info(const ConfigInfo& info) { info_ = info; }
  const MetaDigestMap& digest_by_namespace() const {
    return digest_by_namespace_;
  }
  void set_digest_by_namespace(const MetaDigestMap& digest_by_namespace) {
    digest_by_namespace_ = digest_by_namespace;
  }
  const MetaSettingsMap& settings() const { return settings_; }
  void AddSetting(const ConfigSetting& setting, const std::string& value);
  std::string GetSetting(const ConfigSetting& setting) const;
  const MetaCustomSignalsMap& custom_signals() const { return custom_signals_; }
  void set_custom_signals(const MetaCustomSignalsMap& custom_signals) {
    custom_signals_ = custom_signals;
  }
  bool operator==(const RemoteConfigMetadata& right) const;
 private:
  ConfigInfo info_;
  MetaDigestMap digest_by_namespace_;
  MetaSettingsMap settings_;
  MetaCustomSignalsMap custom_signals_;
};
template <typename T>
inline void DeserializeMap(T* map, const flexbuffers::Map& map_ref) {
  if (!map) return;
  map->clear();
  const auto keys = map_ref.Keys();
  const auto values = map_ref.Values();
  for (size_t i = 0, n = map_ref.size(); i < n; ++i) {
    const char* key = keys[i].AsKey();
    if (!key || *key == '\0' || !values[i].IsString()) continue;
    flexbuffers::String val = values[i].AsString();
    map->try_emplace(key, val.c_str(), val.length());
  }
}
}  // namespace internal
}  // namespace remote_config
}  // namespace firebase
#endif
