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

#include "auth/src/desktop/sign_in_flow.h"

#include "auth/src/desktop/rpcs/get_account_info_request.h"
#include "auth/src/desktop/rpcs/get_account_info_response.h"

namespace firebase {
namespace auth {

GetAccountInfoResult GetAccountInfo(const GetAccountInfoRequest& request) {
  auto response = GetResponse<GetAccountInfoResponse>(request);
  return GetAccountInfoResult::FromResponse(response);
}

GetAccountInfoResult GetAccountInfo(const AuthData& auth_data,
                                    const std::string& access_token) {
  const GetAccountInfoRequest request(GetApiKey(auth_data),
                                      access_token.c_str());
  return GetAccountInfo(request);
}

}  // namespace auth
}  // namespace firebase
