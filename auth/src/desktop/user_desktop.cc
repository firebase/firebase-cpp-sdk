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

#include "auth/src/desktop/user_desktop.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <memory>
#include <type_traits>
#include <utility>

#include "app/rest/transport_builder.h"
#include "app/rest/util.h"
#include "app/src/callback.h"
#include "app/src/include/firebase/future.h"
#include "app/src/scheduler.h"
#include "app/src/secure/user_secure_manager.h"
#include "auth/src/common.h"
#include "auth/src/common/promise.h"
#include "auth/src/data.h"
#include "auth/src/desktop/auth_data_handle.h"
#include "auth/src/desktop/auth_desktop.h"
#include "auth/src/desktop/auth_util.h"
#include "auth/src/desktop/get_account_info_result.h"
#include "auth/src/desktop/rpcs/delete_account_request.h"
#include "auth/src/desktop/rpcs/delete_account_response.h"
#include "auth/src/desktop/rpcs/get_oob_confirmation_code_request.h"
#include "auth/src/desktop/rpcs/get_oob_confirmation_code_response.h"
#include "auth/src/desktop/rpcs/secure_token_request.h"
#include "auth/src/desktop/rpcs/secure_token_response.h"
#include "auth/src/desktop/rpcs/set_account_info_request.h"
#include "auth/src/desktop/rpcs/set_account_info_response.h"
#include "auth/src/desktop/rpcs/sign_up_request.h"
#include "auth/src/desktop/rpcs/verify_assertion_request.h"
#include "auth/src/desktop/rpcs/verify_assertion_response.h"
#include "auth/src/desktop/rpcs/verify_password_request.h"
#include "auth/src/desktop/rpcs/verify_password_response.h"
#include "auth/src/desktop/set_account_info_result.h"
#include "auth/src/desktop/sign_in_flow.h"
#include "auth/src/desktop/user_view.h"
#include "auth/src/desktop/validate_credential.h"
#include "auth/src/id_token_util.h"
#include "auth/src/include/firebase/auth/types.h"
#include "auth/user_data_generated.h"
#include "auth/user_data_resource.h"
#include "flatbuffers/idl.h"

namespace firebase {
namespace auth {

using firebase::app::secure::UserSecureManager;
using firebase::callback::NewCallback;

namespace {

const char kIdTokenResultParseErrorMessage[] =
    "Failed to decode the ID token as a JWT.";

class GetTokenResult {
 public:
  explicit GetTokenResult(const AuthError error) : error_(error) {}
  explicit GetTokenResult(const std::string& token)
      : error_(kAuthErrorNone), token_(token) {}

  bool IsValid() const { return error_ == kAuthErrorNone; }
  AuthError error() const { return error_; }
  std::string token() const { return token_; }

 private:
  AuthError error_;
  std::string token_;
};

void SignOutIfUserNoLongerValid(Auth* const auth, const AuthError error_code) {
  FIREBASE_ASSERT_RETURN_VOID(auth);
  if (error_code == kAuthErrorUserNotFound ||
      error_code == kAuthErrorUserTokenExpired ||
      error_code == kAuthErrorUserDisabled) {
    auth->SignOut();
  }
}

GetTokenResult GetTokenIfFresh(const UserView::Reader& user,
                               const bool force_refresh) {
  if (!force_refresh && user.IsValid() && !user->id_token.empty() &&
      user->access_token_expiration_date > std::time(nullptr) + 5 * 60) {
    return GetTokenResult(user->id_token);
  }
  return GetTokenResult(kAuthErrorFailure);
}

GetTokenResult EnsureFreshToken(AuthData* const auth_data,
                                const bool force_refresh,
                                const bool notify_listener = true) {
  FIREBASE_ASSERT_RETURN(GetTokenResult(kAuthErrorFailure), auth_data);

  GetTokenResult old_token(kAuthErrorFailure);
  std::string uid, refresh_token;
  if (!UserView::TryRead(auth_data, [&](const UserView::Reader& user) {
        old_token = GetTokenIfFresh(user, force_refresh);
        uid = user->uid;
        refresh_token = user->refresh_token;
      })) {
    return GetTokenResult(kAuthErrorNoSignedInUser);
  }
  if (old_token.IsValid()) return old_token;

  SecureTokenRequest request(*auth_data->app, GetApiKey(*auth_data),
                             refresh_token.c_str());
  auto response = GetResponse<SecureTokenResponse>(request);
  if (!response.IsSuccessful()) {
    SignOutIfUserNoLongerValid(auth_data->auth, response.error_code());
    return GetTokenResult(response.error_code());
  }

  bool has_token_changed = false;
  const auto token_update = TokenUpdate(response);
  if (token_update.HasUpdate()) {
    UserView::Writer writer = UserView::GetWriter(auth_data);
    if (writer.IsValid() && writer->uid == uid &&
        writer->refresh_token == refresh_token) {
      has_token_changed = UpdateUserTokensIfChanged(writer, token_update);
    } else {
      return GetTokenResult(kAuthErrorNoSignedInUser);
    }
  }
  if (has_token_changed) {
    TriggerSaveUserFlow(auth_data);
    if (notify_listener) NotifyIdTokenListeners(auth_data);
  }
  return GetTokenResult(response.id_token());
}

template <typename T>
bool ValidateCurrentUser(Promise<T>* const promise, AuthData* const auth_data) {
  if (!promise) return false;
  if (!auth_data || !UserView::GetReader(auth_data).IsValid()) {
    promise->InvalidateLastResult();
    return false;
  }
  return true;
}

template <typename ResultT, typename RequestT>
void CallAsyncWithFreshToken(
    AuthData* const auth_data, Promise<ResultT> promise,
    std::unique_ptr<RequestT> request,
    const typename AuthDataHandle<ResultT, RequestT>::CallbackT callback) {
  FIREBASE_ASSERT_RETURN_VOID(auth_data && request && callback);
  using HandleT = AuthDataHandle<ResultT, RequestT>;
  auto scheduler_callback = NewCallback(
      [](HandleT* const raw_auth_data_handle) {
        std::unique_ptr<HandleT> handle(raw_auth_data_handle);
        const GetTokenResult token_res =
            EnsureFreshToken(handle->auth_data, false);
        if (!token_res.IsValid()) {
          FailPromise(&handle->promise, token_res.error());
          return;
        }
        handle->request->SetIdToken(token_res.token().c_str());
        if constexpr (std::is_base_of_v<AuthRequest, RequestT>) {
          handle->request->PrepareRequestHeaders();
        }
        handle->callback(handle.get());
      },
      new HandleT(auth_data, promise, std::move(request), callback));
  static_cast<AuthImpl*>(auth_data->auth_impl)
      ->scheduler_.Schedule(scheduler_callback);
}

void CompleteSetAccountInfoPromise(Promise<void>* const promise,
                                   User* const user) {
  FIREBASE_ASSERT_RETURN_VOID(promise);
  if (!user) {
    FailPromise(promise, kAuthErrorNoSignedInUser);
    return;
  }
  promise->Complete();
}

void CompleteSetAccountInfoPromise(Promise<AuthResult>* const promise,
                                   User* const user) {
  FIREBASE_ASSERT_RETURN_VOID(promise);
  if (!user) {
    FailPromise(promise, kAuthErrorNoSignedInUser);
    return;
  }
  AuthResult result;
  result.user = *user;
  promise->CompleteWithResult(result);
}

template <typename ResponseT, typename ResultT, typename RequestT>
void PerformSignUpOrLinkAssertionFlow(
    AuthDataHandle<ResultT, RequestT>* const promise) {
  FIREBASE_ASSERT_RETURN_VOID(promise && promise->request);
  const auto response = GetResponse<ResponseT>(*promise->request);
  const AuthenticationResult auth_response =
      CompleteSignInFlow(promise->auth_data, response);
  if (auth_response.IsValid()) {
    CompletePromise(&promise->promise,
                    auth_response.SetAsCurrentUser(promise->auth_data));
  } else {
    SignOutIfUserNoLongerValid(promise->auth_data->auth, auth_response.error());
    FailPromise(&promise->promise, auth_response.error());
  }
}

template <typename ResultT>
void PerformSetAccountInfoFlow(
    AuthDataHandle<ResultT, SetAccountInfoRequest>* const promise) {
  const auto response = GetResponse<SetAccountInfoResponse>(*promise->request);
  const auto account_info = SetAccountInfoResult::FromResponse(response);
  if (account_info.IsValid()) {
    User* api_user_to_return =
        account_info.MergeToCurrentUser(promise->auth_data);
    if (api_user_to_return) TriggerSaveUserFlow(promise->auth_data);
    CompleteSetAccountInfoPromise(&promise->promise, api_user_to_return);
  } else {
    SignOutIfUserNoLongerValid(promise->auth_data->auth, account_info.error());
    FailPromise(&promise->promise, account_info.error());
  }
}

bool IsProviderAlreadyLinked(const std::string& provider,
                             const UserView::Reader& user) {
  if (provider == kEmailPasswordAuthProviderId &&
      user->has_email_password_credential) {
    return true;
  }
  const auto& linked_providers = user.GetUserInfos();
  return std::find_if(linked_providers.begin(), linked_providers.end(),
                      [&provider](const UserInfoInterface* const linked) {
                        if (!linked) LogError("Null provider data");
                        return linked && linked->provider_id() == provider;
                      }) != linked_providers.end();
}

template <typename ResultT>
void DoLinkCredential(Promise<ResultT> promise, AuthData* const auth_data,
                      const std::string& provider,
                      const void* const raw_credential) {
  FIREBASE_ASSERT_RETURN_VOID(auth_data);
  if (!ValidateCredential(&promise, provider, raw_credential)) return;

  bool is_provider_already_linked = false;
  if (!UserView::TryRead(auth_data, [&](const UserView::Reader& user) {
        is_provider_already_linked = IsProviderAlreadyLinked(provider, user);
      })) {
    promise.InvalidateLastResult();
    return;
  }
  if (is_provider_already_linked) {
    FailPromise(&promise, kAuthErrorProviderAlreadyLinked);
    return;
  }
  if (provider == kEmailPasswordAuthProviderId) {
    const EmailAuthCredential* email_credential =
        GetEmailCredential(raw_credential);
    CallAsyncWithFreshToken(
        auth_data, promise,
        SignUpRequest::CreateLinkWithEmailAndPasswordRequest(
            *auth_data->app, GetApiKey(*auth_data),
            email_credential->GetEmail().c_str(),
            email_credential->GetPassword().c_str()),
        PerformSignUpOrLinkAssertionFlow<SignUpNewUserResponse, ResultT,
                                         SignUpRequest>);
    return;
  }

  auto request = CreateVerifyAssertionRequest(*auth_data, raw_credential);
  if (!request) {
    FailPromise(&promise, kAuthErrorInvalidCredential);
    return;
  }
  CallAsyncWithFreshToken(
      auth_data, promise, std::move(request),
      PerformSignUpOrLinkAssertionFlow<VerifyAssertionResponse, ResultT,
                                       VerifyAssertionRequest>);
}

template <typename ResponseT, typename FutureResultT, typename RequestT>
void PerformReauthFlow(AuthDataHandle<FutureResultT, RequestT>* const promise) {
  const auto response = GetResponse<ResponseT>(*promise->request);
  const AuthenticationResult authentication_result =
      CompleteSignInFlow(promise->auth_data, response);
  if (!authentication_result.IsValid()) {
    SignOutIfUserNoLongerValid(promise->auth_data->auth,
                               authentication_result.error());
    FailPromise(&promise->promise, authentication_result.error());
    return;
  }

  std::string current_uid;
  if (!UserView::TryRead(promise->auth_data, [&](const UserView::Reader& user) {
        current_uid = user->uid;
      })) {
    FailPromise(&promise->promise, kAuthErrorNoSignedInUser);
    return;
  }

  if (authentication_result.uid() == current_uid) {
    CompletePromise(&promise->promise,
                    authentication_result.SetAsCurrentUser(promise->auth_data));
  } else {
    FailPromise(&promise->promise, kAuthErrorUserMismatch);
  }
}

template <typename ResultT>
void DoReauthenticate(Promise<ResultT> promise, AuthData* const auth_data,
                      const std::string& provider,
                      const void* const raw_credential) {
  FIREBASE_ASSERT_RETURN_VOID(auth_data);
  if (!ValidateCurrentUser(&promise, auth_data) ||
      !ValidateCredential(&promise, provider, raw_credential)) {
    return;
  }
  auto request =
      CreateRequestFromCredential(auth_data, provider, raw_credential);
  if (!request) {
    FailPromise(&promise, kAuthErrorInvalidCredential);
    return;
  }
  CallAsync(auth_data, promise, std::move(request),
            provider == kEmailPasswordAuthProviderId
                ? PerformReauthFlow<VerifyPasswordResponse, ResultT,
                                    AuthRequest>
                : PerformReauthFlow<VerifyAssertionResponse, ResultT,
                                    AuthRequest>);
}

std::string GetLanguageCode(AuthData* auth_data) {
  auto auth_impl = static_cast<AuthImpl*>(auth_data->auth_impl);
  MutexLock lock(auth_impl->provider_mutex);
  return auth_impl->language_code;
}

void PerformOobRequestCallback(
    AuthDataHandle<void, GetOobConfirmationCodeRequest>* const p) {
  auto response = GetResponse<GetOobConfirmationCodeResponse>(*p->request);
  if (response.IsSuccessful()) {
    p->promise.Complete();
  } else {
    SignOutIfUserNoLongerValid(p->auth_data->auth, response.error_code());
    FailPromise(&p->promise, response.error_code());
  }
}

}  // namespace

void TriggerSaveUserFlow(AuthData* const auth_data) {
  if (!auth_data) return;
  auto auth_impl = static_cast<AuthImpl*>(auth_data->auth_impl);
  if (auth_impl && auth_impl->user_data_persist) {
    auth_impl->user_data_persist->SaveUserData(auth_data);
  }
}

UserDataPersist::UserDataPersist(const char* app_id)
    : user_secure_manager_(
          std::make_unique<UserSecureManager>("auth", app_id)) {}

UserDataPersist::UserDataPersist(
    std::unique_ptr<UserSecureManager> user_secure_manager)
    : user_secure_manager_(std::move(user_secure_manager)) {}

void UserDataPersist::OnAuthStateChanged(Auth* auth) {  // NOLINT
  if (auth->current_user().is_valid()) {
    SaveUserData(auth->auth_data_);
  } else {
    DeleteUserData(auth->auth_data_);
  }
}

bool AssignLoadedData(const Future<std::string>& future, AuthData* auth_data) {
  if (future.error() == firebase::app::secure::kNoEntry) {
    LogDebug(future.error_message());
    return false;
  }
  if (future.error() != 0 || !future.result()) {
    if (future.error_message()) {
      LogWarning("Auth: Error loading persistent user data: %s",
                 future.error_message());
    }
    return false;
  }

  const std::string& loaded_string = *future.result();
  if (loaded_string.empty()) return false;

  std::string decoded;
  if (!UserSecureManager::AsciiToBinary(loaded_string, &decoded)) {
    LogWarning("Auth: Error decoding persistent user data.");
    return false;
  }

  flatbuffers::Verifier verifier(
      reinterpret_cast<const uint8_t*>(decoded.c_str()), decoded.length());
  if (!VerifyUserDataDesktopBuffer(verifier)) {
    LogWarning("Auth: Error verifying persistent user data.");
    return false;
  }

  auto userData = GetUserDataDesktop(decoded.c_str());
  if (!userData) {
    LogWarning("Auth: Error reading persistent user data.");
    return false;
  }

  auto safe_fb_str = [](const flatbuffers::String* str) -> const char* {
    return str ? str->c_str() : "";
  };

  UserData loaded_user;
  loaded_user.uid = safe_fb_str(userData->uid());
  loaded_user.email = safe_fb_str(userData->email());
  loaded_user.display_name = safe_fb_str(userData->display_name());
  loaded_user.photo_url = safe_fb_str(userData->photo_url());
  loaded_user.provider_id = safe_fb_str(userData->provider_id());
  loaded_user.phone_number = safe_fb_str(userData->phone_number());
  loaded_user.is_anonymous = userData->is_anonymous();
  loaded_user.is_email_verified = userData->is_email_verified();
  loaded_user.id_token = safe_fb_str(userData->id_token());
  loaded_user.refresh_token = safe_fb_str(userData->refresh_token());
  loaded_user.access_token = safe_fb_str(userData->access_token());
  loaded_user.access_token_expiration_date =
      userData->access_token_expiration_date();
  loaded_user.has_email_password_credential =
      userData->has_email_password_credential();
  loaded_user.last_sign_in_timestamp = userData->last_sign_in_timestamp();
  loaded_user.creation_timestamp = userData->creation_timestamp();

  std::vector<UserInfoImpl> loaded_provider_data;
  if (const auto* provider_data = userData->provider_data()) {
    for (size_t i = 0; i < provider_data->size(); ++i) {
      const auto* pd = provider_data->Get(i);
      if (!pd) continue;
      loaded_provider_data.push_back(
          {safe_fb_str(pd->uid()), safe_fb_str(pd->email()),
           safe_fb_str(pd->display_name()), safe_fb_str(pd->photo_url()),
           safe_fb_str(pd->provider_id()), safe_fb_str(pd->phone_number())});
    }
  }

  MutexLock listeners_lock(auth_data->listeners_mutex);
  if (!auth_data->persistent_cache_load_pending ||
      UserView::GetReader(auth_data).IsValid()) {
    return false;
  }

  auto writer = UserView::ResetUser(auth_data, loaded_user);
  writer.ResetUserInfos(loaded_provider_data);
  return true;
}

void HandleLoadedData(const Future<std::string>& future, void* auth_data) {
  auto cast_auth_data = static_cast<AuthData*>(auth_data);
  MutexLock destructing_lock(cast_auth_data->destructing_mutex);
  if (cast_auth_data->destructing) return;

  const bool loaded = AssignLoadedData(future, cast_auth_data);
  auto scheduler_callback =
      loaded ? NewCallback(
                   [](AuthData* callback_auth_data) {
                     (void)EnsureFreshToken(callback_auth_data, false, false);
                     LoadFinishTriggerListeners(callback_auth_data);
                   },
                   cast_auth_data)
             : NewCallback(
                   [](AuthData* callback_auth_data) {
                     LoadFinishTriggerListeners(callback_auth_data);
                   },
                   cast_auth_data);
  static_cast<AuthImpl*>(cast_auth_data->auth_impl)
      ->scheduler_.Schedule(scheduler_callback);
}

Future<std::string> UserDataPersist::LoadUserData(AuthData* auth_data) {
  if (!auth_data || !auth_data->app) return Future<std::string>();
  Future<std::string> future =
      user_secure_manager_->LoadUserData(auth_data->app->name());
  future.OnCompletion(HandleLoadedData, auth_data);
  return future;
}

Future<void> UserDataPersist::SaveUserData(AuthData* auth_data) {
  if (!auth_data || !auth_data->app) return Future<void>();
  const auto user = UserView::GetReader(auth_data);
  if (!user.IsValid()) return Future<void>();

  flatbuffers::FlatBufferBuilder builder;
  std::vector<const UserInfoInterface*> valid_user_infos;
  for (const auto* info : user.GetUserInfos()) {
    if (info) valid_user_infos.push_back(info);
  }

  auto provider_data_list =
      builder.CreateVector<flatbuffers::Offset<UserProviderData>>(
          valid_user_infos.size(), [&](size_t index) {
            const auto* ui = valid_user_infos[index];
            return CreateUserProviderData(
                builder, builder.CreateString(ui->uid()),
                builder.CreateString(ui->email()),
                builder.CreateString(ui->display_name()),
                builder.CreateString(ui->photo_url()),
                builder.CreateString(ui->provider_id()),
                builder.CreateString(ui->phone_number()));
          });

  auto desktop = CreateUserDataDesktop(
      builder, builder.CreateString(user->uid),
      builder.CreateString(user->email),
      builder.CreateString(user->display_name),
      builder.CreateString(user->photo_url),
      builder.CreateString(user->provider_id),
      builder.CreateString(user->phone_number), user->is_anonymous,
      user->is_email_verified, builder.CreateString(user->id_token),
      builder.CreateString(user->refresh_token),
      builder.CreateString(user->access_token),
      user->access_token_expiration_date, user->has_email_password_credential,
      user->last_sign_in_timestamp, user->creation_timestamp,
      provider_data_list);
  builder.Finish(desktop);

  auto* buf = reinterpret_cast<const char*>(builder.GetBufferPointer());
  std::string save_string(buf, buf + builder.GetSize());
  std::string encoded;
  UserSecureManager::BinaryToAscii(save_string, &encoded);
  return user_secure_manager_->SaveUserData(auth_data->app->name(), encoded);
}

Future<void> UserDataPersist::DeleteUserData(AuthData* auth_data) {
  if (!auth_data || !auth_data->app) return Future<void>();
  return user_secure_manager_->DeleteUserData(auth_data->app->name());
}

bool User::is_valid() const {
  return ValidUser(auth_data_) && UserView::GetReader(auth_data_).IsValid();
}

template <typename R, typename MapFn, bool ForceRefresh>
void EnsureFreshTokenCallback(AuthDataHandle<R, rest::Request>* const p) {
  auto res = EnsureFreshToken(p->auth_data, ForceRefresh);
  if (!res.IsValid()) {
    FailPromise(&p->promise, res.error());
    return;
  }
  MapFn()(p->promise, res.token());
}

template <typename R, typename MapFn>
void GetTokenHelper(AuthData* auth_data, Promise<R> promise, bool force_refresh,
                    MapFn map_fn) {
  if (!auth_data || !ValidateCurrentUser(&promise, auth_data)) return;
  GetTokenResult current_token(kAuthErrorFailure);
  if (!UserView::TryRead(auth_data, [&](const UserView::Reader& user) {
        current_token = GetTokenIfFresh(user, force_refresh);
      })) {
    FailPromise(&promise, kAuthErrorNoSignedInUser);
    return;
  }
  if (current_token.IsValid()) {
    map_fn(promise, current_token.token());
    return;
  }
  CallAsync(auth_data, promise, std::unique_ptr<rest::Request>(),
            force_refresh ? EnsureFreshTokenCallback<R, MapFn, true>
                          : EnsureFreshTokenCallback<R, MapFn, false>);
}

struct MapToken {
  void operator()(Promise<std::string>& p, const std::string& t) const {
    p.CompleteWithResult(t);
  }
};

struct MapIdTokenResult {
  void operator()(Promise<IdTokenResult>& p, const std::string& t) const {
    IdTokenResult r;
    if (!ParseIdToken(t, &r)) {
      LogWarning("Failed to parse ID token.");
      p.Fail(kAuthErrorFailure, kIdTokenResultParseErrorMessage);
      return;
    }
    p.CompleteWithResult(r);
  }
};

void GetTokenInternal(AuthData* auth_data, Promise<std::string> promise,
                      const bool force_refresh, const int /*future_id*/) {
  GetTokenHelper(auth_data, promise, force_refresh, MapToken());
}

void GetToken(AuthData* auth_data, Promise<std::string> promise,
              const bool force_refresh) {
  GetTokenInternal(auth_data, promise, force_refresh, kUserFn_GetToken);
}

void GetIdTokenResult(AuthData* auth_data, Promise<IdTokenResult> promise,
                      const bool force_refresh) {
  GetTokenHelper(auth_data, promise, force_refresh, MapIdTokenResult());
}

void Delete(AuthData* auth_data, Promise<void> promise) {
  if (!ValidateCurrentUser(&promise, auth_data)) return;
  CallAsyncWithFreshToken(
      auth_data, promise,
      std::make_unique<DeleteAccountRequest>(*auth_data->app,
                                             GetApiKey(*auth_data)),
      +[](AuthDataHandle<void, DeleteAccountRequest>* const p) {
        auto response = GetResponse<DeleteAccountResponse>(*p->request);
        if (response.IsSuccessful()) {
          p->auth_data->auth->SignOut();
          p->promise.Complete();
        } else {
          SignOutIfUserNoLongerValid(p->auth_data->auth, response.error_code());
          FailPromise(&p->promise, response.error_code());
        }
      });
}

void SendEmailVerification(AuthData* auth_data, Promise<void> promise) {
  if (!ValidateCurrentUser(&promise, auth_data)) return;
  std::string lang = GetLanguageCode(auth_data);
  CallAsyncWithFreshToken(
      auth_data, promise,
      GetOobConfirmationCodeRequest::CreateSendEmailVerificationRequest(
          *auth_data->app, GetApiKey(*auth_data),
          lang.empty() ? nullptr : lang.c_str()),
      PerformOobRequestCallback);
}

void SendEmailVerificationBeforeUpdatingEmail(AuthData* auth_data,
                                              Promise<void> promise,
                                              const char* email) {
  if (!ValidateCurrentUser(&promise, auth_data) ||
      !ValidateEmail(&promise, email)) {
    return;
  }
  std::string lang = GetLanguageCode(auth_data);
  CallAsyncWithFreshToken(
      auth_data, promise,
      GetOobConfirmationCodeRequest::
          CreateSendEmailVerificationBeforeUpdatingEmailRequest(
              *auth_data->app, GetApiKey(*auth_data), email,
              lang.empty() ? nullptr : lang.c_str()),
      PerformOobRequestCallback);
}

void Reload(AuthData* auth_data, Promise<void> promise) {
  if (!ValidateCurrentUser(&promise, auth_data)) return;
  std::string id_token;
  if (!UserView::TryRead(auth_data, [&](const UserView::Reader& user) {
        id_token = user->id_token;
      })) {
    promise.InvalidateLastResult();
    return;
  }
  CallAsyncWithFreshToken(
      auth_data, promise,
      std::make_unique<GetAccountInfoRequest>(
          *auth_data->app, GetApiKey(*auth_data), id_token.c_str()),
      +[](AuthDataHandle<void, GetAccountInfoRequest>* const p) {
        GetAccountInfoResult account_info = GetAccountInfo(*p->request);
        if (account_info.IsValid()) {
          account_info.MergeToCurrentUser(p->auth_data);
          TriggerSaveUserFlow(p->auth_data);
          p->promise.Complete();
        } else {
          SignOutIfUserNoLongerValid(p->auth_data->auth, account_info.error());
          FailPromise(&p->promise, account_info.error());
        }
      });
}

void UpdatePassword(AuthData* auth_data, Promise<void> promise,
                    const char* const password) {
  if (!ValidateCurrentUser(&promise, auth_data)) return;
  CallAsyncWithFreshToken(
      auth_data, promise,
      SetAccountInfoRequest::CreateUpdatePasswordRequest(
          *auth_data->app, GetApiKey(*auth_data), password),
      PerformSetAccountInfoFlow<void>);
}

void UpdateUserProfile(AuthData* auth_data, Promise<void> promise,
                       const User::UserProfile& profile) {
  if (!ValidateCurrentUser(&promise, auth_data)) return;
  CallAsyncWithFreshToken(
      auth_data, promise,
      SetAccountInfoRequest::CreateUpdateProfileRequest(
          *auth_data->app, GetApiKey(*auth_data), profile.display_name,
          profile.photo_url),
      PerformSetAccountInfoFlow<void>);
}

void Unlink(AuthData* auth_data, Promise<AuthResult> promise,
            const char* const provider) {
  if (!auth_data || !ValidateCurrentUser(&promise, auth_data)) return;
  if (!provider || strlen(provider) == 0) {
    FailPromise(&promise, kAuthErrorNoSuchProvider);
    return;
  }
  bool is_provider_linked = false;
  if (!UserView::TryRead(auth_data, [&](const UserView::Reader& user) {
        is_provider_linked = IsProviderAlreadyLinked(provider, user);
      })) {
    promise.InvalidateLastResult();
    return;
  }
  if (!is_provider_linked) {
    FailPromise(&promise, kAuthErrorNoSuchProvider);
    return;
  }
  CallAsyncWithFreshToken(
      auth_data, promise,
      SetAccountInfoRequest::CreateUnlinkProviderRequest(
          *auth_data->app, GetApiKey(*auth_data), provider),
      PerformSetAccountInfoFlow<AuthResult>);
}

void LinkWithCredential(AuthData* auth_data, Promise<AuthResult> promise,
                        const Credential& credential) {
  if (!auth_data || !ValidateCurrentUser(&promise, auth_data)) return;
  DoLinkCredential(promise, auth_data, credential.provider(),
                   internal::CredentialAccess::GetImpl(credential));
}

Future<AuthResult> LinkWithProvider(AuthData* auth_data,
                                    FederatedAuthProvider* provider) {
  if (!auth_data) return Future<AuthResult>();
  FIREBASE_ASSERT_RETURN(Future<AuthResult>(), provider);
  return provider->Link(auth_data);
}

void Reauthenticate(AuthData* auth_data, Promise<void> promise,
                    const Credential& credential) {
  if (!auth_data || !ValidateCurrentUser(&promise, auth_data)) return;
  DoReauthenticate(promise, auth_data, credential.provider(),
                   internal::CredentialAccess::GetImpl(credential));
}

void ReauthenticateAndRetrieveData(AuthData* auth_data,
                                   Promise<AuthResult> promise,
                                   const Credential& credential) {
  if (!auth_data || !ValidateCurrentUser(&promise, auth_data)) return;
  DoReauthenticate(promise, auth_data, credential.provider(),
                   internal::CredentialAccess::GetImpl(credential));
}

Future<AuthResult> ReauthenticateWithProvider(AuthData* auth_data,
                                              FederatedAuthProvider* provider) {
  if (!auth_data) return Future<AuthResult>();
  FIREBASE_ASSERT_RETURN(Future<AuthResult>(), provider);
  return provider->Reauthenticate(auth_data);
}

std::vector<UserInfoInterface> User::provider_data() const {
  std::vector<UserInfoInterface> provider_data;
  if (!auth_data_) return provider_data;
  const auto user = UserView::GetReader(auth_data_);
  if (user.IsValid()) {
    const auto& internal_provider_data = user.GetUserInfos();
    provider_data.reserve(internal_provider_data.size());
    for (const auto* info : internal_provider_data) {
      if (!info) continue;
      UserInfoInterface user_info;
      user_info.uid_ = info->uid();
      user_info.email_ = info->email();
      user_info.display_name_ = info->display_name();
      user_info.photo_url_ = info->photo_url();
      user_info.provider_id_ = info->provider_id();
      user_info.phone_number_ = info->phone_number();
      provider_data.push_back(user_info);
    }
  }
  return provider_data;
}

UserMetadata User::metadata() const {
  UserMetadata metadata;
  if (!auth_data_) return metadata;
  const auto user_view = UserView::GetReader(auth_data_);
  if (user_view.IsValid()) {
    metadata.last_sign_in_timestamp = user_view->last_sign_in_timestamp;
    metadata.creation_timestamp = user_view->creation_timestamp;
  }
  return metadata;
}

namespace {
template <typename T, typename C>
T GetUserField(AuthData* auth_data, T C::*field, T default_val = T()) {
  if (!auth_data) return default_val;
  const auto user_view = UserView::GetReader(auth_data);
  return user_view.IsValid() ? (*user_view).*field : default_val;
}
}  // namespace

bool User::is_email_verified() const {
  return GetUserField(auth_data_, &UserData::is_email_verified, false);
}
bool User::is_anonymous() const {
  return GetUserField(auth_data_, &UserData::is_anonymous, false);
}
std::string User::uid() const {
  return GetUserField(auth_data_, &UserData::uid);
}
std::string User::email() const {
  return GetUserField(auth_data_, &UserData::email);
}
std::string User::display_name() const {
  return GetUserField(auth_data_, &UserData::display_name);
}
std::string User::phone_number() const {
  return GetUserField(auth_data_, &UserData::phone_number);
}
std::string User::photo_url() const {
  return GetUserField(auth_data_, &UserData::photo_url);
}
std::string User::provider_id() const {
  return GetUserField(auth_data_, &UserData::provider_id);
}

void UpdatePhoneNumberCredential(AuthData* auth_data, Promise<User> promise,
                                 const PhoneAuthCredential& /*credential*/) {
  if (!auth_data || !ValidateCurrentUser(&promise, auth_data)) return;
  promise.Fail(kAuthErrorApiNotAvailable,
               "Phone Auth is not supported on desktop");
}

}  // namespace auth
}  // namespace firebase
