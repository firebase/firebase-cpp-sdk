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

#include <fstream>
#include <memory>

#include "app/rest/transport_builder.h"
#include "app/rest/util.h"
#include "app/src/callback.h"
#include "app/src/include/firebase/future.h"
#include "app/src/scheduler.h"
#include "app/src/secure/user_secure_manager.h"
#include "auth/src/common.h"
#include "auth/src/data.h"
#include "auth/src/desktop/auth_data_handle.h"
#include "auth/src/desktop/auth_desktop.h"
#include "auth/src/desktop/auth_util.h"
#include "auth/src/desktop/get_account_info_result.h"
#include "auth/src/desktop/promise.h"
#include "auth/src/desktop/rpcs/delete_account_request.h"
#include "auth/src/desktop/rpcs/delete_account_response.h"
#include "auth/src/desktop/rpcs/get_oob_confirmation_code_request.h"
#include "auth/src/desktop/rpcs/get_oob_confirmation_code_response.h"
#include "auth/src/desktop/rpcs/secure_token_request.h"
#include "auth/src/desktop/rpcs/secure_token_response.h"
#include "auth/src/desktop/rpcs/set_account_info_request.h"
#include "auth/src/desktop/rpcs/set_account_info_response.h"
#include "auth/src/desktop/rpcs/verify_assertion_request.h"
#include "auth/src/desktop/rpcs/verify_assertion_response.h"
#include "auth/src/desktop/rpcs/verify_password_request.h"
#include "auth/src/desktop/rpcs/verify_password_response.h"
#include "auth/src/desktop/set_account_info_result.h"
#include "auth/src/desktop/sign_in_flow.h"
#include "auth/src/desktop/user_view.h"
#include "auth/src/desktop/validate_credential.h"
#include "auth/src/include/firebase/auth/types.h"
#include "auth/user_data_generated.h"
#include "auth/user_data_resource.h"
#include "flatbuffers/idl.h"

namespace firebase {
namespace auth {

using firebase::app::secure::UserSecureManager;
using firebase::callback::NewCallback;

namespace {

// Contains the results of a GetToken operation, either successful, in which
// case IsValid() will return true and token() will be non-blank, or
// non-successful.
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

// Signs out the current user if the error indicates the user is no longer
// valid.
void SignOutIfUserNoLongerValid(Auth* const auth, const AuthError error_code) {
  FIREBASE_ASSERT_RETURN_VOID(auth);

  if (error_code == kAuthErrorUserNotFound ||
      error_code == kAuthErrorUserTokenExpired ||
      error_code == kAuthErrorUserDisabled) {
    auth->SignOut();
  }
}

// Checks whether the given user has a non-expired ID token.
// If current token is still good for at least 5 minutes, we re-use it.
GetTokenResult GetTokenIfFresh(const UserView::Reader& user,
                               const bool force_refresh) {
  if (force_refresh) {
    return GetTokenResult(kAuthErrorFailure);
  }

  if (user.IsValid() && !user->id_token.empty() &&
      user->access_token_expiration_date > std::time(nullptr) + 5 * 60) {
    return GetTokenResult(user->id_token);
  }
  return GetTokenResult(kAuthErrorFailure);
}

// Makes sure that calling auth->current_user()->id_token() will result in
// a token that is good for at least 5 minutes. Will fetch a new token from the
// backend if necessary.
//
// If force_refresh is given, then a new token will be fetched without checking
// the current token at all.
//
// Note: this is a blocking call! The caller is supposed to call this function
// on the appropriate thread.
GetTokenResult EnsureFreshToken(AuthData* const auth_data,
                                const bool force_refresh,
                                const bool notify_listener) {
  FIREBASE_ASSERT_RETURN(GetTokenResult(kAuthErrorFailure), auth_data);

  GetTokenResult old_token(kAuthErrorFailure);
  std::string refresh_token;
  const bool is_user_logged_in =
      UserView::TryRead(auth_data, [&](const UserView::Reader& user) {
        old_token = GetTokenIfFresh(user, force_refresh);
        refresh_token = user->refresh_token;
      });

  if (!is_user_logged_in) {
    return GetTokenResult(kAuthErrorNoSignedInUser);
  }
  if (old_token.IsValid()) {
    return GetTokenResult(old_token.token());
  }

  const SecureTokenRequest request(GetApiKey(*auth_data),
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
    if (writer.IsValid()) {
      has_token_changed =
          UpdateUserTokensIfChanged(writer, TokenUpdate(response));
    } else {
      return GetTokenResult(kAuthErrorNoSignedInUser);
    }
  }
  if (has_token_changed && notify_listener) {
    NotifyIdTokenListeners(auth_data);
  }

  return GetTokenResult(response.id_token());
}

GetTokenResult EnsureFreshToken(AuthData* const auth_data,
                                const bool force_refresh) {
  return EnsureFreshToken(auth_data, force_refresh, true);
}

// Checks whether there is a currently logged in user. If no user is signed in,
// fails the given promise and returns false. Otherwise, doesn't touch the
// promise and returns true.
// Don't call while holding the lock on AuthData::future_impl.mutex()!
template <typename T>
bool ValidateCurrentUser(Promise<T>* const promise, AuthData* const auth_data) {
  const bool is_user_signed_in = UserView::GetReader(auth_data).IsValid();
  if (!is_user_signed_in) {
    promise->InvalidateLastResult();
    return false;
  }
  return true;
}

// Similar to CallAsync, but first ensures that current user has a fresh token
// and sets this token on the given request.
template <typename ResultT, typename RequestT>
Future<ResultT> CallAsyncWithFreshToken(
    AuthData* const auth_data, Promise<ResultT> promise,
    std::unique_ptr<RequestT> request,
    const typename AuthDataHandle<ResultT, RequestT>::CallbackT callback) {
  FIREBASE_ASSERT_RETURN(Future<ResultT>(), auth_data && request && callback);

  typedef AuthDataHandle<ResultT, RequestT> HandleT;

  auto scheduler_callback = NewCallback(
      [](HandleT* const raw_auth_data_handle) {
        std::unique_ptr<HandleT> handle(raw_auth_data_handle);

        const GetTokenResult get_token_result =
            EnsureFreshToken(handle->auth_data, false);
        if (!get_token_result.IsValid()) {
          FailPromise(&handle->promise, get_token_result.error());
          return;
        }
        handle->request->SetIdToken(get_token_result.token().c_str());

        handle->callback(handle.get());
      },
      new HandleT(auth_data, promise, std::move(request), callback));
  auto auth_impl = static_cast<AuthImpl*>(auth_data->auth_impl);
  auth_impl->scheduler_.Schedule(scheduler_callback);

  return promise.LastResult();
}

void CompleteSetAccountInfoPromise(Promise<void>* const promise,
                                   User* const user) {
  FIREBASE_ASSERT_RETURN_VOID(promise);
  promise->Complete();
}

void CompleteSetAccountInfoPromise(Promise<User*>* const promise,
                                   User* const user) {
  FIREBASE_ASSERT_RETURN_VOID(promise && user);
  promise->CompleteWithResult(user);
}

void CompleteSetAccountInfoPromise(Promise<SignInResult>* const promise,
                                   User* const user) {
  FIREBASE_ASSERT_RETURN_VOID(promise && user);

  SignInResult result;
  result.user = user;
  promise->CompleteWithResult(result);
}

void TriggerSaveUserFlow(AuthData* const auth_data) {
  auto auth_impl = static_cast<AuthImpl*>(auth_data->auth_impl);
  if (auth_impl != nullptr) {
    auth_impl->user_data_persist->SaveUserData(auth_data);
  }
}

template <typename ResultT>
void PerformSetAccountInfoFlow(
    AuthDataHandle<ResultT, SetAccountInfoRequest>* const handle) {
  const auto response = GetResponse<SetAccountInfoResponse>(*handle->request);
  const auto account_info = SetAccountInfoResult::FromResponse(response);

  if (account_info.IsValid()) {
    User* api_user_to_return =
        account_info.MergeToCurrentUser(handle->auth_data);

    TriggerSaveUserFlow(handle->auth_data);
    CompleteSetAccountInfoPromise(&handle->promise, api_user_to_return);
  } else {
    SignOutIfUserNoLongerValid(handle->auth_data->auth, account_info.error());
    FailPromise(&handle->promise, account_info.error());
  }
}

// Calls setAccountInfo endpoint to link the current user with the given email
// credential. The given void pointer must be a CredentialImpl containing
// a EmailAuthCredential.
// Non-blocking.
template <typename ResultT>
Future<ResultT> DoLinkWithEmailAndPassword(
    AuthData* const auth_data, Promise<ResultT> promise,
    const void* const raw_credential_impl) {
  FIREBASE_ASSERT_RETURN(Future<ResultT>(), auth_data && raw_credential_impl);

  const EmailAuthCredential* email_credential =
      GetEmailCredential(raw_credential_impl);

  typedef SetAccountInfoRequest RequestT;
  auto request = RequestT::CreateLinkWithEmailAndPasswordRequest(
      GetApiKey(*auth_data), email_credential->GetEmail().c_str(),
      email_credential->GetPassword().c_str());

  return CallAsyncWithFreshToken(auth_data, promise, std::move(request),
                                 PerformSetAccountInfoFlow<ResultT>);
}

// Checks that the given provider wasn't already linked to the currently
// signed-in user.
bool IsProviderAlreadyLinked(const std::string& provider,
                             const UserView::Reader& user) {
  const auto& linked_providers = user.GetUserInfos();
  const auto found =
      std::find_if(std::begin(linked_providers), std::end(linked_providers),
                   [&provider](const UserInfoInterface* const linked) {
                     if (!linked) {
                       LogError("Null provider data");
                     }
                     return linked && linked->provider_id() == provider;
                   });
  return found != std::end(linked_providers);
}

template <typename ResultT>
Future<ResultT> DoLinkCredential(Promise<ResultT> promise,
                                 AuthData* const auth_data,
                                 const std::string& provider,
                                 const void* const raw_credential) {
  FIREBASE_ASSERT_RETURN(Future<ResultT>(), auth_data && raw_credential);

  if (!ValidateCredential(&promise, provider, raw_credential)) {
    return promise.LastResult();
  }

  bool is_provider_already_linked = false;
  const bool is_user_logged_in =
      UserView::TryRead(auth_data, [&](const UserView::Reader& user) {
        is_provider_already_linked = IsProviderAlreadyLinked(provider, user);
      });

  if (!is_user_logged_in) {
    return promise.InvalidateLastResult();
  }
  if (is_provider_already_linked) {
    FailPromise(&promise, kAuthErrorProviderAlreadyLinked);
    return promise.LastResult();
  }

  if (provider == kEmailPasswordAuthProviderId) {
    return DoLinkWithEmailAndPassword(auth_data, promise, raw_credential);
  }

  // The difference with sign in is that verifyAssertion is called with an ID
  // token.
  // Current user may have become invalid - sign out in this case (this doesn't
  // apply to PerformSignIn, which is why it's not used here).
  return CallAsyncWithFreshToken(
      auth_data, promise,
      CreateVerifyAssertionRequest(*auth_data, raw_credential),
      [](AuthDataHandle<ResultT, VerifyAssertionRequest>* const handle) {
        FIREBASE_ASSERT_RETURN_VOID(handle && handle->request);

        const auto response =
            GetResponse<VerifyAssertionResponse>(*handle->request);
        const AuthenticationResult auth_result =
            CompleteSignInFlow(handle->auth_data, response);

        if (auth_result.IsValid()) {
          const SignInResult sign_in_result =
              auth_result.SetAsCurrentUser(handle->auth_data);
          CompletePromise(&handle->promise, sign_in_result);
        } else {
          SignOutIfUserNoLongerValid(handle->auth_data->auth,
                                     auth_result.error());
          FailPromise(&handle->promise, auth_result.error());
        }
      });
}

// Reauthenticates the current user and completes the promise contained within
// the given handle (either successfully or with an error, if the backend call
// failed).
template <typename ResponseT, typename FutureResultT, typename RequestT>
void PerformReauthFlow(AuthDataHandle<FutureResultT, RequestT>* const handle) {
  const auto response = GetResponse<ResponseT>(*handle->request);
  const AuthenticationResult auth_result =
      CompleteSignInFlow(handle->auth_data, response);
  if (!auth_result.IsValid()) {
    SignOutIfUserNoLongerValid(handle->auth_data->auth, auth_result.error());
    FailPromise(&handle->promise, auth_result.error());
    return;
  }

  std::string current_uid;
  const bool is_user_logged_in = UserView::TryRead(
      handle->auth_data,
      [&](const UserView::Reader& user) { current_uid = user->uid; });
  if (!is_user_logged_in) {
    FailPromise(&handle->promise, kAuthErrorNoSignedInUser);
    return;
  }

  if (auth_result.uid() == current_uid) {
    const SignInResult sign_in_result =
        auth_result.SetAsCurrentUser(handle->auth_data);

    TriggerSaveUserFlow(handle->auth_data);
    CompletePromise(&handle->promise, sign_in_result);
  } else {
    FailPromise(&handle->promise, kAuthErrorUserMismatch);
  }
}

template <typename ResultT>
Future<ResultT> DoReauthenticate(Promise<ResultT> promise,
                                 AuthData* const auth_data,
                                 const std::string& provider,
                                 const void* const raw_credential) {
  FIREBASE_ASSERT_RETURN(Future<ResultT>(), auth_data && raw_credential);

  if (!ValidateCurrentUser(&promise, auth_data)) {
    return promise.LastResult();
  }
  if (!ValidateCredential(&promise, provider, raw_credential)) {
    return promise.LastResult();
  }

  auto request =
      CreateRequestFromCredential(auth_data, provider, raw_credential);

  // Note: no need to get fresh tokens for reauthentication
  if (provider == kEmailPasswordAuthProviderId) {
    CallAsync(auth_data, promise, std::move(request),
              PerformReauthFlow<VerifyPasswordResponse>);
  } else {
    CallAsync(auth_data, promise, std::move(request),
              PerformReauthFlow<VerifyAssertionResponse>);
  }

  return promise.LastResult();
}

}  // namespace

UserDataPersist::UserDataPersist(const char* app_id) {
  user_secure_manager_ = MakeUnique<UserSecureManager>("auth", app_id);
}

UserDataPersist::UserDataPersist(
    UniquePtr<UserSecureManager> user_secure_manager)
    : user_secure_manager_(std::move(user_secure_manager)) {}

void UserDataPersist::OnAuthStateChanged(Auth* auth) {  // NOLINT
  if (auth->current_user() != nullptr) {
    SaveUserData(auth->auth_data_);
  } else {
    DeleteUserData(auth->auth_data_);
  }
}

void AssignLoadedData(const Future<std::string>& future, AuthData* auth_data) {
  // This function will change the persistent_cache_load_pending, which is
  // a critical flag to decide listener trigger event, so lock listener_mutex
  // to protect it.
  if (future.error() == firebase::app::secure::kNoEntry) {
    LogDebug(future.error_message());
    return;
  }

  std::string loaded_string = *(future.result());
  if (loaded_string.length() == 0) {
    return;
  }

  // Decode to flatbuffer
  std::string decoded;
  if (!UserSecureManager::AsciiToBinary(loaded_string, &decoded)) {
    LogWarning("Auth: Error decoding persistent user data.");
    return;
  }

  // Verify the Flatbuffer is valid.
  flatbuffers::Verifier verifier(
      reinterpret_cast<const uint8_t*>(decoded.c_str()), decoded.length());
  if (!VerifyUserDataDesktopBuffer(verifier)) {
    LogWarning("Auth: Error verifying persistent user data.");
    return;
  }

  auto userData = GetUserDataDesktop(decoded.c_str());
  if (userData == nullptr) {
    LogWarning("Auth: Error reading persistent user data.");
    return;
  }

  UserData loaded_user;
  loaded_user.uid = userData->uid()->c_str();
  loaded_user.email = userData->email()->c_str();
  loaded_user.display_name = userData->display_name()->c_str();
  loaded_user.photo_url = userData->photo_url()->c_str();
  loaded_user.provider_id = userData->provider_id()->c_str();
  loaded_user.phone_number = userData->phone_number()->c_str();
  loaded_user.is_anonymous = userData->is_anonymous();
  loaded_user.is_email_verified = userData->is_email_verified();
  loaded_user.id_token = userData->id_token()->c_str();
  loaded_user.refresh_token = userData->refresh_token()->c_str();
  loaded_user.access_token = userData->access_token()->c_str();
  loaded_user.access_token_expiration_date =
      userData->access_token_expiration_date();
  loaded_user.has_email_password_credential =
      userData->has_email_password_credential();
  loaded_user.last_sign_in_timestamp = userData->last_sign_in_timestamp();
  loaded_user.creation_timestamp = userData->creation_timestamp();

  std::vector<UserInfoImpl> loaded_provider_data;
  const auto& provider_data = userData->provider_data();
  if (provider_data) {
    for (size_t i = 0; i < provider_data->size(); ++i) {
      auto providerData = provider_data->Get(i);

      UserInfoImpl loaded_user_info;
      loaded_user_info.uid = providerData->uid()->c_str();
      loaded_user_info.email = providerData->email()->c_str();
      loaded_user_info.display_name = providerData->display_name()->c_str();
      loaded_user_info.photo_url = providerData->photo_url()->c_str();
      loaded_user_info.provider_id = providerData->provider_id()->c_str();
      loaded_user_info.phone_number = providerData->phone_number()->c_str();

      loaded_provider_data.push_back(loaded_user_info);
    }
  }

  auto writer = UserView::ResetUser(auth_data, loaded_user);
  writer.ResetUserInfos(loaded_provider_data);
}

void HandleLoadedData(const Future<std::string>& future, void* auth_data) {
  auto cast_auth_data = static_cast<AuthData*>(auth_data);
  MutexLock destructing_lock(cast_auth_data->desctruting_mutex);
  if (cast_auth_data->destructing) {
    // If auth is destructing, abort.
    return;
  }
  AssignLoadedData(future, cast_auth_data);
  auto scheduler_callback = NewCallback(
      [](AuthData* callback_auth_data) {
        // Don't trigger token listeners if token get refreshed, since
        // it will get triggered inside LoadFinishTriggerListeners anyways.
        const GetTokenResult get_token_result =
            EnsureFreshToken(callback_auth_data, false, false);
        LoadFinishTriggerListeners(callback_auth_data);
      },
      cast_auth_data);

  auto auth_impl = static_cast<AuthImpl*>(cast_auth_data->auth_impl);
  auth_impl->scheduler_.Schedule(scheduler_callback);
}

Future<std::string> UserDataPersist::LoadUserData(AuthData* auth_data) {
  if (auth_data == nullptr) {
    return Future<std::string>();
  }

  Future<std::string> future =
      user_secure_manager_->LoadUserData(auth_data->app->name());
  future.OnCompletion(HandleLoadedData, auth_data);
  return future;
}

Future<void> UserDataPersist::SaveUserData(AuthData* auth_data) {
  if (auth_data == nullptr) {
    return Future<void>();
  }

  const auto user = UserView::GetReader(auth_data);
  if (!user.IsValid()) {
    return Future<void>();
  }

  // Build up a serialized buffer algorithmically:
  flatbuffers::FlatBufferBuilder builder;

  const auto& user_infos = user.GetUserInfos();

  auto create_callback = [&builder, &user_infos](size_t index){
    const auto& user_info = user_infos[index];

    auto uid = builder.CreateString(user_info->uid());
    auto email = builder.CreateString(user_info->email());
    auto display_name = builder.CreateString(user_info->display_name());
    auto photo_url = builder.CreateString(user_info->photo_url());
    auto provider_id = builder.CreateString(user_info->provider_id());
    auto phone_number = builder.CreateString(user_info->phone_number());

    return CreateUserProviderData(
      builder, uid, email, display_name, photo_url, provider_id, phone_number);
  };

  auto provider_data_list =
      builder.CreateVector<flatbuffers::Offset<UserProviderData>>(
        user_infos.size(), create_callback);

  // Compile data using schema
  auto uid = builder.CreateString(user->uid);
  auto email = builder.CreateString(user->email);
  auto display_name = builder.CreateString(user->display_name);
  auto photo_url = builder.CreateString(user->photo_url);
  auto provider_id = builder.CreateString(user->provider_id);
  auto phone_number = builder.CreateString(user->phone_number);

  auto id_token = builder.CreateString(user->id_token);
  auto refresh_token = builder.CreateString(user->refresh_token);
  auto access_token = builder.CreateString(user->access_token);

  auto desktop = CreateUserDataDesktop(
      builder, uid, email, display_name, photo_url, provider_id, phone_number,
      user->is_anonymous, user->is_email_verified, id_token, refresh_token,
      access_token, user->access_token_expiration_date,
      user->has_email_password_credential, user->last_sign_in_timestamp,
      user->creation_timestamp, provider_data_list);
  builder.Finish(desktop);

  std::string save_string;
  auto bufferpointer =
      reinterpret_cast<const char*>(builder.GetBufferPointer());
  save_string.assign(bufferpointer, bufferpointer + builder.GetSize());
  // Encode flatbuffer
  std::string encoded;
  UserSecureManager::BinaryToAscii(save_string, &encoded);

  return user_secure_manager_->SaveUserData(auth_data->app->name(), encoded);
}

Future<void> UserDataPersist::DeleteUserData(AuthData* auth_data) {
  return user_secure_manager_->DeleteUserData(auth_data->app->name());
}

User::~User() {
  // Make sure we don't have any pending futures in flight before we disappear.
  while (!auth_data_->future_impl.IsSafeToDelete()) {
    internal::Sleep(100);
  }
}

// RPCs

Future<std::string> User::GetToken(const bool force_refresh) {
  return GetTokenInternal(force_refresh, kUserFn_GetToken);
}

Future<std::string> User::GetTokenInternal(const bool force_refresh,
                                           const int future_identifier) {
  Promise<std::string> promise(&auth_data_->future_impl, future_identifier);

  GetTokenResult current_token(kAuthErrorFailure);
  const bool is_user_logged_in =
      UserView::TryRead(auth_data_, [&](const UserView::Reader& user) {
        current_token = GetTokenIfFresh(user, force_refresh);
      });

  if (!is_user_logged_in) {
    auto future = promise.future();
    future.Release();
    return future;
  }
  if (current_token.IsValid()) {
    promise.CompleteWithResult(current_token.token());
    return promise.future();
  }

  const auto callback =
      [](AuthDataHandle<std::string, rest::Request>* const handle) {
        const GetTokenResult get_token_result =
            EnsureFreshToken(handle->auth_data, true);
        if (!get_token_result.IsValid()) {
          FailPromise(&handle->promise, get_token_result.error());
          return;
        }

        handle->promise.CompleteWithResult(get_token_result.token());
      };

  // Note: request is deliberately null because EnsureFreshToken will create it.
  return CallAsync(auth_data_, promise, std::unique_ptr<rest::Request>(),
                   callback);
}

Future<void> User::Delete() {
  Promise<void> promise(&auth_data_->future_impl, kUserFn_Delete);
  if (!ValidateCurrentUser(&promise, auth_data_)) {
    return promise.LastResult();
  }

  typedef DeleteAccountRequest RequestT;
  // Note: make_unique can't be used because it's not supported by Visual Studio
  // 2012.
  auto request = std::unique_ptr<RequestT>(  // NOLINT
      new RequestT(GetApiKey(*auth_data_)));

  const auto callback = [](AuthDataHandle<void, RequestT>* const handle) {
    const auto response = GetResponse<DeleteAccountResponse>(*handle->request);
    if (response.IsSuccessful()) {
      handle->auth_data->auth->SignOut();
      handle->promise.Complete();
    } else {
      FailPromise(&handle->promise, response.error_code());
    }
  };

  return CallAsyncWithFreshToken(auth_data_, promise, std::move(request),
                                 callback);
}

Future<void> User::SendEmailVerification() {
  Promise<void> promise(&auth_data_->future_impl,
                        kUserFn_SendEmailVerification);
  if (!ValidateCurrentUser(&promise, auth_data_)) {
    return promise.LastResult();
  }

  typedef GetOobConfirmationCodeRequest RequestT;
  auto request =
      RequestT::CreateSendEmailVerificationRequest(GetApiKey(*auth_data_));

  const auto callback = [](AuthDataHandle<void, RequestT>* const handle) {
    const auto response =
        GetResponse<GetOobConfirmationCodeResponse>(*handle->request);
    if (response.IsSuccessful()) {
      handle->promise.Complete();
    } else {
      FailPromise(&handle->promise, response.error_code());
    }
  };

  return CallAsyncWithFreshToken(auth_data_, promise, std::move(request),
                                 callback);
}

Future<void> User::Reload() {
  Promise<void> promise(&auth_data_->future_impl, kUserFn_Reload);
  std::string id_token;
  const bool is_user_logged_in = UserView::TryRead(
      auth_data_,
      [&](const UserView::Reader& user) { id_token = user->id_token; });

  if (!is_user_logged_in) {
    return promise.InvalidateLastResult();
  }

  typedef GetAccountInfoRequest RequestT;
  auto request =
      std::unique_ptr<RequestT>(new RequestT(GetApiKey(*auth_data_),
                                             id_token.c_str()));  // NOLINT

  const auto callback = [](AuthDataHandle<void, RequestT>* const handle) {
    const GetAccountInfoResult account_info = GetAccountInfo(*handle->request);
    // No listeners will be notified: UID couldn't have changed, because we
    // are reloading the same user. Token couldn't have changed, because
    // GetAccountInfoResponse doesn't contain any tokens.
    if (account_info.IsValid()) {
      account_info.MergeToCurrentUser(handle->auth_data);
      handle->promise.Complete();
    } else {
      SignOutIfUserNoLongerValid(handle->auth_data->auth, account_info.error());
      FailPromise(&handle->promise, account_info.error());
    }
  };

  return CallAsyncWithFreshToken(auth_data_, promise, std::move(request),
                                 callback);
}

Future<void> User::UpdateEmail(const char* const email) {
  Promise<void> promise(&auth_data_->future_impl, kUserFn_UpdateEmail);
  if (!ValidateEmail(&promise, email)) {
    return promise.LastResult();
  }
  if (!ValidateCurrentUser(&promise, auth_data_)) {
    return promise.LastResult();
  }

  typedef SetAccountInfoRequest RequestT;
  auto request =
      RequestT::CreateUpdateEmailRequest(GetApiKey(*auth_data_), email);
  return CallAsyncWithFreshToken(auth_data_, promise, std::move(request),
                                 PerformSetAccountInfoFlow<void>);
}

Future<void> User::UpdatePassword(const char* const password) {
  Promise<void> promise(&auth_data_->future_impl, kUserFn_UpdatePassword);
  if (!ValidatePassword(&promise, password)) {
    return promise.LastResult();
  }
  if (!ValidateCurrentUser(&promise, auth_data_)) {
    return promise.LastResult();
  }

  typedef SetAccountInfoRequest RequestT;
  auto request =
      RequestT::CreateUpdatePasswordRequest(GetApiKey(*auth_data_), password);

  return CallAsyncWithFreshToken(auth_data_, promise, std::move(request),
                                 PerformSetAccountInfoFlow<void>);
}

Future<void> User::UpdateUserProfile(const UserProfile& profile) {
  Promise<void> promise(&auth_data_->future_impl, kUserFn_UpdateUserProfile);
  if (!ValidateCurrentUser(&promise, auth_data_)) {
    return promise.LastResult();
  }

  typedef SetAccountInfoRequest RequestT;
  auto request = RequestT::CreateUpdateProfileRequest(
      GetApiKey(*auth_data_), profile.display_name, profile.photo_url);

  return CallAsyncWithFreshToken(auth_data_, promise, std::move(request),
                                 PerformSetAccountInfoFlow<void>);
}

Future<User*> User::Unlink(const char* const provider) {
  Promise<User*> promise(&auth_data_->future_impl, kUserFn_Unlink);
  if (!provider || strlen(provider) == 0) {
    FailPromise(&promise, kAuthErrorNoSuchProvider);
    return promise.LastResult();
  }

  bool is_provider_linked = false;
  const bool is_user_logged_in =
      UserView::TryRead(auth_data_, [&](const UserView::Reader& user) {
        is_provider_linked = IsProviderAlreadyLinked(provider, user);
      });

  if (!is_user_logged_in) {
    return promise.InvalidateLastResult();
  }
  if (!is_provider_linked) {
    FailPromise(&promise, kAuthErrorNoSuchProvider);
    return promise.LastResult();
  }

  typedef SetAccountInfoRequest RequestT;
  auto request =
      RequestT::CreateUnlinkProviderRequest(GetApiKey(*auth_data_), provider);

  return CallAsyncWithFreshToken(auth_data_, promise, std::move(request),
                                 PerformSetAccountInfoFlow<User*>);
}

Future<User*> User::LinkWithCredential(const Credential& credential) {
  Promise<User*> promise(&auth_data_->future_impl, kUserFn_LinkWithCredential);
  return DoLinkCredential(promise, auth_data_, credential.provider(),
                          credential.impl_);
}

Future<SignInResult> User::LinkAndRetrieveDataWithCredential(
    const Credential& credential) {
  Promise<SignInResult> promise(&auth_data_->future_impl,
                                kUserFn_LinkAndRetrieveDataWithCredential);
  return DoLinkCredential(promise, auth_data_, credential.provider(),
                          credential.impl_);
}

Future<SignInResult> User::LinkWithProvider(
    FederatedAuthProvider* provider) const {
  FIREBASE_ASSERT_RETURN(Future<SignInResult>(), provider);
  // TODO(b/139363200)
  // return provider->Link(auth_data_);
  SafeFutureHandle<SignInResult> handle =
      auth_data_->future_impl.SafeAlloc<SignInResult>(kUserFn_LinkWithProvider);
  auth_data_->future_impl.CompleteWithResult(
      handle, kAuthErrorUnimplemented,
      "Operation is not supported on non-mobile systems.",
      /*result=*/{});
  return MakeFuture(&auth_data_->future_impl, handle);
}

Future<void> User::Reauthenticate(const Credential& credential) {
  Promise<void> promise(&auth_data_->future_impl, kUserFn_Reauthenticate);
  return DoReauthenticate(promise, auth_data_, credential.provider(),
                          credential.impl_);
}

Future<SignInResult> User::ReauthenticateAndRetrieveData(
    const Credential& credential) {
  Promise<SignInResult> promise(&auth_data_->future_impl,
                                kUserFn_ReauthenticateAndRetrieveData);
  return DoReauthenticate(promise, auth_data_, credential.provider(),
                          credential.impl_);
}

Future<SignInResult> User::ReauthenticateWithProvider(
    FederatedAuthProvider* provider) const {
  FIREBASE_ASSERT_RETURN(Future<SignInResult>(), provider);
  // TODO(b/139363200)
  // return provider->Reauthenticate(auth_data_);
  SafeFutureHandle<SignInResult> handle =
      auth_data_->future_impl.SafeAlloc<SignInResult>(
          kUserFn_ReauthenticateWithProvider);
  auth_data_->future_impl.CompleteWithResult(
      handle, kAuthErrorUnimplemented,
      "Operation is not supported on non-mobile systems.",
      /*result=*/{});
  return MakeFuture(&auth_data_->future_impl, handle);
}

const std::vector<UserInfoInterface*>& User::provider_data() const {
  return auth_data_->user_infos;
}

UserMetadata User::metadata() const {
  if (!ValidUser(auth_data_)) return UserMetadata();

  UserData* user_data = static_cast<UserData*>(auth_data_->user_impl);
  UserMetadata metadata;

  metadata.last_sign_in_timestamp = user_data->last_sign_in_timestamp;
  metadata.creation_timestamp = user_data->creation_timestamp;
  return metadata;
}

bool User::is_email_verified() const {
  const auto user = UserView::GetReader(auth_data_);
  return user.IsValid() ? user->is_email_verified : false;
}

bool User::is_anonymous() const {
  const auto user = UserView::GetReader(auth_data_);
  return user.IsValid() ? user->is_anonymous : true;
}

std::string User::uid() const {
  const auto user = UserView::GetReader(auth_data_);
  return user.IsValid() ? user->uid : std::string();
}

std::string User::email() const {
  const auto user = UserView::GetReader(auth_data_);
  return user.IsValid() ? user->email : std::string();
}

std::string User::display_name() const {
  const auto user = UserView::GetReader(auth_data_);
  return user.IsValid() ? user->display_name : std::string();
}

std::string User::phone_number() const {
  const auto user = UserView::GetReader(auth_data_);
  return user.IsValid() ? user->phone_number : std::string();
}

std::string User::photo_url() const {
  const auto user = UserView::GetReader(auth_data_);
  return user.IsValid() ? user->photo_url : std::string();
}

std::string User::provider_id() const {
  const auto user = UserView::GetReader(auth_data_);
  return user.IsValid() ? user->provider_id : std::string();
}

// Not implemented

Future<User*> User::UpdatePhoneNumberCredential(const Credential& credential) {
  Promise<User*> promise(&auth_data_->future_impl,
                         kUserFn_UpdatePhoneNumberCredential);
  if (!ValidateCurrentUser(&promise, auth_data_)) {
    return promise.LastResult();
  }

  // TODO(varconst): for now, phone auth is not supported on desktop.
  promise.Fail(kAuthErrorApiNotAvailable,
               "Phone Auth is not supported on desktop");
  return promise.LastResult();
}

}  // namespace auth
}  // namespace firebase
