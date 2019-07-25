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

#ifndef FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_GET_ADDITIONAL_USER_INFO_H_
#define FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_GET_ADDITIONAL_USER_INFO_H_

#include <map>
#include <string>

#include "app/src/include/firebase/variant.h"
#include "auth/src/common.h"
#include "auth/src/data.h"
#include "auth/src/desktop/auth_constants.h"
#include "auth/src/desktop/rpcs/verify_assertion_response.h"
#include "auth/src/include/firebase/auth.h"

namespace firebase {
namespace auth {

template <class ResponseT>
AdditionalUserInfo GetAdditionalUserInfo(const ResponseT& response);

// Implementation

namespace detail {

std::map<Variant, Variant> ParseUserProfile(const std::string& json);
}  // namespace detail

template <typename ResponseT>
inline AdditionalUserInfo GetAdditionalUserInfo(const ResponseT& /*unused*/) {
  return AdditionalUserInfo();
}

inline std::string ParseFieldFromRawUserInfo(AdditionalUserInfo* info,
                                             const std::string& key) {
  assert(info);
  if (info->profile.count(key) > 0 && info->profile[key].is_string()) {
    return info->profile[key].string_value();
  }
  return "";
}

inline void ParseFieldsFromRawUserInfo(AdditionalUserInfo* info) {
  assert(info);
  if (info->provider_id == kGitHubAuthProviderId) {
    info->user_name = ParseFieldFromRawUserInfo(info, "login");
  } else if (info->provider_id == kTwitterAuthProviderId) {
    info->user_name = ParseFieldFromRawUserInfo(info, "screen_name");
  }
}

inline AdditionalUserInfo GetAdditionalUserInfo(
    const FederatedAuthProvider::AuthenticatedUserData& user_data) {
  AdditionalUserInfo info;
  info.provider_id = user_data.provider_id;
  info.profile = user_data.raw_user_info;
  ParseFieldsFromRawUserInfo(&info);
  return info;
}

template <>
inline AdditionalUserInfo GetAdditionalUserInfo(
    const VerifyAssertionResponse& response) {
  AdditionalUserInfo info;
  info.provider_id = response.provider_id();
  info.profile = detail::ParseUserProfile(response.raw_user_info());
  ParseFieldsFromRawUserInfo(&info);
  return info;
}

}  // namespace auth
}  // namespace firebase
#endif  // FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_GET_ADDITIONAL_USER_INFO_H_
