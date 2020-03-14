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

#ifndef FIREBASE_APP_CLIENT_CPP_SRC_SECURE_USER_SECURE_LINUX_INTERNAL_H_
#define FIREBASE_APP_CLIENT_CPP_SRC_SECURE_USER_SECURE_LINUX_INTERNAL_H_

#include <string>

#include "app/src/secure/user_secure_data_handle.h"
#include "app/src/secure/user_secure_internal.h"
#include "libsecret/secret.h"

namespace firebase {
namespace app {
namespace secure {

// Linux specific implementation for the secure manager of user data.
class UserSecureLinuxInternal : public UserSecureInternal {
 public:
  ~UserSecureLinuxInternal() override;

  // Overloaded constructor to set the storage schema for keys.
  // domain = Library name (e.g. "auth", "iid", "fis")
  // key_namespace = app ID (e.g. "com.mycompany.myapp");
  UserSecureLinuxInternal(const char* domain, const char* key_namespace);

  std::string LoadUserData(const std::string& app_name) override;

  void SaveUserData(const std::string& app_name,
                    const std::string& user_data) override;

  void DeleteUserData(const std::string& app_name) override;

  void DeleteAllData() override;

 private:
  // Check if there is an error, log it and free it if there is.
  // Returns true iff there was an error.
  bool CheckForError(GError** error, const char* function_name);

  const std::string domain_;
  const std::string key_namespace_;
  SecretSchema storage_schema_;
  int known_error_code_;  // If nonzero, this error was already logged, so
                          // don't be too spammy about it.
};

}  // namespace secure
}  // namespace app
}  // namespace firebase

#endif  // FIREBASE_APP_CLIENT_CPP_SRC_SECURE_USER_SECURE_LINUX_INTERNAL_H_
