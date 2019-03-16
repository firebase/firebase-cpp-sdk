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

#ifndef FIREBASE_REMOTE_CONFIG_CLIENT_CPP_SRC_DESKTOP_FILE_MANAGER_H_
#define FIREBASE_REMOTE_CONFIG_CLIENT_CPP_SRC_DESKTOP_FILE_MANAGER_H_

#include <string>

#include "remote_config/src/desktop/config_data.h"

namespace firebase {
namespace remote_config {
namespace internal {

// Use this class to save Remote Config Client `LayeredConfigs` to file and
// load from file.
class RemoteConfigFileManager {
 public:
  explicit RemoteConfigFileManager(const std::string& file_path);

  // Load `configs` from file. Will return `true` if success.
  bool Load(LayeredConfigs* configs) const;

  // Save `configs` to file. Will return `true` if success.
  bool Save(const LayeredConfigs& configs) const;

 private:
  // Path to file with data.
  std::string file_path_;
};

}  // namespace internal
}  // namespace remote_config
}  // namespace firebase

#endif  // FIREBASE_REMOTE_CONFIG_CLIENT_CPP_SRC_DESKTOP_FILE_MANAGER_H_
