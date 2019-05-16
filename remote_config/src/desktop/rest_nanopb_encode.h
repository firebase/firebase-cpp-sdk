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

#ifndef FIREBASE_REMOTE_CONFIG_CLIENT_CPP_SRC_DESKTOP_REST_NANOPB_ENCODE_H_
#define FIREBASE_REMOTE_CONFIG_CLIENT_CPP_SRC_DESKTOP_REST_NANOPB_ENCODE_H_

#include <map>
#include <string>

namespace firebase {
namespace remote_config {
namespace internal {

typedef std::map<std::string, std::string> NamedValues;

struct PackageData {
  // name of the package for which the device is fetching config from the
  // backend.
  std::string package_name;

  // Firebase Project Number.
  std::string gmp_project_id;

  // per namespace digests of the local config table of the app, in
  // the format of: NamedValue(name=namespace, value=digest)
  NamedValues namespace_digest;

  // custom variables as defined by the client app.
  NamedValues custom_variable;

  // optional
  // The instance id of the app.
  std::string app_instance_id;

  // optional
  // The instance id token of the app.
  std::string app_instance_id_token;

  // version of the firebase remote config sdk. constructed by a major version,
  // a minor version, and a patch version, using the formula:
  // (major * 10000) + (minor * 100) + patch
  int sdk_version;

  // the cache expiration seconds specified while calling fetch()
  // in seconds
  int requested_cache_expiration_seconds;

  // the age of the fetched config: now() - last time fetch() was called
  // in seconds
  // if there was no fetched config, the value will be set to -1
  int fetched_config_age_seconds;

  // the age of the active config:
  // now() - last time activateFetched() was called
  // in seconds
  // if there was no active config, the value will be set to -1
  int active_config_age_seconds;
};

struct ConfigFetchRequest {
  int client_version;
  int device_type;
  int device_subtype;
  PackageData package_data;
};

std::string EncodeFetchRequest(const ConfigFetchRequest& config_fetch_request);

}  // namespace internal
}  // namespace remote_config
}  // namespace firebase

#endif  // FIREBASE_REMOTE_CONFIG_CLIENT_CPP_SRC_DESKTOP_REST_NANOPB_ENCODE_H_
