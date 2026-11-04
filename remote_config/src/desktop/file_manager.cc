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
      AppDataDir(app_data_prefix.c_str(), true, &error);
  if (error.empty() && !app_dir.empty()) {
    file_path_ = MakeFilesystemPath(
        app_dir + "/" + SanitizeFilenameComponent(app.name()) + "_" + filename);
  }
}
bool RemoteConfigFileManager::Load(LayeredConfigs* configs) const {
  if (!configs || file_path_.empty()) return false;
  std::ifstream input(file_path_, std::ios::in | std::ios::binary);
  if (!input.is_open() || !input.good()) return false;
  std::stringstream ss;
  ss << input.rdbuf();
  return !input.bad() && configs->Deserialize(ss.str());
}
bool RemoteConfigFileManager::Save(const LayeredConfigs& configs) const {
  if (file_path_.empty()) return false;
  std::string buffer = configs.Serialize();
  std::filesystem::path tmp_path = file_path_;
  tmp_path += ".tmp." + std::to_string(CurrentProcessId()) + "." +
              std::to_string(NextTempFileId());
#if FIREBASE_PLATFORM_WINDOWS
  FILE* fp = _wfopen(tmp_path.c_str(), L"wb");
  if (!fp) return false;
  const bool ok = (std::fwrite(buffer.data(), 1, buffer.size(), fp) ==
                   buffer.size()) &&
                  (std::fflush(fp) == 0) && (_commit(_fileno(fp)) == 0);
  if (std::fclose(fp) != 0 || !ok ||
      !MoveFileExW(tmp_path.c_str(), file_path_.c_str(),
                   MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
    _wremove(tmp_path.c_str());
    return false;
  }
  return true;
#else
  FILE* fp = std::fopen(tmp_path.c_str(), "wb");
  if (!fp) return false;
  const bool ok = (std::fwrite(buffer.data(), 1, buffer.size(), fp) ==
                   buffer.size()) &&
                  (std::fflush(fp) == 0) && (fsync(fileno(fp)) == 0);
  if (std::fclose(fp) != 0 || !ok ||
      std::rename(tmp_path.c_str(), file_path_.c_str()) != 0) {
    std::remove(tmp_path.c_str());
    return false;
  }
  return true;
#endif
}
}  // namespace firebase::remote_config::internal
