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

#include <algorithm>
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

#include "app/src/util_windows.h"
#else
#include <unistd.h>
#endif

// Note: Desktop-only compilation unit (never compiled for iOS/tvOS).
namespace firebase::remote_config::internal {

namespace {

std::filesystem::path MakeFilesystemPath(const std::string& utf8_path) {
#if FIREBASE_PLATFORM_WINDOWS
  return std::filesystem::path(Utf8ToWideString(utf8_path));
#else
  return std::filesystem::path(utf8_path);
#endif
}

uint64_t NextTempFileId() {
  static std::atomic<uint64_t> counter{0};
  return counter.fetch_add(1, std::memory_order_relaxed);
}

uint64_t CurrentProcessId() {
#if FIREBASE_PLATFORM_WINDOWS
  return static_cast<uint64_t>(GetCurrentProcessId());
#else
  return static_cast<uint64_t>(getpid());
#endif
}

std::string SanitizeFilenameComponent(const char* name) {
  if (!name) return "";
  std::string sanitized(name);
  std::replace_if(
      sanitized.begin(), sanitized.end(),
      [](char c) {
        return c == '/' || c == '\\' || c == ':' || c == '<' || c == '>' ||
               c == '"' || c == '|' || c == '?' || c == '*';
      },
      '_');
  return sanitized;
}

}  // namespace

RemoteConfigFileManager::RemoteConfigFileManager(const std::string& file_path)
    : file_path_(MakeFilesystemPath(file_path)) {}

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
  if (error.empty() && !app_dir.empty()) {
    std::string safe_app_name = SanitizeFilenameComponent(app.name());
    std::string combined = app_dir + "/" + safe_app_name + "_" + filename;
    file_path_ = MakeFilesystemPath(combined);
  }
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
  const uint64_t pid = CurrentProcessId();
  std::string suffix =
      ".tmp." + std::to_string(pid) + "." + std::to_string(tmp_id);
  std::filesystem::path tmp_path = file_path_;
  tmp_path += suffix;
#if FIREBASE_PLATFORM_WINDOWS
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

}  // namespace firebase::remote_config::internal
