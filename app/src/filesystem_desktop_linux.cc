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

#include "app/src/filesystem.h"
#include "app/src/util.h"

namespace FIREBASE_NAMESPACE {

namespace {

std::string HomeDir(std::string* out_error) {
  const char* home_dir = getenv("HOME");
  if (home_dir) return home_dir;

  auto buffer_size = static_cast<size_t>(sysconf(_SC_GETPW_R_SIZE_MAX));
  std::string buffer(buffer_size, '\0');
  uid_t uid = getuid();
  int rc;
  passwd pwd;
  passwd* result;
  do {
    rc = getpwuid_r(uid, &pwd, &buffer[0], buffer_size, &result);
  } while (rc == EINTR);

  if (rc != 0) {
    if (out_error) {
      *out_error = std::string(
                       "Failed to find the home directory for the current user "
                       "(errno: ") +
                   std::to_string(errno) + ")";
    }
    return "";
  }

  return pwd.pw_dir;
}

std::string XdgDataHomeDir(std::string* out_error) {
  const char* data_home = getenv("XDG_DATA_HOME");
  if (data_home) return data_home;
  return "";
}

bool Mkdir(const std::string& path, std::string* out_error) {
  int retval = mkdir(path.c_str(), 0700);
  if (retval != 0 && errno != EEXIST) {
    if (out_error) {
      *out_error = std::string("Could not create directory '") + path +
                   "' (error code: " + std::to_string(errno) + ")";
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
  std::string home = XdgDataHomeDir(out_error);
  if (home.empty()) {
    home = HomeDir(out_error);
    if (home.empty()) return "";
    app_dir = std::string(".local/share/") + app_name;
  }

  if (should_create) {
    std::string current_path = home;
    for (const std::string& nested_dir : SplitString(app_dir, '/')) {
      current_path += '/';
      current_path += nested_dir;

      bool created = Mkdir(current_path, out_error);
      if (!created) return "";
    }
  }

  return home + "/" + app_dir;
}

}  // namespace FIREBASE_NAMESPACE
