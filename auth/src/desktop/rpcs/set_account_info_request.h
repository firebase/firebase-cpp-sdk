/*
 * Copyright 2017 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_RPCS_SET_ACCOUNT_INFO_REQUEST_H_
#define FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_RPCS_SET_ACCOUNT_INFO_REQUEST_H_

#include <memory>
#include <string>

#include "app/src/log.h"
#include "auth/request_generated.h"
#include "auth/request_resource.h"
#include "auth/src/desktop/rpcs/auth_request.h"

namespace firebase {
namespace auth {

class SetAccountInfoRequest : public AuthRequest {
 public:
  static std::unique_ptr<SetAccountInfoRequest> CreateUpdateEmailRequest(
      const char* api_key, const char* email);
  static std::unique_ptr<SetAccountInfoRequest> CreateUpdatePasswordRequest(
      const char* api_key, const char* password);
  static std::unique_ptr<SetAccountInfoRequest>
  CreateLinkWithEmailAndPasswordRequest(const char* api_key, const char* email,
                                        const char* password);
  static std::unique_ptr<SetAccountInfoRequest> CreateUpdateProfileRequest(
      const char* api_key, const char* set_display_name,
      const char* set_photo_url);
  static std::unique_ptr<SetAccountInfoRequest> CreateUnlinkProviderRequest(
      const char* api_key, const char* provider);

  void SetIdToken(const char* const id_token) {
    if (id_token) {
      application_data_->idToken = id_token;
      UpdatePostFields();
    } else {
      LogError("No id token given.");
    }
  }

 private:
  explicit SetAccountInfoRequest(const char* api_key);
  static std::unique_ptr<SetAccountInfoRequest> CreateRequest(
      const char* api_key);
};

}  // namespace auth
}  // namespace firebase

#endif  // FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_RPCS_SET_ACCOUNT_INFO_REQUEST_H_
