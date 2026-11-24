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

#include "auth/src/desktop/auth_desktop.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "app/rest/transport_curl.h"
#include "app/src/app_common.h"
#include "app/src/app_identifier.h"
#include "app/src/app_state.h"
#include "app/src/assert.h"
#include "app/src/heartbeat/heartbeat_controller_desktop.h"
#include "app/src/include/firebase/app.h"
#include "auth/src/common.h"
#include "auth/src/data.h"
#include "auth/src/desktop/auth_data_handle.h"
#include "auth/src/desktop/auth_util.h"
#include "auth/src/desktop/authentication_result.h"
#include "auth/src/desktop/credential_impl.h"
#include "auth/src/desktop/promise.h"
#include "auth/src/desktop/rpcs/create_auth_uri_request.h"
#include "auth/src/desktop/rpcs/create_auth_uri_response.h"
#include "auth/src/desktop/rpcs/get_account_info_request.h"
#include "auth/src/desktop/rpcs/get_account_info_response.h"
#include "auth/src/desktop/rpcs/get_oob_confirmation_code_request.h"
#include "auth/src/desktop/rpcs/get_oob_confirmation_code_response.h"
#include "auth/src/desktop/rpcs/sign_up_new_user_request.h"
#include "auth/src/desktop/rpcs/sign_up_new_user_response.h"
#include "auth/src/desktop/rpcs/verify_assertion_request.h"
#include "auth/src/desktop/rpcs/verify_assertion_response.h"
#include "auth/src/desktop/rpcs/verify_custom_token_request.h"
#include "auth/src/desktop/rpcs/verify_custom_token_response.h"
#include "auth/src/desktop/rpcs/verify_password_request.h"
#include "auth/src/desktop/rpcs/verify_password_response.h"
#include "auth/src/desktop/sign_in_flow.h"
#include "auth/src/desktop/user_desktop.h"
#include "auth/src/desktop/user_view.h"
#include "auth/src/desktop/validate_credential.h"

namespace firebase {
namespace auth {

namespace {

template <typename ResultT>
Future<ResultT> DoSignInWithCredential(Promise<ResultT> promise,
                                       AuthData* const auth_data,
                                       const std::string& provider,
                                       const void* const raw_credential) {
  FIREBASE_ASSERT_RETURN(Future<ResultT>(), auth_data && raw_credential);

  if (!ValidateCredential(&promise, provider, raw_credential)) {
    return promise.LastResult();
  }

  auto request =
      CreateRequestFromCredential(auth_data, provider, raw_credential);

  if (provider == kEmailPasswordAuthProviderId) {
    CallAsync(auth_data, promise, std::move(request),
              PerformSignInFlow<VerifyPasswordResponse>);
  } else {
    CallAsync(auth_data, promise, std::move(request),
              PerformSignInFlow<VerifyAssertionResponse>);
  }

  return promise.LastResult();
}

void InitializeInternalAuthStateListener(AuthData* auth_data) {
  auto auth_impl = static_cast<AuthImpl*>(auth_data->auth_impl);
  auth_impl->internal_listeners =
      std::make_unique<InternalAuthStateListener>();
  auth_data->AddInternalAuthStateListener(auth_impl->internal_listeners.get());
}

void DestroyInternalAuthStateListener(AuthData* auth_data) {
  auto auth_impl = static_cast<AuthImpl*>(auth_data->auth_impl);
  auth_data->RemoveInternalAuthStateListener(
      auth_impl->internal_listeners.get());
  auth_impl->internal_listeners.reset();
}

}  // namespace

void* CreatePlatformAuth(const App& app) {
  FIREBASE_ASSERT_RETURN(nullptr, app.is_valid());

  AuthImpl* const auth = new AuthImpl();
  auth->api_key = app.options().api_key();
  auth->app_name = app.name();

  // Check environment variables for emulator if set.
  if (std::getenv("USE_AUTH_EMULATOR") != nullptr) {
    auth->assigned_emulator_url.append(kEmulatorLocalHost);
    auth->assigned_emulator_url.append(":");
    if (std::getenv("AUTH_EMULATOR_PORT") == nullptr) {
      auth->assigned_emulator_url.append(kEmulatorPort);
    } else {
      auth->assigned_emulator_url.append(std::getenv("AUTH_EMULATOR_PORT"));
    }
    LogInfo("Using Auth Emulator: %s", auth->assigned_emulator_url.c_str());
  }

  return auth;
}

void LogHeartbeat(const Auth& auth) {
  if (App app = auth.app(); app.is_valid()) {
    app.LogHeartbeat();
  }
}

IdTokenRefreshListener::IdTokenRefreshListener() : token_timestamp_(0) {}

IdTokenRefreshListener::~IdTokenRefreshListener() {}

void IdTokenRefreshListener::OnIdTokenChanged(Auth auth) {
  if (!auth.auth_data_) return;
  AuthData* auth_data = auth.auth_data_.get();
  // Note: Make sure to always make future_impl.mutex the innermost lock,
  // to prevent deadlocks!
  MutexLock lock(mutex_);
  MutexLock future_lock(auth_data->future_impl.mutex());
  if (auth_data->user_impl != nullptr) {
    ResetTokenRefreshCounter(auth_data);

    // Retrieve id_token from auth_data.
    {
      UserView::Reader reader = UserView::GetReader(auth_data);
      assert(reader.IsValid());
      current_token_ = reader->id_token;
    }
    token_timestamp_ = internal::GetTimestampEpoch();
  } else {
    current_token_ = "";
  }
}

std::string IdTokenRefreshListener::GetCurrentToken() {
  MutexLock lock(mutex_);
  return current_token_;
}

uint64_t IdTokenRefreshListener::GetTokenTimestamp() {
  MutexLock lock(mutex_);
  return token_timestamp_;
}

void InternalAuthStateListener::AddListener(Callback callback, void* context) {
  if (!callback) return;
  MutexLock lock(mutex_);
  callbacks_.emplace_back(callback, context);
}

void InternalAuthStateListener::RemoveListener(Callback callback,
                                               void* context) {
  if (!callback) return;
  MutexLock lock(mutex_);
  Entry entry = {callback, context};
  auto iter = std::find(callbacks_.begin(), callbacks_.end(), entry);
  if (iter != callbacks_.end()) {
    callbacks_.erase(iter);
  }
}

void InternalAuthStateListener::OnAuthStateChanged(Auth /*auth*/) {
  std::vector<Entry> callbacks_copy;
  {
    MutexLock lock(mutex_);
    callbacks_copy = callbacks_;
  }
  for (const Entry& entry : callbacks_copy) {
    entry.first(entry.second);
  }
}

// ::firebase::internal::AuthTokenProvider overrides on AuthData (Desktop):

std::string AuthData::GetCurrentToken() {
  // Make sure the persistent cache is loaded.
  if (Auth handle = MakeAuthHandle(); handle.is_valid()) {
    (void)handle.current_user();
  }
  MutexLock lock(token_listener_mutex);
  auto* impl = static_cast<AuthImpl*>(auth_impl);
  return impl ? impl->token_refresh_thread.CurrentAuthToken() : std::string();
}

Future<std::string> AuthData::GetTokenAsync(bool force_refresh) {
  Auth handle = MakeAuthHandle();
  if (!handle.is_valid()) return Future<std::string>();
  User user = handle.current_user();
  if (!user.is_valid()) return Future<std::string>();
  return GetUserTokenInternal(this, force_refresh,
                              kInternalFn_GetTokenForFunctionRegistry);
}

std::string AuthData::GetCurrentUserUid() {
  Auth handle = MakeAuthHandle();
  if (!handle.is_valid()) return std::string();
  User user = handle.current_user();
  return user.is_valid() ? user.uid() : std::string();
}

void AuthData::StartTokenListener() { EnableTokenAutoRefresh(this); }

void AuthData::StopTokenListener() { DisableTokenAutoRefresh(this); }

void AuthData::AddAuthStateListener(AuthStateCallback callback, void* context) {
  auto* impl = static_cast<AuthImpl*>(auth_impl);
  if (impl && impl->internal_listeners) {
    impl->internal_listeners->AddListener(callback, context);
  }
}

void AuthData::RemoveAuthStateListener(AuthStateCallback callback,
                                       void* context) {
  auto* impl = static_cast<AuthImpl*>(auth_impl);
  if (impl && impl->internal_listeners) {
    impl->internal_listeners->RemoveListener(callback, context);
  }
}

void InitPlatformAuth(AuthData* const auth_data) {
  firebase::rest::InitTransportCurl();

  // Add internal listeners first so they run before user-supplied listeners.
  InitializeInternalAuthStateListener(auth_data);

  // Load existing UserData.
  InitializeUserDataPersist(auth_data);

  InitializeTokenRefresher(auth_data);
}

void DestroyPlatformAuth(AuthData* const auth_data) {
  FIREBASE_ASSERT_RETURN_VOID(auth_data);
  auto auth_impl = static_cast<AuthImpl*>(auth_data->auth_impl);
  if (!auth_impl) return;

  auth_impl->scheduler_.CancelAllAndShutdownWorkerThread();

  DestroyInternalAuthStateListener(auth_data);

  DestroyTokenRefresher(auth_data);

  DestroyUserDataPersist(auth_data);

  UserView::ClearUser(auth_data);

  delete auth_impl;
  auth_data->auth_impl = nullptr;
  firebase::rest::CleanupTransportCurl();
}

// RPCs
Future<AuthResult> Auth::SignInWithCustomToken(const char* custom_token) {
  if (!auth_data_) return Future<AuthResult>();
  Promise<AuthResult> promise(&auth_data_->future_impl,
                              kAuthFn_SignInWithCustomToken);
  if (!custom_token || strlen(custom_token) == 0) {
    FailPromise(&promise, kAuthErrorInvalidCustomToken);
    return promise.LastResult();
  }

  App app_handle = app();
  typedef VerifyCustomTokenRequest RequestT;
  auto request = std::unique_ptr<RequestT>(  // NOLINT
      new RequestT(app_handle, GetApiKey(*auth_data_), custom_token));

  return CallAsync(auth_data_.get(), promise, std::move(request),
                   PerformSignInFlow<VerifyCustomTokenResponse>);
}

Future<User> Auth::SignInWithCredential(const Credential& credential) {
  if (!auth_data_) return Future<User>();
  Promise<User> promise(&auth_data_->future_impl, kAuthFn_SignInWithCredential);
  if (!ValidateCredential(&promise, credential.provider(), credential.impl_)) {
    return promise.LastResult();
  }

  return DoSignInWithCredential(promise, auth_data_.get(),
                                credential.provider(), credential.impl_);
}

Future<AuthResult> Auth::SignInWithProvider(FederatedAuthProvider* provider) {
  if (!auth_data_) return Future<AuthResult>();
  FIREBASE_ASSERT_RETURN(Future<AuthResult>(), provider);

  SafeFutureHandle<AuthResult> handle =
      auth_data_->future_impl.SafeAlloc<AuthResult>(kAuthFn_SignInWithProvider);
  auth_data_->future_impl.CompleteWithResult(
      handle, kAuthErrorUnimplemented,
      "Operation is not supported on non-mobile systems.",
      /*result=*/{});
  return MakeFuture(&auth_data_->future_impl, handle);
}

Future<AuthResult> Auth::SignInAnonymously() {
  if (!auth_data_) return Future<AuthResult>();
  Promise<AuthResult> promise(&auth_data_->future_impl,
                              kAuthFn_SignInAnonymously);

  // If user is already signed in anonymously, return immediately.
  bool is_anonymous = false;
  UserView::TryRead(auth_data_.get(), [&](const UserView::Reader& reader) {
    is_anonymous = reader->is_anonymous;
  });

  if (is_anonymous) {
    AuthResult auth_result;
    auth_result.user = auth_data_->MakeCurrentUserHandle();
    promise.CompleteWithResult(auth_result);
    return promise.LastResult();
  }

  App app_handle = app();
  typedef SignUpNewUserRequest RequestT;
  auto request = std::unique_ptr<RequestT>(  // NOLINT
      new RequestT(app_handle, GetApiKey(*auth_data_)));

  return CallAsync(auth_data_.get(), promise, std::move(request),
                   PerformSignInFlow<SignUpNewUserResponse>);
}

Future<AuthResult> Auth::SignInWithEmailAndPassword(
    const char* const email, const char* const password) {
  if (!auth_data_) return Future<AuthResult>();
  Promise<AuthResult> promise(&auth_data_->future_impl,
                              kAuthFn_SignInWithEmailAndPassword);
  if (!ValidateEmailAndPassword(&promise, email, password)) {
    return promise.LastResult();
  }

  App app_handle = app();
  typedef VerifyPasswordRequest RequestT;
  auto request = std::unique_ptr<RequestT>(  // NOLINT
      new RequestT(app_handle, GetApiKey(*auth_data_), email, password));

  return CallAsync(auth_data_.get(), promise, std::move(request),
                   PerformSignInFlow<VerifyPasswordResponse>);
}

Future<AuthResult> Auth::CreateUserWithEmailAndPassword(
    const char* const email, const char* const password) {
  if (!auth_data_) return Future<AuthResult>();
  Promise<AuthResult> promise(&auth_data_->future_impl,
                              kAuthFn_CreateUserWithEmailAndPassword);
  if (!ValidateEmailAndPassword(&promise, email, password)) {
    return promise.LastResult();
  }

  App app_handle = app();
  typedef SignUpNewUserRequest RequestT;
  auto request = std::unique_ptr<RequestT>(  // NOLINT
      new RequestT(app_handle, GetApiKey(*auth_data_), email, password, ""));

  return CallAsync(auth_data_.get(), promise, std::move(request),
                   PerformSignInFlow<SignUpNewUserResponse>);
}

Future<AuthResult> Auth::SignInAndRetrieveDataWithCredential(
    const Credential& credential) {
  if (!auth_data_) return Future<AuthResult>();
  Promise<AuthResult> promise(&auth_data_->future_impl,
                              kAuthFn_SignInAndRetrieveDataWithCredential);
  return DoSignInWithCredential(promise, auth_data_.get(),
                                credential.provider(), credential.impl_);
}

Future<Auth::FetchProvidersResult> Auth::FetchProvidersForEmail(
    const char* email) {
  if (!auth_data_) return Future<FetchProvidersResult>();
  Promise<FetchProvidersResult> promise(&auth_data_->future_impl,
                                        kAuthFn_FetchProvidersForEmail);
  if (!ValidateEmail(&promise, email)) {
    return promise.LastResult();
  }

  App app_handle = app();
  typedef CreateAuthUriRequest RequestT;
  auto request = std::unique_ptr<RequestT>(  // NOLINT
      new RequestT(app_handle, GetApiKey(*auth_data_), email));

  const auto callback =
      [](AuthDataHandle<FetchProvidersResult, RequestT>* handle) {
        const auto response =
            GetResponse<CreateAuthUriResponse>(*handle->request);
        if (response.IsSuccessful()) {
          FetchProvidersResult result;
          result.providers = response.providers();
          handle->promise.CompleteWithResult(result);
        } else {
          FailPromise(&handle->promise, response.error_code());
        }
      };

  return CallAsync(auth_data_.get(), promise, std::move(request), callback);
}

Future<void> Auth::SendPasswordResetEmail(const char* email) {
  if (!auth_data_) return Future<void>();
  Promise<void> promise(&auth_data_->future_impl,
                        kAuthFn_SendPasswordResetEmail);
  if (!ValidateEmail(&promise, email)) {
    return promise.LastResult();
  }

  const char* language_code = nullptr;
  auto auth_impl = static_cast<AuthImpl*>(auth_data_->auth_impl);
  if (!auth_impl->language_code.empty()) {
    language_code = auth_impl->language_code.c_str();
  }

  App app_handle = app();
  typedef GetOobConfirmationCodeRequest RequestT;
  auto request = RequestT::CreateSendPasswordResetEmailRequest(
      app_handle, GetApiKey(*auth_data_), email, language_code);

  const auto callback = [](AuthDataHandle<void, RequestT>* handle) {
    const auto response =
        GetResponse<GetOobConfirmationCodeResponse>(*handle->request);
    if (response.IsSuccessful()) {
      handle->promise.Complete();
    } else {
      FailPromise(&handle->promise, response.error_code());
    }
  };

  return CallAsync(auth_data_.get(), promise, std::move(request), callback);
}

void Auth::SignOut() {
  if (!auth_data_) return;
  // No REST request. So we can do this in the main thread.
  AuthenticationResult::SignOut(auth_data_.get());
}

// AuthStateListener to wait for current_user() until persistent
// cache load is finished.
class CurrentUserBlockListener : public firebase::auth::AuthStateListener {
 public:
  CurrentUserBlockListener() : semaphore_(0) {}
  ~CurrentUserBlockListener() override {}

  void OnAuthStateChanged(Auth /*auth*/) override { semaphore_.Post(); }

  void WaitForEvent() { semaphore_.Wait(); }

 private:
  Semaphore semaphore_;
};

User Auth::current_user() {
  if (!auth_data_) return User();

  // Add a listener and wait for the first trigger.
  CurrentUserBlockListener listener;
  AddAuthStateListener(&listener);
  // If the persistent cache has not been loaded, this waits until loading
  // finishes and OnAuthStateChanged() triggers. Otherwise OnAuthStateChanged()
  // fires synchronously inside AddAuthStateListener().
  listener.WaitForEvent();
  RemoveAuthStateListener(&listener);

  return auth_data_->MakeCurrentUserHandle();
}

std::string Auth::language_code() const {
  if (!auth_data_) return "";
  auto auth_impl = static_cast<AuthImpl*>(auth_data_->auth_impl);
  return auth_impl->language_code;
}

void Auth::set_language_code(const char* language_code) {
  if (!auth_data_) return;

  auto auth_impl = static_cast<AuthImpl*>(auth_data_->auth_impl);
  std::string code;
  if (language_code != nullptr) {
    code.assign(language_code);
  }
  auth_impl->language_code = code;
}

void Auth::UseAppLanguage() {
  if (!auth_data_) return;
  auto auth_impl = static_cast<AuthImpl*>(auth_data_->auth_impl);
  auth_impl->language_code.clear();
}

void Auth::UseEmulator(std::string host, uint32_t port) {
  if (!auth_data_) return;
  auto auth_impl = static_cast<AuthImpl*>(auth_data_->auth_impl);
  auth_impl->assigned_emulator_url = "";
  auth_impl->assigned_emulator_url.append(host);
  auth_impl->assigned_emulator_url.append(":");
  auth_impl->assigned_emulator_url.append(std::to_string(port));
  LogInfo("Using Auth Emulator: %s", auth_impl->assigned_emulator_url.c_str());
}

void InitializeTokenRefresher(AuthData* auth_data) {
  auto auth_impl = static_cast<AuthImpl*>(auth_data->auth_impl);
  auth_impl->token_refresh_thread.Initialize(auth_data);
}

void DestroyTokenRefresher(AuthData* auth_data) {
  auto auth_impl = static_cast<AuthImpl*>(auth_data->auth_impl);
  auth_impl->token_refresh_thread.Destroy();
}

void EnableTokenAutoRefresh(AuthData* auth_data) {
  auto auth_impl = static_cast<AuthImpl*>(auth_data->auth_impl);
  if (auth_impl) {
    auth_impl->token_refresh_thread.EnableAuthRefresh();
  }
}

void DisableTokenAutoRefresh(AuthData* auth_data) {
  auto auth_impl = static_cast<AuthImpl*>(auth_data->auth_impl);
  if (auth_impl) {
    auth_impl->token_refresh_thread.DisableAuthRefresh();
  }
}

// Called automatically whenever anyone refreshes the auth token.
void ResetTokenRefreshCounter(AuthData* auth_data) {
  auto auth_impl = static_cast<AuthImpl*>(auth_data->auth_impl);
  if (auth_impl) {
    auth_impl->token_refresh_thread.WakeThread();
  }
}

void InitializeUserDataPersist(AuthData* auth_data) {
  auto auth_impl = static_cast<AuthImpl*>(auth_data->auth_impl);
  auth_impl->user_data_persist = std::make_unique<UserDataPersist>(
      internal::CreateAppIdentifierFromOptions(auth_data->app_state.options())
          .c_str());

  auth_data->AddInternalAuthStateListener(auth_impl->user_data_persist.get());
  auth_impl->user_data_persist->LoadUserData(auth_data);
}

void DestroyUserDataPersist(AuthData* auth_data) {
  auto auth_impl = static_cast<AuthImpl*>(auth_data->auth_impl);
  auth_data->RemoveInternalAuthStateListener(
      auth_impl->user_data_persist.get());
}

void LoadFinishTriggerListeners(AuthData* auth_data) {
  MutexLock destructing_lock(auth_data->destructing_mutex);
  if (auth_data->destructing) {
    // If auth is destructing, abort.
    return;
  }
  // Obtain Auth handle before acquiring listeners_mutex (strict lock ordering).
  Auth auth_handle = auth_data->MakeAuthHandle();
  if (!auth_handle.is_valid()) {
    return;
  }
  MutexLock lock(auth_data->listeners_mutex);
  NotifyAuthStateListeners(auth_data);
  NotifyIdTokenListeners(auth_data);
}

void IdTokenRefreshThread::WakeThread() { wakeup_sem_.Post(); }

IdTokenRefreshThread::IdTokenRefreshThread()
    : ref_count_(0), is_shutting_down_(false), wakeup_sem_(0) {}

void IdTokenRefreshThread::Initialize(AuthData* auth_data) {
  MutexLock lock(ref_count_mutex_);
  set_is_shutting_down(false);
  auth_data_ = auth_data;
  auth_data_->AddInternalIdTokenListener(&token_refresh_listener_);
  ref_count_ = 0;

  thread_ = firebase::Thread(
      [](IdTokenRefreshThread* refresh_thread) {
        AuthData* auth_data = refresh_thread->auth_data_;
        ExponentialBackoff backoff;

        while (!refresh_thread->is_shutting_down()) {
          // Note: Make sure to always make future_impl.mutex the innermost
          // lock, to prevent deadlocks!
          refresh_thread->ref_count_mutex_.Acquire();
          auth_data->future_impl.mutex().Acquire();
          if (auth_data->user_impl && refresh_thread->ref_count_ > 0) {
            uint64_t ms_since_last_refresh =
                internal::GetTimestampEpoch() -
                refresh_thread->token_refresh_listener_.GetTokenTimestamp();

            if (ms_since_last_refresh >= kMsPerTokenRefresh) {
              Future<std::string> future = GetUserTokenInternal(
                  auth_data, true, kInternalFn_GetTokenForRefresher);
              auth_data->future_impl.mutex().Release();
              refresh_thread->ref_count_mutex_.Release();

              Semaphore future_sem(0);
              future.OnCompletion(
                  [](const firebase::Future<std::string>& result, void* data) {
                    Semaphore* sem = static_cast<Semaphore*>(data);
                    sem->Post();
                  },
                  &future_sem);
              future_sem.Wait();

              if (future.error() != 0) {
                if (!refresh_thread->is_shutting_down()) {
                  refresh_thread->wakeup_sem_.TimedWait(backoff.NextDelayMs());
                }
                continue;
              } else {
                backoff.Reset();
              }
            } else {
              auth_data->future_impl.mutex().Release();
              refresh_thread->ref_count_mutex_.Release();
            }

            // Now that we've got a token, wait until it needs to be refreshed.
            while (!refresh_thread->is_shutting_down()) {
              {
                MutexLock lock(refresh_thread->ref_count_mutex_);
                if (refresh_thread->ref_count_ <= 0) break;
              }

              ms_since_last_refresh =
                  internal::GetTimestampEpoch() -
                  refresh_thread->token_refresh_listener_.GetTokenTimestamp();

              if (!refresh_thread->wakeup_sem_.TimedWait(
                      kMsPerTokenRefresh - ms_since_last_refresh)) {
                break;
              }
            }
          } else {
            auth_data->future_impl.mutex().Release();
            refresh_thread->ref_count_mutex_.Release();

            if (!refresh_thread->is_shutting_down()) {
              refresh_thread->wakeup_sem_.Wait();
            }
          }
        }
      },
      this);
}

void IdTokenRefreshThread::Destroy() {
  assert(!is_shutting_down());
  set_is_shutting_down(true);
  if (auth_data_) {
    auth_data_->RemoveInternalIdTokenListener(&token_refresh_listener_);
  }

  wakeup_sem_.Post();

  assert(thread_.Joinable());
  thread_.Join();
  auth_data_ = nullptr;
}

void IdTokenRefreshThread::EnableAuthRefresh() {
  {
    MutexLock lock(ref_count_mutex_);
    ref_count_++;
  }
  wakeup_sem_.Post();
}

void IdTokenRefreshThread::DisableAuthRefresh() {
  MutexLock lock(ref_count_mutex_);
  ref_count_--;
}

}  // namespace auth
}  // namespace firebase
