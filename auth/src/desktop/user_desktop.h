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

#ifndef FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_USER_DESKTOP_H_
#define FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_USER_DESKTOP_H_

#include <firebase/auth.h>

#include <ctime>
#include <string>
#include <vector>

#include "app/src/secure/user_secure_manager.h"

namespace firebase {
namespace auth {

// LINT.IfChange
// The desktop-specific UserInfo implementation.
struct UserInfoImpl {
  // Note: since Visual Studio 2013 and below don't autogenerate move
  // constructors/move assignment operators, this class and UserData below will
  // be copied on these compilers in cases where more conformant compilers will
  // move. Implementing move operations will lead to a chain effect, though,
  // because once a user-defined move constructor is supplied, conformant
  // compilers will refuse to generate copy constructors. In practice, move is
  // only triggered to get the previous User state during sign in/sign out,
  // which is a rare operation.

  // The user's ID, unique to the Firebase project.
  std::string uid;

  // The associated email, if any.
  std::string email;

  // The display name, if any.
  std::string display_name;

  // Associated photo url, if any.
  std::string photo_url;

  // A provider ID for the user e.g. "Facebook".
  std::string provider_id;

  // The user's phone number, if any.
  std::string phone_number;
};

// The desktop-specific User implementation: simply defined as struct that
// stores all relevant information. We cannot add those in class User because it
// is defined in the platform-independent public header file.
struct UserData : public UserInfoImpl {
  UserData()
      : is_anonymous(false),
        is_email_verified(false),
        access_token_expiration_date(0),
        has_email_password_credential(false) {}

  // Whether is anonymous.
  bool is_anonymous;

  // Whether email is verified.
  bool is_email_verified;

  // Tokens for authentication and authorization.
  std::string id_token;  // an authorization code or access_token
  std::string refresh_token;
  std::string access_token;

  // The approximate expiration date of the access token.
  std::time_t access_token_expiration_date;

  // Whether or not the user can be authenticated by provider 'password'.
  bool has_email_password_credential;

  /// The last sign in UTC timestamp in milliseconds.
  /// See https://en.wikipedia.org/wiki/Unix_time for details of UTC.
  uint64_t last_sign_in_timestamp;

  /// The Firebase user creation UTC timestamp in milliseconds.
  uint64_t creation_timestamp;
};

// LINT.ThenChange(//depot_firebase_cpp/auth/client/cpp/src/desktop/user_data.fbs)

// Class to save/load UserData for desktop. UserData will be persisted in
// os specific secret locations for security reason.
class UserDataPersist : public firebase::auth::AuthStateListener {
 public:
  UserDataPersist(const char* app_id);
  ~UserDataPersist() {}

  // Overloaded constructor to set the internal instance.
  explicit UserDataPersist(
      UniquePtr<firebase::app::secure::UserSecureManager> user_secure_manager);

  void OnAuthStateChanged(Auth* auth) override;

  Future<void> SaveUserData(AuthData* auth_data);
  Future<std::string> LoadUserData(AuthData* auth_data);
  Future<void> DeleteUserData(AuthData* auth_data);

 private:
  UniquePtr<firebase::app::secure::UserSecureManager> user_secure_manager_;
};

}  // namespace auth
}  // namespace firebase

#endif  // FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_USER_DESKTOP_H_
