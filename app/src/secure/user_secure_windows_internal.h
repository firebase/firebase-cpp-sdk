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

#ifndef FIREBASE_APP_CLIENT_CPP_SRC_SECURE_USER_SECURE_WINDOWS_INTERNAL_H_
#define FIREBASE_APP_CLIENT_CPP_SRC_SECURE_USER_SECURE_WINDOWS_INTERNAL_H_

#include <string>

#include "app/src/secure/user_secure_internal.h"

namespace firebase {
namespace app {
namespace secure {

// Windows specific implementation for the secure manager of user data.
class UserSecureWindowsInternal : public UserSecureInternal {
 public:
  // domain = Library name (e.g. "auth", "iid", "fis")
  // key_namespace = app ID (e.g. "com.mycompany.myapp");
  UserSecureWindowsInternal(const char* domain, const char* key_namespace);

  ~UserSecureWindowsInternal() override;

  std::string LoadUserData(const std::string& app_name) override;

  void SaveUserData(const std::string& app_name,
                    const std::string& user_data) override;

  void DeleteUserData(const std::string& app_name) override;

  void DeleteAllData() override;

 private:
  std::string GetTargetName(const std::string& app_name);
  std::string GetTargetName(const std::string& app_name, size_t idx);

  const std::string domain_;
  std::string namespace_;
};

}  // namespace secure
}  // namespace app
}  // namespace firebase

#endif  // FIREBASE_APP_CLIENT_CPP_SRC_SECURE_USER_SECURE_WINDOWS_INTERNAL_H_
