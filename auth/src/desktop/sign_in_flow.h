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

#ifndef FIREBASE_AUTH_SRC_DESKTOP_SIGN_IN_FLOW_H_
#define FIREBASE_AUTH_SRC_DESKTOP_SIGN_IN_FLOW_H_

#include <assert.h>

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

GetAccountInfoResult GetAccountInfo(const AuthData& auth_data,
                                    const std::string& access_token);
GetAccountInfoResult GetAccountInfo(GetAccountInfoRequest& request);

inline AuthenticationResult CompleteSignInFlowWithAuthResult(
    AuthData* const auth_data, AuthenticationResult auth_result) {
  if (!auth_result.IsValid()) return auth_result;
  const GetAccountInfoResult user_account_info =
      GetAccountInfo(*auth_data, auth_result.id_token());
  if (!user_account_info.IsValid()) {
    return AuthenticationResult(user_account_info.error());
  }
  auth_result.SetAccountInfo(user_account_info);
  return auth_result;
}

template <typename ResponseT>
inline AuthenticationResult CompleteSignInFlow(AuthData* const auth_data,
                                               const ResponseT& response) {
  FIREBASE_ASSERT_RETURN(AuthenticationResult(kAuthErrorFailure), auth_data);
  return CompleteSignInFlowWithAuthResult(
      auth_data, AuthenticationResult::FromResponse(response));
}

inline AuthenticationResult CompleteAuthenticatedUserSignInFlow(
    AuthData* const auth_data,
    const FederatedAuthProvider::AuthenticatedUserData& user_data) {
  FIREBASE_ASSERT_RETURN(AuthenticationResult(kAuthErrorFailure), auth_data);
  return CompleteSignInFlowWithAuthResult(
      auth_data, AuthenticationResult::FromAuthenticatedUserData(user_data));
}

inline AuthenticationResult CompleteAuthenticedUserSignInFlow(
    AuthData* const auth_data,
    const FederatedAuthProvider::AuthenticatedUserData& user_data) {
  return CompleteAuthenticatedUserSignInFlow(auth_data, user_data);
}

template <typename ResponseT, typename FutureResultT, typename RequestT>
inline void PerformSignInFlow(
    AuthDataHandle<FutureResultT, RequestT>* const handle) {
  FIREBASE_ASSERT_RETURN_VOID(handle && handle->request);
  const auto response = GetResponse<ResponseT>(*handle->request);
  const AuthenticationResult auth_response =
      CompleteSignInFlow(handle->auth_data, response);
  if (auth_response.IsValid()) {
    CompletePromise(&handle->promise,
                    auth_response.SetAsCurrentUser(handle->auth_data));
  } else {
    FailPromise(&handle->promise, auth_response.error());
  }
}

}  // namespace auth
}  // namespace firebase

#endif  // FIREBASE_AUTH_SRC_DESKTOP_SIGN_IN_FLOW_H_
