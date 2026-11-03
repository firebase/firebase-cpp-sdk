// Copyright 2019 Google LLC
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

#include "app/src/secure/user_secure_fake_internal.h"

#include <cstring>

#include "app/src/include/firebase/internal/platform.h"
#include "app/src/log.h"

#if FIREBASE_PLATFORM_WINDOWS
#include <direct.h>
#include <windows.h>
#else
#include <dirent.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif  // FIREBASE_PLATFORM_WINDOWS

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <mutex>
#include <string>
#include <vector>

namespace firebase {
namespace app {
namespace secure {

namespace {

std::mutex& GetFakeSecureMutex() {
  static auto* mutex = new std::mutex();
  return *mutex;
}

}  // namespace

#if FIREBASE_PLATFORM_WINDOWS
static const char kDirectorySeparator[] = "\\";
#define unlink _unlink
#define mkdir(x, y) _mkdir(x)
#define rmdir _rmdir
#else
static const char kDirectorySeparator[] = "/";
#endif  // FIREBASE_PLATFORM_WINDOWS

static const char kFileExtension[] = ".firbin";

UserSecureFakeInternal::UserSecureFakeInternal(const char* domain,
                                               const char* base_path)
    : domain_(domain ? domain : ""),
      base_path_((base_path && base_path[0] != '\0') ? base_path : ".") {
  full_path_ = base_path_ + kDirectorySeparator + domain_;
}

UserSecureFakeInternal::~UserSecureFakeInternal() = default;

std::string UserSecureFakeInternal::LoadUserData(const std::string& app_name) {
  std::lock_guard<std::mutex> lock(GetFakeSecureMutex());
  std::string output;
  std::string filename = GetFilePath(app_name);

  std::ifstream infile;
  infile.open(filename, std::ios::binary);
  if (infile.fail()) {
    LogDebug("LoadUserData: Failed to read %s", filename.c_str());
    return "";
  }

  infile.seekg(0, std::ios::end);
  std::streamoff pos = infile.tellg();
  if (pos <= 0) {
    return "";
  }
  size_t length = static_cast<size_t>(pos);
  infile.seekg(0, std::ios::beg);
  output.resize(length);
  infile.read(&output[0], static_cast<std::streamsize>(length));
  if (infile.fail()) {
    return "";
  }
  infile.close();
  return output;
}

void UserSecureFakeInternal::SaveUserData(const std::string& app_name,
                                          const std::string& user_data) {
  std::lock_guard<std::mutex> lock(GetFakeSecureMutex());
  // Make the directory in case it doesn't already exist, ignoring errors.
  if (mkdir(base_path_.c_str(), 0700) < 0) {
    int error = errno;
    if (error != 0 && error != EEXIST) {
      LogWarning("SaveUserData: Couldn't create directory %s: %d",
                 base_path_.c_str(), error);
    }
  }
  if (mkdir(full_path_.c_str(), 0700) < 0) {
    int error = errno;
    if (error != 0 && error != EEXIST) {
      LogWarning("SaveUserData: Couldn't create directory %s: %d",
                 full_path_.c_str(), error);
    }
  }

  std::string filename = GetFilePath(app_name);

  std::ofstream ofile(filename, std::ios::binary | std::ios::trunc);
  if (ofile.fail()) {
    LogWarning("SaveUserData: Couldn't open file %s for writing",
               filename.c_str());
    return;
  }
  ofile.write(user_data.c_str(),
              static_cast<std::streamsize>(user_data.length()));
  ofile.flush();
  ofile.close();
  if (ofile.fail()) {
    LogWarning("SaveUserData: Failed to write file %s", filename.c_str());
  }
}

void UserSecureFakeInternal::DeleteUserData(const std::string& app_name) {
  std::lock_guard<std::mutex> lock(GetFakeSecureMutex());
  std::string filename = GetFilePath(app_name);
  if (unlink(filename.c_str()) == -1) {
    int error = errno;
    if (error != 0 && error != ENOENT) {
      LogWarning("DeleteUserData: Couldn't remove file %s: %d",
                 filename.c_str(), error);
    }
  }
}

void UserSecureFakeInternal::DeleteAllData() {
  std::lock_guard<std::mutex> lock(GetFakeSecureMutex());
  std::vector<std::string> files_to_delete;
#if FIREBASE_PLATFORM_WINDOWS
  std::string file_spec =
      full_path_ + kDirectorySeparator + "*" + kFileExtension;
  WIN32_FIND_DATAA file_data;
  HANDLE handle = FindFirstFileA(file_spec.c_str(), &file_data);
  if (INVALID_HANDLE_VALUE == handle) {
    DWORD error = GetLastError();
    if (error != ERROR_FILE_NOT_FOUND && error != ERROR_PATH_NOT_FOUND) {
      LogWarning("DeleteAllData: Couldn't find file list matching %s: %lu",
                 file_spec.c_str(), static_cast<unsigned long>(error));
    }
    return;
  }
  do {
    std::string file_path =
        full_path_ + kDirectorySeparator + file_data.cFileName;
    files_to_delete.push_back(file_path);
  } while (FindNextFileA(handle, &file_data));
  FindClose(handle);
#else
  // These are data types defined in the "dirent" header
  DIR* the_folder = opendir(full_path_.c_str());
  if (!the_folder) {
    return;
  }
  struct dirent* next_file;

  const size_t ext_len = strlen(kFileExtension);
  while ((next_file = readdir(the_folder)) != nullptr) {
    // Only delete files matching the file extension.
    size_t name_len = strlen(next_file->d_name);
    if (name_len < ext_len ||
        strcasecmp(next_file->d_name + name_len - ext_len, kFileExtension) !=
            0) {
      continue;
    }
    // Build the path for each file in the folder
    std::string file_path =
        full_path_ + kDirectorySeparator + next_file->d_name;
    files_to_delete.push_back(file_path);
  }
  closedir(the_folder);
#endif
  for (size_t i = 0; i < files_to_delete.size(); ++i) {
    if (unlink(files_to_delete[i].c_str()) == -1) {
      int error = errno;
      if (error != 0 && error != ENOENT) {
        LogWarning("DeleteAllData: Couldn't remove file %s: %d",
                   files_to_delete[i].c_str(), error);
      }
    }
  }
  // Remove the directory if it's empty, ignoring errors.
  if (rmdir(full_path_.c_str()) == -1) {
    int error = errno;
    LogDebug("DeleteAllData: Couldn't remove directory %s: %d",
             full_path_.c_str(), error);
  }
}

std::string UserSecureFakeInternal::GetFilePath(const std::string& app_name) {
  std::string filepath =
      full_path_ + kDirectorySeparator + app_name + kFileExtension;
  return filepath;
}

}  // namespace secure
}  // namespace app
}  // namespace firebase
