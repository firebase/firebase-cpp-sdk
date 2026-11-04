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

#include <utility>

#include "auth/src/common.h"

namespace firebase {
namespace auth {

User::User(const User& user) { auth_data_ = user.auth_data_; }

User& User::operator=(const User& user) {
  auth_data_ = user.auth_data_;
  return *this;
}

User::~User() { auth_data_ = nullptr; }

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

namespace {
template <typename R, typename Fn, typename... Args>
Future<R> RunUserPromiseFn(AuthData* auth_data, int fn_id, Fn fn,
                           Args&&... args) {
  if (!auth_data) return Future<R>();
  Promise<R> promise(&auth_data->future_impl, fn_id);
  fn(auth_data, promise, std::forward<Args>(args)...);
  return promise.future();
}
}  // namespace

Future<std::string> User::GetTokenThreadSafe(bool force_refresh) {
  if (!auth_data_) return Future<std::string>();
  MutexLock lock(auth_data_->future_impl.mutex());
  Future<std::string> future = GetToken(force_refresh);
  if (future.status() != kFutureStatusPending) return future;
  FutureBase base = auth_data_->future_impl.LastResultProxy(kUserFn_GetToken);
  const FutureBase& r_future = base;
  return static_cast<const Future<std::string>&>(r_future);
}

Future<IdTokenResult> User::GetIdTokenResultThreadSafe(bool force_refresh) {
  if (!auth_data_) return Future<IdTokenResult>();
  MutexLock lock(auth_data_->future_impl.mutex());
  Future<IdTokenResult> future = GetIdTokenResult(force_refresh);
  if (future.status() != kFutureStatusPending) return future;
  FutureBase base =
      auth_data_->future_impl.LastResultProxy(kUserFn_GetIdTokenResult);
  const FutureBase& r_future = base;
  return static_cast<const Future<IdTokenResult>&>(r_future);
}

UserInfoInterface::~UserInfoInterface() = default;

std::string UserInfoInterface::uid() const { return uid_; }
std::string UserInfoInterface::email() const { return email_; }
std::string UserInfoInterface::display_name() const { return display_name_; }
std::string UserInfoInterface::photo_url() const { return photo_url_; }
std::string UserInfoInterface::provider_id() const { return provider_id_; }
std::string UserInfoInterface::phone_number() const { return phone_number_; }

Future<std::string> User::GetToken(const bool force_refresh) {
  return RunUserPromiseFn<std::string>(auth_data_, kUserFn_GetToken,
                                       ::firebase::auth::GetToken,
                                       force_refresh);
}

Future<IdTokenResult> User::GetIdTokenResult(const bool force_refresh) {
  return RunUserPromiseFn<IdTokenResult>(auth_data_, kUserFn_GetIdTokenResult,
                                         ::firebase::auth::GetIdTokenResult,
                                         force_refresh);
}

Future<void> User::Delete() {
  return RunUserPromiseFn<void>(auth_data_, kUserFn_Delete,
                                ::firebase::auth::Delete);
}

Future<void> User::SendEmailVerification() {
  return RunUserPromiseFn<void>(auth_data_, kUserFn_SendEmailVerification,
                                ::firebase::auth::SendEmailVerification);
}

Future<void> User::SendEmailVerificationBeforeUpdatingEmail(const char* email) {
  return RunUserPromiseFn<void>(
      auth_data_, kUserFn_SendEmailVerificationBeforeUpdatingEmail,
      ::firebase::auth::SendEmailVerificationBeforeUpdatingEmail, email);
}

Future<void> User::Reload() {
  return RunUserPromiseFn<void>(auth_data_, kUserFn_Reload,
                                ::firebase::auth::Reload);
}

Future<void> User::UpdatePassword(const char* const password) {
  return RunUserPromiseFn<void>(auth_data_, kUserFn_UpdatePassword,
                                ::firebase::auth::UpdatePassword, password);
}

Future<void> User::UpdateUserProfile(const User::UserProfile& profile) {
  return RunUserPromiseFn<void>(auth_data_, kUserFn_UpdateUserProfile,
                                ::firebase::auth::UpdateUserProfile, profile);
}

Future<AuthResult> User::Unlink(const char* const provider) {
  return RunUserPromiseFn<AuthResult>(auth_data_, kUserFn_Unlink,
                                      ::firebase::auth::Unlink, provider);
}

Future<AuthResult> User::LinkWithCredential(const Credential& credential) {
  return RunUserPromiseFn<AuthResult>(auth_data_, kUserFn_LinkWithCredential,
                                      ::firebase::auth::LinkWithCredential,
                                      credential);
}

Future<AuthResult> User::LinkWithProvider(
    FederatedAuthProvider* provider) const {
  return auth_data_ ? ::firebase::auth::LinkWithProvider(auth_data_, provider)
                    : Future<AuthResult>();
}

Future<void> User::Reauthenticate(const Credential& credential) {
  return RunUserPromiseFn<void>(auth_data_, kUserFn_Reauthenticate,
                                ::firebase::auth::Reauthenticate, credential);
}

Future<AuthResult> User::ReauthenticateAndRetrieveData(
    const Credential& credential) {
  return RunUserPromiseFn<AuthResult>(
      auth_data_, kUserFn_ReauthenticateAndRetrieveData,
      ::firebase::auth::ReauthenticateAndRetrieveData, credential);
}

Future<AuthResult> User::ReauthenticateWithProvider(
    FederatedAuthProvider* provider) const {
  return auth_data_
             ? ::firebase::auth::ReauthenticateWithProvider(auth_data_,
                                                            provider)
             : Future<AuthResult>();
}

Future<User> User::UpdatePhoneNumberCredential(
    const PhoneAuthCredential& credential) {
  return RunUserPromiseFn<User>(
      auth_data_, kUserFn_UpdatePhoneNumberCredential,
      ::firebase::auth::UpdatePhoneNumberCredential, credential);
}

Future<std::string> User::GetTokenInternal(const bool force_refresh,
                                           const int future_identifier) {
  return RunUserPromiseFn<std::string>(auth_data_, future_identifier,
                                       ::firebase::auth::GetTokenInternal,
                                       force_refresh, future_identifier);
}

}  // namespace auth
}  // namespace firebase
