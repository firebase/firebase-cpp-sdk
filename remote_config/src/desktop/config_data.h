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

#ifndef FIREBASE_REMOTE_CONFIG_SRC_DESKTOP_CONFIG_DATA_H_
#define FIREBASE_REMOTE_CONFIG_SRC_DESKTOP_CONFIG_DATA_H_

#include <cstdint>  // for uint64_t
#include <map>
#include <set>
#include <string>
#include <string_view>

#include "remote_config/src/desktop/metadata.h"

namespace firebase::remote_config::internal {

// NOTE: Configs are organized per app (iOS or Android). Instead of containing
// them all, we only store the set belonging to the app based on the AppOptions.

using NamespaceKeyValueMap =
    std::map<std::string, std::map<std::string, std::string>>;

// Use to keep and work with key/value records. Each namespace contains some
// amount of key/value records.
class NamespacedConfigData {
 public:
  NamespacedConfigData();
  NamespacedConfigData(NamespaceKeyValueMap config, uint64_t timestamp);

  // Serialize to a buffer as a string.
  // This happens to be using Flexbuffers, but could be implemented with any
  // Serialization method.
  [[nodiscard]] std::string Serialize() const;
  // Deserializes a string buffer previously Serialized.
  [[nodiscard]] bool Deserialize(std::string_view buffer);

  // Set key/value records from `map` by `namespace`.
  void SetNamespace(const std::map<std::string, std::string>& map,
                    const std::string& name_space);
  // Return true if `config` contains value by namespace and key.
  [[nodiscard]] bool HasValue(std::string_view key,
                              std::string_view name_space) const;

  // Return value by namespace and key. Will return empty string if no value in
  // `config`.
  [[nodiscard]] std::string GetValue(std::string_view key,
                                     std::string_view name_space) const;

  // Assign keys that start with `prefix` from `name_space` namespace to the
  // `keys` variable.
  void GetKeysByPrefix(std::string_view prefix, std::string_view name_space,
                       std::set<std::string>* keys) const;

  [[nodiscard]] const NamespaceKeyValueMap& config() const;
  [[nodiscard]] uint64_t timestamp() const;

  bool operator==(const NamespacedConfigData& right) const;

 private:
  // Contains key/value records for each namespace. To get some value need two
  // keys: namespace and key.
  //
  // The server returns all values as strings, so we store it that way.
  // When the values are accessed, they will be converted appropriately.
  NamespaceKeyValueMap config_;

  // Meaning varies based on config layer:
  // The time (in milliseconds since the epoch) since ...
  // * fetched: the last fetch operation completed.
  // * active: ActivateFetched was last called.
  // * default: the last setDefault function was called.
  uint64_t timestamp_;
};

// Contains all data needed for Remote Config client.
//
// Also to make HTTP request for fetching fresh data we need some
// information from this structure.
// TODO(b/74495747): If we only need *some*, don't use this whole struct for
// making HTTP requests.
struct LayeredConfigs {
  LayeredConfigs();
  LayeredConfigs(const NamespacedConfigData& config_fetched,
                 const NamespacedConfigData& config_active,
                 const NamespacedConfigData& config_default,
                 const RemoteConfigMetadata& fetch_metadata);

  [[nodiscard]] std::string Serialize() const;
  [[nodiscard]] bool Deserialize(std::string_view buffer);

  // For testing.
  bool operator==(const LayeredConfigs& right) const;

  NamespacedConfigData fetched;
  NamespacedConfigData active;
  NamespacedConfigData defaults;
  RemoteConfigMetadata metadata;
};

}  // namespace firebase::remote_config::internal

#endif  // FIREBASE_REMOTE_CONFIG_SRC_DESKTOP_CONFIG_DATA_H_
