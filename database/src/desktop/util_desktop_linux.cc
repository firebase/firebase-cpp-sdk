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

#include <dirent.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "database/src/desktop/util_desktop.h"

namespace firebase {
namespace database {
namespace internal {

std::string GetAppDataPath(const char* app_name, bool should_create) {
  // On Linux, use XDG data home, usually $HOME/.local/share/$app_name, or user
  // home directory otherwise.

  const char* xdg_data_home = getenv("XDG_DATA_HOME");
  if (xdg_data_home) {
    // Assume that this directory already exists.
    return std::string(xdg_data_home);
  }
  std::string home_directory = "";

  const char* home = getenv("HOME");
  if (home) {
    home_directory = std::string(home);
  } else {
    // Get home directory from user info.
    passwd pwd;
    passwd* result;
    auto buffer_size = static_cast<size_t>(sysconf(_SC_GETPW_R_SIZE_MAX));
    std::string buffer(buffer_size, '\0');
    uid_t uid = getuid();
    int rc;
    do {
      rc = getpwuid_r(uid, &pwd, &buffer[0], buffer_size, &result);
    } while (rc == EINTR);
    if (rc == 0) {
      home_directory = std::string(pwd.pw_dir);
    }
  }
  if (home_directory.empty()) return "";

  // Make sure home/.local/share exists, ignore "already exists" errors making
  // these directories.
  if (should_create) {
    int retval;
    std::string new_dirs = std::string("/.local/share/") + app_name;

    // Start is the location of the next "/" in the new_dirs string.
    size_t start = 0;
    while (start != std::string::npos) {
      // Get the next directory.
      size_t finish = new_dirs.find("/", start + 1);
      home_directory.append(new_dirs, start, finish - start);
      start = finish;

      // Ensure the new directory exists.
      retval = mkdir(home_directory.c_str(), 0700);
      if (retval != 0 && errno != EEXIST) return "";
    }
  }
  return home_directory;
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
