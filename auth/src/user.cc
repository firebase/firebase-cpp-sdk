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

#if !defined(SWIG)
User::User(User&& rhs) noexcept : auth_data_(rhs.auth_data_) {
  rhs.auth_data_ = nullptr;
}

User& User::operator=(User&& rhs) noexcept {
  if (this != &rhs) {
    auth_data_ = rhs.auth_data_;
    rhs.auth_data_ = nullptr;
  }
  return *this;
}
#endif  // !defined(SWIG)

AUTH_RESULT_FN(User, GetToken, std::string)
AUTH_RESULT_FN(User, GetIdTokenResult, IdTokenResult)
AUTH_RESULT_FN(User, UpdatePassword, void)
AUTH_RESULT_FN(User, LinkWithCredential, AuthResult)
AUTH_RESULT_FN(User, LinkWithProvider, AuthResult)
AUTH_RESULT_FN(User, Reauthenticate, void)
AUTH_RESULT_FN(User, ReauthenticateAndRetrieveData, AuthResult)
AUTH_RESULT_FN(User, ReauthenticateWithProvider, AuthResult)
AUTH_RESULT_FN(User, SendEmailVerification, void)
AUTH_RESULT_FN(User, SendEmailVerificationBeforeUpdatingEmail, void)
AUTH_RESULT_FN(User, Unlink, AuthResult)
AUTH_RESULT_FN(User, UpdateUserProfile, void)
AUTH_RESULT_FN(User, Reload, void)
AUTH_RESULT_FN(User, Delete, void)
AUTH_RESULT_FN(User, UpdatePhoneNumberCredential, User)

Future<std::string> User::GetTokenThreadSafe(bool force_refresh) {
  if (!auth_data_) {
    return Future<std::string>();
  }
  MutexLock lock(auth_data_->future_impl.mutex());
  Future<std::string> future = GetToken(force_refresh);
  if (future.status() != kFutureStatusPending) {
    return future;
  }
  FutureBase base = auth_data_->future_impl.LastResultProxy(kUserFn_GetToken);
  const FutureBase& r_future = base;
  return static_cast<const Future<std::string>&>(r_future);
}

Future<IdTokenResult> User::GetIdTokenResultThreadSafe(bool force_refresh) {
  if (!auth_data_) {
    return Future<IdTokenResult>();
  }
  MutexLock lock(auth_data_->future_impl.mutex());
  Future<IdTokenResult> future = GetIdTokenResult(force_refresh);
  if (future.status() != kFutureStatusPending) {
    return future;
  }
  FutureBase base =
      auth_data_->future_impl.LastResultProxy(kUserFn_GetIdTokenResult);
  const FutureBase& r_future = base;
  return static_cast<const Future<IdTokenResult>&>(r_future);
}

// Non-inline implementation of UserInfoInterface's virtual destructor
// to prevent its vtable being emitted in each translation unit.
UserInfoInterface::~UserInfoInterface() = default;

std::string UserInfoInterface::uid() const { return uid_; }
std::string UserInfoInterface::email() const { return email_; }
std::string UserInfoInterface::display_name() const { return display_name_; }
std::string UserInfoInterface::photo_url() const { return photo_url_; }
std::string UserInfoInterface::provider_id() const { return provider_id_; }
std::string UserInfoInterface::phone_number() const { return phone_number_; }

}  // namespace auth
}  // namespace firebase
