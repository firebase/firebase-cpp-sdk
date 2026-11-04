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

#ifndef FIREBASE_AUTH_SRC_DESKTOP_USER_DESKTOP_H_
#define FIREBASE_AUTH_SRC_DESKTOP_USER_DESKTOP_H_

#include <firebase/auth.h>
#include <stdint.h>

#include <ctime>
#include <memory>
#include <string>
#include <vector>

#include "app/src/secure/user_secure_manager.h"

namespace firebase {
namespace auth {

// LINT.IfChange
struct UserInfoImpl {
  std::string uid;
  std::string email;
  std::string display_name;
  std::string photo_url;
  std::string provider_id;
  std::string phone_number;
};

struct UserData : public UserInfoImpl {
  bool is_anonymous = false;
  bool is_email_verified = false;
  std::string id_token;
  std::string refresh_token;
  std::string access_token;
  std::time_t access_token_expiration_date = 0;
  bool has_email_password_credential = false;
  uint64_t last_sign_in_timestamp = 0;
  uint64_t creation_timestamp = 0;
};
// LINT.ThenChange(auth/src/desktop/user_data.fbs)

void TriggerSaveUserFlow(AuthData* auth_data);

class UserDataPersist : public firebase::auth::AuthStateListener {
 public:
  explicit UserDataPersist(const char* app_id);
  ~UserDataPersist() override = default;

  explicit UserDataPersist(
      std::unique_ptr<firebase::app::secure::UserSecureManager>
          user_secure_manager);

  void OnAuthStateChanged(Auth* auth) override;

  Future<void> SaveUserData(AuthData* auth_data);
  Future<std::string> LoadUserData(AuthData* auth_data);
  Future<void> DeleteUserData(AuthData* auth_data);

 private:
  std::unique_ptr<firebase::app::secure::UserSecureManager>
      user_secure_manager_;
};

}  // namespace auth
}  // namespace firebase

#endif  // FIREBASE_AUTH_SRC_DESKTOP_USER_DESKTOP_H_
