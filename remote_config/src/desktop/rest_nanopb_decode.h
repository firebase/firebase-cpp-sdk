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

#ifndef FIREBASE_REMOTE_CONFIG_CLIENT_CPP_SRC_DESKTOP_REST_NANOPB_DECODE_H_
#define FIREBASE_REMOTE_CONFIG_CLIENT_CPP_SRC_DESKTOP_REST_NANOPB_DECODE_H_

#include <string>
#include <vector>

// Needed because of direct use of enum from generated header.
#include "remote_config/config.pb.h"

namespace firebase {
namespace remote_config {
namespace internal {

// All of these structs are meant to store data from the proto, one-to-one.
// src_protos/config.proto

struct KeyValue {
  std::string key;
  std::string value;
};

struct AppNamespaceConfig {
  // namespace is the source of the configuration included in this message.
  std::string config_namespace;
  std::string digest;
  std::vector<KeyValue> key_values;

  desktop_config_AppNamespaceConfigTable_NamespaceStatus status;
};

struct AppConfig {
  // This represents the package name.
  std::string app_name;

  // Holds per namespace configuration for this app.
  // if the app has no configuration defined, then this field will be empty.
  std::vector<AppNamespaceConfig> ns_configs;
};

struct ConfigFetchResponse {
  // holds all configuration data to be sent to the fetching device.
  std::vector<AppConfig> configs;
};

bool DecodeResponse(ConfigFetchResponse* destination,
                    const std::string& proto_str);

}  // namespace internal
}  // namespace remote_config
}  // namespace firebase

#endif  // FIREBASE_REMOTE_CONFIG_CLIENT_CPP_SRC_DESKTOP_REST_NANOPB_DECODE_H_
