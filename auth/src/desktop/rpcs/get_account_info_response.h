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

#ifndef FIREBASE_AUTH_SRC_DESKTOP_RPCS_GET_ACCOUNT_INFO_RESPONSE_H_
#define FIREBASE_AUTH_SRC_DESKTOP_RPCS_GET_ACCOUNT_INFO_RESPONSE_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "auth/src/desktop/rpcs/auth_response.h"
#include "auth/src/desktop/visual_studio_compatibility.h"

namespace firebase {
namespace auth {

class GetAccountInfoResponse : public AuthResponse {
 public:
  DEFAULT_AND_MOVE_CTRS_NO_CLASS_MEMBERS(GetAccountInfoResponse, AuthResponse)

  const std::string& local_id() const {
    return user() ? user()->localId : EmptyString();
  }

  const std::string& display_name() const {
    return user() ? user()->displayName : EmptyString();
  }

  const std::string& email() const {
    return user() ? user()->email : EmptyString();
  }

  const std::string& photo_url() const {
    return user() ? user()->photoUrl : EmptyString();
  }

  bool email_verified() const {
    return user() ? user()->emailVerified : false;
  }

  const std::string& password_hash() const {
    return user() ? user()->passwordHash : EmptyString();
  }

  const std::string& phone_number() const {
    return user() ? user()->phoneNumber : EmptyString();
  }

  uint64_t last_login_at() const {
    return user() ? user()->lastLoginAt : 0;
  }

  uint64_t created_at() const {
    return user() ? user()->createdAt : 0;
  }

  const std::vector<flatbuffers::unique_ptr<fbs::ProviderUserInfoT>>&
   providerUserInfos() const {
    if (const auto* u = user()) {
      return u->providerUserInfo;
    }
    static const auto* const kFallback =
        new std::vector<flatbuffers::unique_ptr<fbs::ProviderUserInfoT>>();
    return *kFallback;
  }

 private:
  const fbs::UserT* user() const {
    return (application_data_ && !application_data_->users.empty())
               ? application_data_->users[0].get()
               : nullptr;
  }

  static const std::string& EmptyString() {
    static const std::string* const kEmpty = new std::string();
    return *kEmpty;
  }
};

}  // namespace auth
}  // namespace firebase

#endif  // FIREBASE_AUTH_SRC_DESKTOP_RPCS_GET_ACCOUNT_INFO_RESPONSE_H_
