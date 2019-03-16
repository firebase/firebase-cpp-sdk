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

#ifndef FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_PROVIDER_USER_INFO_H_
#define FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_PROVIDER_USER_INFO_H_

#include <string>
#include <vector>
#include "auth/src/desktop/user_desktop.h"
#include "auth/src/include/firebase/auth/user.h"

namespace firebase {
namespace auth {

// Simple storage for user info properties, but conforming to UserInfoInterface.
struct UserInfoInterfaceImpl : public UserInfoInterface {
  explicit UserInfoInterfaceImpl(const UserInfoImpl& set_impl)
      : impl(set_impl) {}

  std::string uid() const override { return impl.uid; }
  std::string email() const override { return impl.email; }
  std::string display_name() const override { return impl.display_name; }
  std::string photo_url() const override { return impl.photo_url; }
  std::string provider_id() const override { return impl.provider_id; }
  std::string phone_number() const override { return impl.phone_number; }

  const UserInfoImpl impl;
};

// Extracts data on providers associated with a user from the given response(at
// this point, only Get/SetAccountInfoResponse).
template <class ResponseT>
std::vector<UserInfoImpl> ParseProviderUserInfo(const ResponseT& response);

// Implementation

template <class ResponseT>
std::vector<UserInfoImpl> ParseProviderUserInfo(const ResponseT& response) {
  std::vector<UserInfoImpl> result;
  for (const auto& providerInfo : response.providerUserInfos()) {
    UserInfoImpl user_info;
    user_info.provider_id = providerInfo->providerId;
    user_info.photo_url = providerInfo->photoUrl;
    user_info.display_name = providerInfo->displayName;
    user_info.phone_number = providerInfo->phoneNumber;
    user_info.email = providerInfo->email;
    user_info.uid = providerInfo->federatedId;
    result.push_back(user_info);
  }

  return result;
}

}  // namespace auth
}  // namespace firebase

#endif  // FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_PROVIDER_USER_INFO_H_
