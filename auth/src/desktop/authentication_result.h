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

#ifndef FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_AUTHENTICATION_RESULT_H_
#define FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_AUTHENTICATION_RESULT_H_

#include <cstddef>
#include <string>

#include "app/rest/util.h"
#include "app/src/log.h"
#include "auth/src/common.h"
#include "auth/src/data.h"
#include "auth/src/desktop/get_account_info_result.h"
#include "auth/src/desktop/get_additional_user_info.h"
#include "auth/src/desktop/rpcs/sign_up_new_user_response.h"
#include "auth/src/desktop/user_desktop.h"
#include "auth/src/include/firebase/auth.h"

namespace firebase {
namespace auth {

// Holds the results of a sign in operation (such as verifyAssertion), to be
// applied to the Auth state.
class AuthenticationResult {
 public:
  // Creates an immutable invalid AuthenticationResult.
  explicit AuthenticationResult(AuthError const error) : error_(error) {
    if (error_ == kAuthErrorNone) {
      LogDebug(
          "When creating invalid AuthenticationResult, use error other than "
          "kAuthErrorNone");
    }
  }

  // Creates sign-in result corresponding to the given response; will be invalid
  // (!IsValid()) if the response contains an error.
  template <typename ResponseT>
  static AuthenticationResult FromResponse(const ResponseT& response);

  // Creates a sign-in result corresponding to the provided user data.
  static AuthenticationResult FromAuthenticatedUserData(
      const FederatedAuthProvider::AuthenticatedUserData& user_data);

  // Signs out the currently signed-in user; no-op if no user has been signed
  // in. Updates to AuthData are done in a thread-safe manner.
  // Listeners will be notified if a user has been previously signed in.
  static void SignOut(AuthData* auth_data);

  // Whether the sign in operation was successful.
  bool IsValid() const { return error_ == kAuthErrorNone; }
  // Error code associated with this sign-in operation.
  AuthError error() const { return error_; }

  // Returns uid of the user associated with this sign in operation; blank if
  // sign in failed.
  std::string uid() const;

  // Returns access token of the user associated with this sign in operation;
  // blank if sign in failed.
  std::string id_token() const { return user_impl_.id_token; }

  // Sets the currently signed in user to the one associated with this sign-in
  // operation, and updates listeners if the user changed.
  //
  // Updates to AuthData are done in a thread-safe manner.
  SignInResult SetAsCurrentUser(AuthData* auth_data) const;

  // Merges user information from the given response into current state.
  // The new response will override fields read from any previous response, but
  // will not reset any fields from the previous response that are absent from
  // the new response.
  void SetAccountInfo(const GetAccountInfoResult& info);

 private:
  AuthenticationResult()
      : error_(kAuthErrorNone), user_account_info_(kAuthErrorFailure) {}

  const AuthError error_;

  UserData user_impl_;
  AdditionalUserInfo info_;

  GetAccountInfoResult user_account_info_;
};

// Implementation

namespace detail {

template <typename T>
bool IsUserAnonymous(const T& response) {
  return false;
}

// Only when using signUpNewUser API is it possible for the user to be
// anonymous.
template <>
inline bool IsUserAnonymous(const SignUpNewUserResponse& response) {
  return response.IsAnonymousUser();
}

template <typename T>
bool NeedsConfirmation(const T& response) {
  return false;
}

template <>
inline bool NeedsConfirmation(const VerifyAssertionResponse& response) {
  return response.need_confirmation();
}

}  // namespace detail

template <typename ResponseT>
inline AuthenticationResult AuthenticationResult::FromResponse(
    const ResponseT& response) {
  if (!response.IsSuccessful()) {
    return AuthenticationResult(response.error_code());
  }
  if (detail::NeedsConfirmation(response)) {
    return AuthenticationResult(
        kAuthErrorAccountExistsWithDifferentCredentials);
  }

  AuthenticationResult result;

  result.user_impl_.is_anonymous = detail::IsUserAnonymous(response);
  result.user_impl_.uid = response.local_id();
  result.user_impl_.id_token = response.id_token();
  result.user_impl_.refresh_token = response.refresh_token();
  result.user_impl_.provider_id = "Firebase";
  // Since we set returnSecureToken to true by default in the REST request
  // the access token (as id token) and expiration date are also in the
  // response. We update them as well and do not make another REST request
  // for access token.
  result.user_impl_.access_token = response.id_token();
  result.user_impl_.access_token_expiration_date =
      response.fetch_time() + response.expires_in();

  result.info_ = GetAdditionalUserInfo(response);

  return result;
}

inline AuthenticationResult AuthenticationResult::FromAuthenticatedUserData(
    const FederatedAuthProvider::AuthenticatedUserData& user_data) {
  AuthenticationResult result;

  result.user_impl_.is_anonymous = false;
  result.user_impl_.uid = user_data.uid;
  result.user_impl_.id_token = user_data.access_token;
  result.user_impl_.refresh_token = user_data.refresh_token;
  result.user_impl_.provider_id = user_data.provider_id;
  result.user_impl_.access_token = user_data.access_token;
  result.user_impl_.access_token_expiration_date =
      std::time(nullptr) + user_data.token_expires_in_seconds;

  result.info_ = GetAdditionalUserInfo(user_data);

  return result;
}

}  // namespace auth
}  // namespace firebase
#endif  // FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_AUTHENTICATION_RESULT_H_
