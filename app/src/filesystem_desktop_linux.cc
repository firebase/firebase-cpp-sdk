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

#include "app/src/filesystem.h"

#include <pwd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <string>

#include "app/src/util.h"

namespace firebase {

namespace {

std::string HomeDir(std::string* out_error) {
  const char* home_dir = getenv("HOME");
  if (home_dir && home_dir[0] != '\0') return home_dir;

  long sysconf_size = sysconf(_SC_GETPW_R_SIZE_MAX);
  size_t buffer_size =
      sysconf_size > 0 ? static_cast<size_t>(sysconf_size) : 1024;
  std::string buffer(buffer_size, '\0');
  uid_t uid = getuid();
  int rc;
  passwd pwd;
  passwd* result = nullptr;
  while (true) {
    rc = getpwuid_r(uid, &pwd, &buffer[0], buffer_size, &result);
    if (rc == EINTR) {
      continue;
    }
    if (rc == ERANGE && buffer_size < 1024 * 1024) {
      buffer_size *= 2;
      buffer.resize(buffer_size, '\0');
      continue;
    }
    break;
  }

  if (rc != 0 || result == nullptr || pwd.pw_dir == nullptr ||
      pwd.pw_dir[0] == '\0') {
    if (out_error) {
      if (rc != 0) {
        *out_error =
            std::string(
                "Failed to find the home directory for the current user "
                "(errno: ") +
            std::to_string(rc) + ")";
      } else {
        *out_error =
            "Failed to find the home directory for the current user "
            "(user entry not found)";
      }
    }
    return "";
  }

  return pwd.pw_dir;
}

std::string XdgDataHomeDir() {
  const char* data_home = getenv("XDG_DATA_HOME");
  if (data_home && data_home[0] != '\0') return data_home;
  return "";
}

bool PathExists(const std::string& path) {
  struct stat sb;

  return (stat(path.c_str(), &sb) == 0 && S_ISDIR(sb.st_mode));
}

bool Mkdir(const std::string& path, std::string* out_error) {
  int retval = mkdir(path.c_str(), 0700);
  if (retval != 0) {
    int err = errno;
    if (err == EEXIST && PathExists(path)) {
      return true;
    }
    if (out_error) {
      *out_error = std::string("Could not create directory '") + path +
                   "' (error code: " + std::to_string(err) + ")";
    }

    return false;
  }

  return true;
}

}  // namespace

std::string AppDataDir(const char* app_name, bool should_create,
                       std::string* out_error) {
  if (!app_name || strlen(app_name) == 0) {
    if (out_error) {
      *out_error = "AppDataDir failed: no app_name provided";
    }
    return "";
  }

  std::string app_dir = app_name;

  // On Linux, use XDG_DATA_HOME, usually $HOME/.local/share/$app_name
  std::string home = XdgDataHomeDir();
  if (home.empty()) {
    home = HomeDir(out_error);
    if (home.empty()) return "";
    app_dir = std::string(".local/share/") + app_name;
  }

  std::string full_path = home + '/' + app_dir;
  if (should_create && !PathExists(full_path)) {
    std::string current_path = full_path[0] == '/' ? "/" : "";

    for (const std::string& nested_dir : SplitString(full_path, '/')) {
      current_path += nested_dir;

      if (!PathExists(current_path)) {
        bool created = Mkdir(current_path, out_error);
        if (!created) return "";
      }
      current_path += '/';
    }
  }

  return full_path;
}

}  // namespace firebase
