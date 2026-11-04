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

Future<std::string> User::GetToken(const bool force_refresh) {
  if (!auth_data_) return Future<std::string>();
  Promise<std::string> promise(&auth_data_->future_impl, kUserFn_GetToken);
  ::firebase::auth::GetToken(auth_data_, promise, force_refresh);
  return promise.future();
}

Future<IdTokenResult> User::GetIdTokenResult(const bool force_refresh) {
  if (!auth_data_) return Future<IdTokenResult>();
  Promise<IdTokenResult> promise(&auth_data_->future_impl, kUserFn_GetIdTokenResult);
  ::firebase::auth::GetIdTokenResult(auth_data_, promise, force_refresh);
  return promise.future();
}

Future<void> User::Delete() {
  if (!auth_data_) return Future<void>();
  Promise<void> promise(&auth_data_->future_impl, kUserFn_Delete);
  ::firebase::auth::Delete(auth_data_, promise);
  return promise.future();
}

Future<void> User::SendEmailVerification() {
  if (!auth_data_) return Future<void>();
  Promise<void> promise(&auth_data_->future_impl, kUserFn_SendEmailVerification);
  ::firebase::auth::SendEmailVerification(auth_data_, promise);
  return promise.future();
}

Future<void> User::SendEmailVerificationBeforeUpdatingEmail(const char* email) {
  if (!auth_data_) return Future<void>();
  Promise<void> promise(&auth_data_->future_impl, kUserFn_SendEmailVerificationBeforeUpdatingEmail);
  ::firebase::auth::SendEmailVerificationBeforeUpdatingEmail(auth_data_, promise, email);
  return promise.future();
}

Future<void> User::Reload() {
  if (!auth_data_) return Future<void>();
  Promise<void> promise(&auth_data_->future_impl, kUserFn_Reload);
  ::firebase::auth::Reload(auth_data_, promise);
  return promise.future();
}

Future<void> User::UpdatePassword(const char* const password) {
  if (!auth_data_) return Future<void>();
  Promise<void> promise(&auth_data_->future_impl, kUserFn_UpdatePassword);
  ::firebase::auth::UpdatePassword(auth_data_, promise, password);
  return promise.future();
}

Future<void> User::UpdateUserProfile(const User::UserProfile& profile) {
  if (!auth_data_) return Future<void>();
  Promise<void> promise(&auth_data_->future_impl, kUserFn_UpdateUserProfile);
  ::firebase::auth::UpdateUserProfile(auth_data_, promise, profile);
  return promise.future();
}

Future<AuthResult> User::Unlink(const char* const provider) {
  if (!auth_data_) return Future<AuthResult>();
  Promise<AuthResult> promise(&auth_data_->future_impl, kUserFn_Unlink);
  ::firebase::auth::Unlink(auth_data_, promise, provider);
  return promise.future();
}

Future<AuthResult> User::LinkWithCredential(const Credential& credential) {
  if (!auth_data_) return Future<AuthResult>();
  Promise<AuthResult> promise(&auth_data_->future_impl, kUserFn_LinkWithCredential);
  ::firebase::auth::LinkWithCredential(auth_data_, promise, credential);
  return promise.future();
}

Future<AuthResult> User::LinkWithProvider(FederatedAuthProvider* provider) const {
  if (!auth_data_) return Future<AuthResult>();
  return ::firebase::auth::LinkWithProvider(auth_data_, provider);
}

Future<void> User::Reauthenticate(const Credential& credential) {
  if (!auth_data_) return Future<void>();
  Promise<void> promise(&auth_data_->future_impl, kUserFn_Reauthenticate);
  ::firebase::auth::Reauthenticate(auth_data_, promise, credential);
  return promise.future();
}

Future<AuthResult> User::ReauthenticateAndRetrieveData(const Credential& credential) {
  if (!auth_data_) return Future<AuthResult>();
  Promise<AuthResult> promise(&auth_data_->future_impl, kUserFn_ReauthenticateAndRetrieveData);
  ::firebase::auth::ReauthenticateAndRetrieveData(auth_data_, promise, credential);
  return promise.future();
}

Future<AuthResult> User::ReauthenticateWithProvider(FederatedAuthProvider* provider) const {
  if (!auth_data_) return Future<AuthResult>();
  return ::firebase::auth::ReauthenticateWithProvider(auth_data_, provider);
}

Future<User> User::UpdatePhoneNumberCredential(const PhoneAuthCredential& credential) {
  if (!auth_data_) return Future<User>();
  Promise<User> promise(&auth_data_->future_impl, kUserFn_UpdatePhoneNumberCredential);
  ::firebase::auth::UpdatePhoneNumberCredential(auth_data_, promise, credential);
  return promise.future();
}



Future<std::string> User::GetTokenInternal(const bool force_refresh, const int future_identifier) {
  if (!auth_data_) return Future<std::string>();
  Promise<std::string> promise(&auth_data_->future_impl, future_identifier);
  ::firebase::auth::GetTokenInternal(auth_data_, promise, force_refresh, future_identifier);
  return promise.future();
}

}  // namespace auth
}  // namespace firebase
