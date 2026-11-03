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

#ifndef FIREBASE_REMOTE_CONFIG_SRC_DESKTOP_FILE_MANAGER_H_
#define FIREBASE_REMOTE_CONFIG_SRC_DESKTOP_FILE_MANAGER_H_

#include <filesystem>
#include <string>

#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/internal/platform.h"
#include "remote_config/src/desktop/config_data.h"

namespace firebase::remote_config::internal {

// Use this class to save Remote Config Client `LayeredConfigs` to file and
// load from file.
class RemoteConfigFileManager {
 public:
  explicit RemoteConfigFileManager(const std::string& file_path);
  RemoteConfigFileManager(const std::string& file_path,
                          const firebase::App& app);

  // Load `configs` from file. Will return `true` if success.
  [[nodiscard]] bool Load(LayeredConfigs* configs) const;

  // Save `configs` to file. Will return `true` if success.
  [[nodiscard]] bool Save(const LayeredConfigs& configs) const;

 private:
  // Path to file with data.
  std::filesystem::path file_path_;
};

}  // namespace firebase::remote_config::internal

#endif  // FIREBASE_REMOTE_CONFIG_SRC_DESKTOP_FILE_MANAGER_H_
