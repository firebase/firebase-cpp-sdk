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

#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>

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
  mkdir(secure_path_.c_str(), 0700);

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
  std::remove(filename.c_str());
}

void UserSecureFakeInternal::DeleteAllData() {
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
    remove(filepath.c_str());
  }
  closedir(theFolder);

  // Remove the directory if it's empty, ignoring errors.
  rmdir(secure_path_.c_str());
}

std::string UserSecureFakeInternal::GetFilePath(const std::string& app_name) {
  std::string filepath = secure_path_ + "/" + app_name + "_bin";
  return filepath;
}

}  // namespace secure
}  // namespace auth
}  // namespace firebase
