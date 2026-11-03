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

#ifndef FIREBASE_APP_SRC_SECURE_USER_SECURE_DARWIN_INTERNAL_H_
#define FIREBASE_APP_SRC_SECURE_USER_SECURE_DARWIN_INTERNAL_H_

#include <string>

#include "app/src/secure/user_secure_internal.h"

namespace firebase::app::secure {

// Darwin-specific implementation for the secure manager of user data.
//
// Stores the secure data in the user's default keychain.
//
// Also sets an entry in NSUserDefaults for the app the first time data is
// written; unless that entry is set, we won't check the keychain (if we do,
// their system will prompt the user for a password if we try to access the
// keychain before writing to it, which is not a great user experience).
class UserSecureDarwinInternal : public UserSecureInternal {
 public:
  // domain = Library name (e.g. "auth", "iid", "fis")
  // service = app ID (e.g. "com.mycompany.myapp");
  UserSecureDarwinInternal(const char* domain, const char* service);

  ~UserSecureDarwinInternal() override;

  UserSecureDarwinInternal(const UserSecureDarwinInternal&) = delete;
  UserSecureDarwinInternal& operator=(const UserSecureDarwinInternal&) = delete;
  UserSecureDarwinInternal(UserSecureDarwinInternal&&) = delete;
  UserSecureDarwinInternal& operator=(UserSecureDarwinInternal&&) = delete;

  std::string LoadUserData(const std::string& app_name) override;

  void SaveUserData(const std::string& app_name,
                    const std::string& user_data) override;

  void DeleteUserData(const std::string& app_name) override;

  void DeleteAllData() override;

 private:
  // Check or update NSUserDefaults flag indicating whether secure data has been
  // written for this service/domain.
  bool UserHasSecureData() const;
  void SetUserHasSecureData(bool has_data);

  // Delete either a single key, or (if app_name is null) all keys.
  // func_name is used for error messages.
  void DeleteData(const char* app_name, const char* func_name);

  std::string GetKeystoreLocation(const std::string& app) const;

  const std::string domain_;
  const std::string service_;
  const std::string user_defaults_key_;
};

}  // namespace firebase::app::secure

#endif  // FIREBASE_APP_SRC_SECURE_USER_SECURE_DARWIN_INTERNAL_H_
