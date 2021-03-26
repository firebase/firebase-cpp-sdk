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

#include "app/rest/transport_builder.h"
#include "auth/src/desktop/rpcs/sign_up_new_user_request.h"
#include "auth/src/desktop/rpcs/sign_up_new_user_response.h"

namespace firebase {
namespace auth {

bool GetNewUserLocalIdAndIdToken(const char* const api_key,
                                  std::string* local_id,
                                  std::string* id_token) {
  SignUpNewUserRequest request(api_key);
  SignUpNewUserResponse response;

  firebase::rest::CreateTransport()->Perform(request, &response);

  if (response.status() != 200) {
    return false;
  }

  *local_id = response.local_id();
  *id_token = response.id_token();
  return true;
}

bool GetNewUserLocalIdAndRefreshToken(const char* const api_key,
                                 std::string* local_id,
                                 std::string* refresh_token) {
  SignUpNewUserRequest request(api_key);
  SignUpNewUserResponse response;

  firebase::rest::CreateTransport()->Perform(request, &response);

  if (response.status() != 200) {
    return false;
  }

  *local_id = response.local_id();
  *refresh_token = response.refresh_token();
  return true;
}

std::string SignUpNewUserAndGetIdToken(const char* const api_key,
                                      const char* const email) {
  SignUpNewUserRequest request(api_key, email, "fake_password", "");
  SignUpNewUserResponse response;

  firebase::rest::CreateTransport()->Perform(request, &response);
  if (response.status() != 200) {
    return "";
  }
  return response.id_token();
}

}  // namespace auth
}  // namespace firebase
