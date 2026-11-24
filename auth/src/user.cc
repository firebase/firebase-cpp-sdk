/*
 * Copyright 2016 Google LLC
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

#include "auth/src/common.h"

namespace firebase {
namespace auth {

USER_RESULT_FN(User, GetToken, std::string)
USER_RESULT_FN(User, GetIdTokenResult, IdTokenResult)
USER_RESULT_FN(User, UpdatePassword, void)
USER_RESULT_FN(User, LinkWithCredential, AuthResult)
USER_RESULT_FN(User, Reauthenticate, void)
USER_RESULT_FN(User, ReauthenticateAndRetrieveData, AuthResult)
USER_RESULT_FN(User, SendEmailVerification, void)
USER_RESULT_FN(User, SendEmailVerificationBeforeUpdatingEmail, void)
USER_RESULT_FN(User, Unlink, AuthResult)
USER_RESULT_FN(User, UpdateUserProfile, void)
USER_RESULT_FN(User, Reload, void)
USER_RESULT_FN(User, Delete, void)
USER_RESULT_FN(User, UpdatePhoneNumberCredential, User)

#if defined(INTERNAL_EXPERIMENTAL)
Future<std::string> User::GetTokenThreadSafe(bool force_refresh) {
  auto data = auth_data_.lock();
  if (!data) return Future<std::string>();
  Future<std::string> future = GetToken(force_refresh);
  if (future.status() != kFutureStatusPending) {
    return future;
  } else {
    FutureBase base = data->future_impl.LastResultProxy(kUserFn_GetToken);
    const FutureBase& rFuture = base;
    return static_cast<const Future<std::string>&>(rFuture);
  }
}

Future<IdTokenResult> User::GetIdTokenResultThreadSafe(bool force_refresh) {
  auto data = auth_data_.lock();
  if (!data) return Future<IdTokenResult>();
  Future<IdTokenResult> future = GetIdTokenResult(force_refresh);
  if (future.status() != kFutureStatusPending) {
    return future;
  } else {
    FutureBase base =
        data->future_impl.LastResultProxy(kUserFn_GetIdTokenResult);
    const FutureBase& rFuture = base;
    return static_cast<const Future<IdTokenResult>&>(rFuture);
  }
}
#endif  // defined(INTERNAL_EXPERIMENTAL)

// Non-inline implementation of UserInfoInterface's virtual destructor
// to prevent its vtable being emitted in each translation unit.
UserInfoInterface::~UserInfoInterface() {}

std::string UserInfoInterface::uid() const { return uid_; }
std::string UserInfoInterface::email() const { return email_; }
std::string UserInfoInterface::display_name() const { return display_name_; }
std::string UserInfoInterface::photo_url() const { return photo_url_; }
std::string UserInfoInterface::provider_id() const { return provider_id_; }
std::string UserInfoInterface::phone_number() const { return phone_number_; }

}  // namespace auth
}  // namespace firebase
