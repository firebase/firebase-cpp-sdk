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

#include <atomic>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

#include "app/src/filesystem.h"
#include "app/src/include/firebase/internal/platform.h"
#include "remote_config/src/desktop/config_data.h"

#if FIREBASE_PLATFORM_WINDOWS
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace firebase {
namespace remote_config {
namespace internal {

namespace {
#if FIREBASE_PLATFORM_WINDOWS
std::wstring Utf8ToWideString(const std::string& str) {
  if (str.empty()) return std::wstring();
  int size_needed = MultiByteToWideChar(
      CP_UTF8, 0, str.data(), static_cast<int>(str.size()), nullptr, 0);
  if (size_needed <= 0) return std::wstring();
  std::wstring result(static_cast<size_t>(size_needed), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()),
                      result.data(), size_needed);
  return result;
}
#endif

uint64_t NextTempFileId() {
  static std::atomic<uint64_t> counter{0};
  return counter.fetch_add(1, std::memory_order_relaxed);
}
}  // namespace

RemoteConfigFileManager::RemoteConfigFileManager(const std::string& file_path) {
#if FIREBASE_PLATFORM_WINDOWS
  file_path_ = Utf8ToWideString(file_path);
#else
  file_path_ = file_path;
#endif
}

RemoteConfigFileManager::RemoteConfigFileManager(const std::string& filename,
                                                 const firebase::App& app) {
  const char* package_name = app.options().package_name();
  std::string app_data_prefix =
      (package_name && package_name[0] != '\0')
          ? std::string(package_name) + "/remote_config"
          : "remote_config";
  std::string error;
  std::string app_dir =
      AppDataDir(app_data_prefix.c_str(), /*should_create=*/true, &error);
  std::string file_path;
  if (error.empty() && !app_dir.empty()) {
    file_path = app_dir + "/" + app.name() + "_" + filename;
  }
#if FIREBASE_PLATFORM_WINDOWS
  file_path_ = Utf8ToWideString(file_path);
#else
  file_path_ = file_path;
#endif
}

bool RemoteConfigFileManager::Load(LayeredConfigs* configs) const {
  if (!configs || file_path_.empty()) {
    return false;
  }
  std::ifstream input(file_path_, std::ios::in | std::ios::binary);
  if (!input.is_open() || !input.good()) {
    return false;
  }
  std::stringstream ss;
  ss << input.rdbuf();
  if (input.bad()) {
    return false;
  }
  return configs->Deserialize(ss.str());
}

bool RemoteConfigFileManager::Save(const LayeredConfigs& configs) const {
  if (file_path_.empty()) {
    return false;
  }
  std::string buffer = configs.Serialize();
  const uint64_t tmp_id = NextTempFileId();
#if FIREBASE_PLATFORM_WINDOWS
  std::wstring tmp_path = file_path_ + L".tmp." + std::to_wstring(tmp_id);
  FILE* fp = _wfopen(tmp_path.c_str(), L"wb");
  if (!fp) {
    return false;
  }
  const size_t written = std::fwrite(buffer.data(), 1, buffer.size(), fp);
  const bool flush_ok =
      (written == buffer.size()) && (std::fflush(fp) == 0) &&
      (_commit(_fileno(fp)) == 0);
  const bool close_ok = (std::fclose(fp) == 0);
  if (!flush_ok || !close_ok) {
    _wremove(tmp_path.c_str());
    return false;
  }
  if (!MoveFileExW(tmp_path.c_str(), file_path_.c_str(),
                   MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
    _wremove(tmp_path.c_str());
    return false;
  }
  return true;
#else
  std::string tmp_path = file_path_ + ".tmp." + std::to_string(tmp_id);
  FILE* fp = std::fopen(tmp_path.c_str(), "wb");
  if (!fp) {
    return false;
  }
  const size_t written = std::fwrite(buffer.data(), 1, buffer.size(), fp);
  const bool flush_ok =
      (written == buffer.size()) && (std::fflush(fp) == 0) &&
      (fsync(fileno(fp)) == 0);
  const bool close_ok = (std::fclose(fp) == 0);
  if (!flush_ok || !close_ok) {
    std::remove(tmp_path.c_str());
    return false;
  }
  if (std::rename(tmp_path.c_str(), file_path_.c_str()) != 0) {
    std::remove(tmp_path.c_str());
    return false;
  }
  return true;
#endif
}

}  // namespace internal
}  // namespace remote_config
}  // namespace firebase
