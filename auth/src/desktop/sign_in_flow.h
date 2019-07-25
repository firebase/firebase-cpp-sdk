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

#ifndef FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_SIGN_IN_FLOW_H_
#define FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_SIGN_IN_FLOW_H_

#include <string>
#include "app/src/assert.h"
#include "auth/src/common.h"
#include "auth/src/desktop/auth_data_handle.h"
#include "auth/src/desktop/auth_util.h"
#include "auth/src/desktop/authentication_result.h"
#include "auth/src/desktop/get_additional_user_info.h"
#include "auth/src/desktop/rpcs/get_account_info_request.h"
#include "auth/src/desktop/user_desktop.h"

namespace firebase {
namespace auth {

// Makes a network call to getAccountInfo RPC, creating a request with the given
// access token.
//
// Note: this is a blocking call! It's the caller's responsibility to make sure
// it's invoked on the appropriate thread.
GetAccountInfoResult GetAccountInfo(const AuthData& auth_data,
                                    const std::string& access_token);
GetAccountInfoResult GetAccountInfo(const GetAccountInfoRequest& request);

// Makes a network call to one of the sign-in endpoints (e.g., verifyPassword or
// verifyAssertion), and completes the promise contained within the given
// handle, either successfully or with an error.
//
// Note: this is a blocking call! It's the caller's responsibility to make sure
// it's invoked on the appropriate thread.
template <typename ResponseT, typename FutureResultT, typename RequestT>
void PerformSignInFlow(AuthDataHandle<FutureResultT, RequestT>* handle);

// Parses the given response and calls getAccountInfo endpoint for the user
// contained within the given response to retrieve additional user info.
//
// Note: this is a blocking call! It's the caller's responsibility to make sure
// it's invoked on the appropriate thread.
template <typename ResponseT>
AuthenticationResult CompleteSignInFlow(AuthData* auth_data,
                                        const ResponseT& response);

// Implementation

template <typename ResponseT>
inline AuthenticationResult CompleteSignInFlow(AuthData* const auth_data,
                                               const ResponseT& response) {
  FIREBASE_ASSERT_RETURN(AuthenticationResult(kAuthErrorFailure), auth_data);

  auto auth_result = AuthenticationResult::FromResponse(response);
  if (!auth_result.IsValid()) {
    return auth_result;
  }

  const GetAccountInfoResult user_account_info =
      GetAccountInfo(*auth_data, auth_result.id_token());
  if (!user_account_info.IsValid()) {
    return AuthenticationResult(user_account_info.error());
  }

  auth_result.SetAccountInfo(user_account_info);
  return auth_result;
}

inline AuthenticationResult CompleteAuthenticedUserSignInFlow(
    AuthData* const auth_data,
    const FederatedAuthProvider::AuthenticatedUserData& user_data) {
  FIREBASE_ASSERT_RETURN(AuthenticationResult(kAuthErrorFailure), auth_data);

  auto auth_result = AuthenticationResult::FromAuthenticatedUserData(user_data);
  if (!auth_result.IsValid()) {
    return auth_result;
  }

  const GetAccountInfoResult user_account_info =
      GetAccountInfo(*auth_data, auth_result.id_token());
  if (!user_account_info.IsValid()) {
    return AuthenticationResult(user_account_info.error());
  }

  auth_result.SetAccountInfo(user_account_info);
  return auth_result;
}

template <typename ResponseT, typename FutureResultT, typename RequestT>
inline void PerformSignInFlow(
    AuthDataHandle<FutureResultT, RequestT>* const handle) {
  FIREBASE_ASSERT_RETURN_VOID(handle && handle->request);

  const auto response = GetResponse<ResponseT>(*handle->request);
  const AuthenticationResult auth_result =
      CompleteSignInFlow(handle->auth_data, response);

  if (auth_result.IsValid()) {
    const SignInResult sign_in_result =
        auth_result.SetAsCurrentUser(handle->auth_data);
    CompletePromise(&handle->promise, sign_in_result);
  } else {
    FailPromise(&handle->promise, auth_result.error());
  }
}

}  // namespace auth
}  // namespace firebase

#endif  // FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_SIGN_IN_FLOW_H_
