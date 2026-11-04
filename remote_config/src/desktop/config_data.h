#ifndef FIREBASE_REMOTE_CONFIG_SRC_DESKTOP_CONFIG_DATA_H_
#define FIREBASE_REMOTE_CONFIG_SRC_DESKTOP_CONFIG_DATA_H_
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include "remote_config/src/desktop/metadata.h"
namespace firebase::remote_config::internal {
using NamespaceKeyValueMap =
    std::map<std::string, std::map<std::string, std::string>>;
class NamespacedConfigData {
 public:
  NamespacedConfigData();
  NamespacedConfigData(NamespaceKeyValueMap config, uint64_t timestamp);
  [[nodiscard]] std::string Serialize() const;
  [[nodiscard]] bool Deserialize(std::string_view buffer);
  void SetNamespace(const std::map<std::string, std::string>& map,
                    const std::string& name_space);
  [[nodiscard]] bool HasValue(std::string_view key,
                              std::string_view name_space) const;
  [[nodiscard]] std::string GetValue(std::string_view key,
                                     std::string_view name_space) const;
  void GetKeysByPrefix(std::string_view prefix, std::string_view name_space,
                       std::set<std::string>* keys) const;
  [[nodiscard]] const NamespaceKeyValueMap& config() const;
  [[nodiscard]] uint64_t timestamp() const;
  bool operator==(const NamespacedConfigData& right) const;
 private:
  NamespaceKeyValueMap config_;
  uint64_t timestamp_;
};
struct LayeredConfigs {
  LayeredConfigs();
  LayeredConfigs(const NamespacedConfigData& config_fetched,
                 const NamespacedConfigData& config_active,
                 const NamespacedConfigData& config_default,
                 const RemoteConfigMetadata& fetch_metadata);
  [[nodiscard]] std::string Serialize() const;
  [[nodiscard]] bool Deserialize(std::string_view buffer);
  bool operator==(const LayeredConfigs& right) const;
  NamespacedConfigData fetched;
  NamespacedConfigData active;
  NamespacedConfigData defaults;
  RemoteConfigMetadata metadata;
};
}  // namespace firebase::remote_config::internal
#endif
