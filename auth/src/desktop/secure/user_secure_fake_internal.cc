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

#include "auth/src/desktop/secure/user_secure_fake_internal.h"

#if defined(_WIN32)
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#endif  // defined(_WIN32)

#include <cstdio>
#include <fstream>

namespace firebase {
namespace auth {
namespace secure {

UserSecureFakeInternal::UserSecureFakeInternal(const char* secure_path)
    : secure_path_(secure_path) {}

UserSecureFakeInternal::~UserSecureFakeInternal() {}

std::string UserSecureFakeInternal::LoadUserData(const std::string& app_name) {
  std::string output;
  std::string filename = GetFilePath(app_name);

  std::ifstream infile;
  infile.open(filename, std::ios::binary);
  if (infile.fail()) {
    return "";
  }

  infile.seekg(0, std::ios::end);
  int64_t length = infile.tellg();
  infile.seekg(0, std::ios::beg);
  output.resize(length);
  infile.read(&*output.begin(), length);
  if (infile.fail()) {
    return "";
  }
  infile.close();
  return output;
}

void UserSecureFakeInternal::SaveUserData(const std::string& app_name,
                                          const std::string& user_data) {
  // Make the directory in case it doesn't already exist, ignoring errors.
#if defined(_WIN32)
  CreateDirectory(secure_path_.c_str(), NULL);
#else
  mkdir(secure_path_.c_str(), 0700);
#endif

  std::string filename = GetFilePath(app_name);

  std::ofstream ofile(filename, std::ios::binary);
  ofile.write(user_data.c_str(), user_data.length());
  ofile.close();
}

void UserSecureFakeInternal::DeleteUserData(const std::string& app_name) {
  std::string filename = GetFilePath(app_name);
  std::ifstream infile;
  infile.open(filename, std::ios::binary);
  if (infile.fail()) {
    return;
  }
  infile.close();
#if defined(_WIN32)
  DeleteFile(filename.c_str());
#else
  unlink(filename.c_str());
#endif  // defined(_WIN32)
}

void UserSecureFakeInternal::DeleteAllData() {
#if defined(_WIN32)
  WIN32_FIND_DATA file_data;
  HANDLE handle = FindFirstFile(secure_path_.c_str(), &file_data);
  if (INVALID_HANDLE_VALUE == handle) {
    return;
  }
  DeleteFile(file_data.cFileName);
  while (FindNextFile(handle, &file_data)) {
    DeleteFile(file_data.cFileName);
  }
  FindClose(handle);
  RemoveDirectory(secure_path_.c_str());
#else
  // These are data types defined in the "dirent" header
  DIR* theFolder = opendir(secure_path_.c_str());
  if (!theFolder) {
    return;
  }
  struct dirent* next_file;

  while ((next_file = readdir(theFolder)) != nullptr) {
    // build the path for each file in the folder
    std::string filepath = secure_path_ + "/";
    filepath.append(next_file->d_name);
    unlink(filepath.c_str());
  }
  closedir(theFolder);

  // Remove the directory if it's empty, ignoring errors.
  rmdir(secure_path_.c_str());
#endif
}

std::string UserSecureFakeInternal::GetFilePath(const std::string& app_name) {
  std::string filepath = secure_path_ + "/" + app_name + "_bin";
  return filepath;
}

}  // namespace secure
}  // namespace auth
}  // namespace firebase
