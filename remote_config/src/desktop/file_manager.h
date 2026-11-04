#ifndef FIREBASE_REMOTE_CONFIG_SRC_DESKTOP_FILE_MANAGER_H_
#define FIREBASE_REMOTE_CONFIG_SRC_DESKTOP_FILE_MANAGER_H_
#include <filesystem>
#include <string>
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/internal/platform.h"
#include "remote_config/src/desktop/config_data.h"
namespace firebase::remote_config::internal {
class RemoteConfigFileManager {
 public:
  explicit RemoteConfigFileManager(const std::string& file_path);
  RemoteConfigFileManager(const std::string& file_path,
                          const firebase::App& app);
  [[nodiscard]] bool Load(LayeredConfigs* configs) const;
  [[nodiscard]] bool Save(const LayeredConfigs& configs) const;
 private:
  std::filesystem::path file_path_;
};
}  // namespace firebase::remote_config::internal
#endif
