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

#include "remote_config/src/desktop/file_manager.h"

#include <cstdint>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <utility>

#include "app/src/filesystem.h"
#include "app/src/include/firebase/internal/platform.h"
#include "remote_config/src/desktop/config_data.h"

#if FIREBASE_PLATFORM_WINDOWS
#include <codecvt>
#include <locale>
#endif

namespace firebase {
namespace remote_config {
namespace internal {

RemoteConfigFileManager::RemoteConfigFileManager(const std::string& filename,
                                                 const firebase::App& app) {
  std::string app_data_prefix =
      std::string(app.options().package_name()) + "/" + app.name();
  std::string file_path =
      AppDataDir(app_data_prefix.c_str(), /*should_create=*/true) + "/" +
      filename;
#if FIREBASE_PLATFORM_WINDOWS
  std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> utf8_to_wstring;
  file_path_ = utf8_to_wstring.from_bytes(file_path);
#else
  file_path_ = file_path;
#endif
}

bool RemoteConfigFileManager::Load(LayeredConfigs* configs) const {
  std::fstream input(file_path_, std::ios::in | std::ios::binary);
  std::stringstream ss;
  ss << input.rdbuf();
  configs->Deserialize(ss.str());
  return true;
}

bool RemoteConfigFileManager::Save(const LayeredConfigs& configs) const {
  std::string buffer = configs.Serialize();
  std::fstream output(file_path_, std::ios::out | std::ios::binary);
  output.write(buffer.c_str(), buffer.size());
  return true;
}

}  // namespace internal
}  // namespace remote_config
}  // namespace firebase
