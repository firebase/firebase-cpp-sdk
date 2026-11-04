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

#include "app/rest/transport_curl.h"
#include "app/src/app_common.h"
#include "app/src/app_identifier.h"
#include "app/src/assert.h"
#include "app/src/function_registry.h"
#include "app/src/include/firebase/app.h"
#include "auth/src/common.h"
#include "auth/src/common/promise.h"
#include "auth/src/data.h"
#include "auth/src/desktop/auth_data_handle.h"
#include "auth/src/desktop/auth_util.h"
#include "auth/src/desktop/authentication_result.h"
#include "auth/src/desktop/credential_impl.h"
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

void LogHeartbeat(Auth* const auth) {
  if (auth) auth->app().LogHeartbeat();
}

namespace {

template <typename ResultT>
void DoSignInWithCredential(Promise<ResultT> promise, AuthData* const auth_data,
                            const std::string& provider,
                            const void* const raw_credential) {
  FIREBASE_ASSERT_RETURN_VOID(auth_data);
  if (!ValidateCredential(&promise, provider, raw_credential)) return;

  auto request =
      CreateRequestFromCredential(auth_data, provider, raw_credential);
  if (!request) {
    FailPromise(&promise, kAuthErrorInvalidCredential);
    return;
  }
  CallAsync(auth_data, promise, std::move(request),
            provider == kEmailPasswordAuthProviderId
                ? PerformSignInFlow<VerifyPasswordResponse, ResultT,
                                    AuthRequest>
                : PerformSignInFlow<VerifyAssertionResponse, ResultT,
                                    AuthRequest>);
}

}  // namespace

void* CreatePlatformAuth(App* const app) {
  FIREBASE_ASSERT_RETURN(nullptr, app);
  AuthImpl* const auth = new AuthImpl();
  auth->api_key = app->options().api_key();
  auth->app_name = app->name();

  const char* emulator_host = std::getenv("FIREBASE_AUTH_EMULATOR_HOST");
  if (emulator_host && emulator_host[0] != '\0') {
    auth->assigned_emulator_url = emulator_host;
    LogInfo("Using Auth Emulator: %s", auth->assigned_emulator_url.c_str());
  } else {
    const char* use_emulator = std::getenv("USE_AUTH_EMULATOR");
    if (use_emulator && use_emulator[0] != '\0' &&
        std::strcmp(use_emulator, "0") != 0 &&
        std::strcmp(use_emulator, "false") != 0 &&
        std::strcmp(use_emulator, "FALSE") != 0) {
      const char* port = std::getenv("AUTH_EMULATOR_PORT");
      auth->assigned_emulator_url =
          std::string(kEmulatorLocalHost) + ":" +
          ((port && port[0] != '\0') ? port : kEmulatorPort);
      LogInfo("Using Auth Emulator: %s", auth->assigned_emulator_url.c_str());
    }
  }
  return auth;
}

void InitializeFunctionRegistryListener(AuthData* auth_data);
void DestroyFunctionRegistryListener(AuthData* auth_data);

IdTokenRefreshListener::IdTokenRefreshListener() : token_timestamp_(0) {}
IdTokenRefreshListener::~IdTokenRefreshListener() {}

void IdTokenRefreshListener::OnIdTokenChanged(Auth* auth) {
  if (!auth || !auth->auth_data_) return;
  MutexLock lock(mutex_);
  std::string new_token;
  if (UserView::TryRead(auth->auth_data_, [&](const UserView::Reader& reader) {
        new_token = reader->id_token;
      })) {
    ResetTokenRefreshCounter(auth->auth_data_);
    current_token_ = std::move(new_token);
    token_timestamp_ = ::firebase::internal::GetTimestampEpoch();
  } else {
    current_token_.clear();
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

bool Auth::GetAuthTokenForRegistry(App* app, void* /*unused*/, void* out) {
  if (!app || !out) return false;
  Auth* auth = Auth::FindAuth(app);
  if (!auth) return false;
  (void)auth->current_user();
  *static_cast<std::string*>(out) =
      static_cast<AuthImpl*>(auth->auth_data_->auth_impl)
          ->token_refresh_thread.CurrentAuthToken();
  return true;
}

bool Auth::GetAuthTokenAsyncForRegistry(App* app, void* force_refresh,
                                        void* out) {
  auto* out_future = static_cast<Future<std::string>*>(out);
  if (out_future) *out_future = Future<std::string>();
  const bool refresh =
      force_refresh ? *static_cast<bool*>(force_refresh) : false;
  Auth* auth = app ? Auth::FindAuth(app) : nullptr;
  if (!auth) return false;
  User user = auth->current_user();
  if (!user.is_valid()) return false;
  Future<std::string> future =
      user.GetTokenInternal(refresh, kInternalFn_GetTokenForFunctionRegistry);
  if (out_future) *out_future = future;
  return true;
}

bool Auth::GetCurrentUserUidForRegistry(App* app, void* /*unused*/, void* out) {
  auto* out_string = static_cast<std::string*>(out);
  if (out_string) out_string->clear();
  Auth* auth = Auth::FindAuth(app);
  if (!auth) return false;
  User user = auth->current_user();
  if (!user.is_valid()) return false;
  if (out_string) *out_string = user.uid();
  return true;
}

bool Auth::StartTokenRefreshThreadForRegistry(App* app, void* /*unused*/,
                                              void* /*unused*/) {
  Auth* auth = Auth::FindAuth(app);
  if (!auth) return false;
  EnableTokenAutoRefresh(auth->auth_data_);
  return true;
}

bool Auth::StopTokenRefreshThreadForRegistry(App* app, void* /*unused*/,
                                             void* /*unused*/) {
  Auth* auth = Auth::FindAuth(app);
  if (!auth) return false;
  DisableTokenAutoRefresh(auth->auth_data_);
  return true;
}

void FunctionRegistryAuthStateListener::AddListener(
    FunctionRegistryCallback callback, void* context) {
  if (!callback) return;
  MutexLock lock(mutex_);
  callbacks_.emplace_back(callback, context);
}

void FunctionRegistryAuthStateListener::RemoveListener(
    FunctionRegistryCallback callback, void* context) {
  MutexLock lock(mutex_);
  auto iter =
      std::find(callbacks_.begin(), callbacks_.end(), Entry{callback, context});
  if (iter != callbacks_.end()) callbacks_.erase(iter);
}

void FunctionRegistryAuthStateListener::OnAuthStateChanged(Auth* /*auth*/) {
  std::vector<Entry> callbacks_copy;
  {
    MutexLock lock(mutex_);
    callbacks_copy = callbacks_;
  }
  for (const Entry& entry : callbacks_copy) {
    if (entry.first) entry.first(entry.second);
  }
}

bool Auth::AddAuthStateListenerForRegistry(App* app, void* callback,
                                           void* context) {
  if (!app || !callback) return false;
  auto typed_callback = reinterpret_cast<FunctionRegistryCallback>(callback);
  Auth* auth = Auth::FindAuth(app);
  if (!auth || !auth->auth_data_) return false;
  auto auth_impl = static_cast<AuthImpl*>(auth->auth_data_->auth_impl);
  if (!auth_impl || !auth_impl->internal_listeners) return false;
  auth_impl->internal_listeners->AddListener(typed_callback, context);
  typed_callback(context);
  return true;
}

bool Auth::RemoveAuthStateListenerForRegistry(App* app, void* callback,
                                              void* context) {
  if (!app || !callback) return false;
  auto typed_callback = reinterpret_cast<FunctionRegistryCallback>(callback);
  Auth* auth = Auth::FindAuth(app);
  if (!auth || !auth->auth_data_) return false;
  auto auth_impl = static_cast<AuthImpl*>(auth->auth_data_->auth_impl);
  if (!auth_impl || !auth_impl->internal_listeners) return false;
  auth_impl->internal_listeners->RemoveListener(typed_callback, context);
  return true;
}

void Auth::InitPlatformAuth(AuthData* const auth_data) {
  firebase::rest::InitTransportCurl();
  auto* reg = auth_data->app->function_registry();
  reg->RegisterFunction(::firebase::internal::FnAuthGetCurrentToken,
                        Auth::GetAuthTokenForRegistry);
  reg->RegisterFunction(::firebase::internal::FnAuthGetCurrentUserUid,
                        Auth::GetCurrentUserUidForRegistry);
  reg->RegisterFunction(::firebase::internal::FnAuthStartTokenListener,
                        Auth::StartTokenRefreshThreadForRegistry);
  reg->RegisterFunction(::firebase::internal::FnAuthStopTokenListener,
                        Auth::StopTokenRefreshThreadForRegistry);
  reg->RegisterFunction(::firebase::internal::FnAuthGetTokenAsync,
                        Auth::GetAuthTokenAsyncForRegistry);
  reg->RegisterFunction(::firebase::internal::FnAuthAddAuthStateListener,
                        Auth::AddAuthStateListenerForRegistry);
  reg->RegisterFunction(::firebase::internal::FnAuthRemoveAuthStateListener,
                        Auth::RemoveAuthStateListenerForRegistry);

  InitializeFunctionRegistryListener(auth_data);
  InitializeUserDataPersist(auth_data);
  InitializeTokenRefresher(auth_data);
}

void Auth::DestroyPlatformAuth(AuthData* const auth_data) {
  FIREBASE_ASSERT_RETURN_VOID(auth_data);
  auto auth_impl = static_cast<AuthImpl*>(auth_data->auth_impl);

  auto* reg = auth_data->app->function_registry();
  for (auto fn : {::firebase::internal::FnAuthRemoveAuthStateListener,
                  ::firebase::internal::FnAuthAddAuthStateListener,
                  ::firebase::internal::FnAuthGetCurrentToken,
                  ::firebase::internal::FnAuthGetCurrentUserUid,
                  ::firebase::internal::FnAuthStartTokenListener,
                  ::firebase::internal::FnAuthStopTokenListener,
                  ::firebase::internal::FnAuthGetTokenAsync}) {
    reg->UnregisterFunction(fn);
  }

  DestroyTokenRefresher(auth_data);
  auth_impl->scheduler_.CancelAllAndShutdownWorkerThread();
  DestroyFunctionRegistryListener(auth_data);
  DestroyUserDataPersist(auth_data);
  UserView::ClearUser(auth_data);

  delete auth_impl;
  auth_data->auth_impl = nullptr;
  firebase::rest::CleanupTransportCurl();
}

template <typename RequestT, typename ResponseT, typename PromiseT>
void RunDesktopSignInTask(AuthData* auth_data, PromiseT promise,
                          std::unique_ptr<RequestT> request) {
  CallAsync(auth_data, promise, std::move(request),
            PerformSignInFlow<ResponseT>);
}

void SignInWithCustomToken(AuthData* auth_data, Promise<AuthResult> promise,
                           const char* custom_token) {
  RunDesktopSignInTask<VerifyCustomTokenRequest, VerifyCustomTokenResponse,
                       Promise<AuthResult>>(
      auth_data, promise,
      std::make_unique<VerifyCustomTokenRequest>(
          *auth_data->app, GetApiKey(*auth_data), custom_token));
}

void SignInWithCredential(AuthData* auth_data, Promise<User> promise,
                          const Credential& credential) {
  DoSignInWithCredential(promise, auth_data, credential.provider(),
                         internal::CredentialAccess::GetImpl(credential));
}

Future<AuthResult> SignInWithProvider(AuthData* auth_data,
                                      FederatedAuthProvider* provider) {
  FIREBASE_ASSERT_RETURN(Future<AuthResult>(), provider);
  return provider->SignIn(auth_data);
}

void SignInAnonymously(AuthData* auth_data, Promise<AuthResult> promise) {
  bool is_anonymous = false;
  User* api_user_to_return = nullptr;
  UserView::TryRead(auth_data, [&](const UserView::Reader& reader) {
    is_anonymous = reader->is_anonymous;
    api_user_to_return = &auth_data->current_user;
  });

  if (is_anonymous) {
    AuthResult auth_result;
    auth_result.user = *api_user_to_return;
    promise.CompleteWithResult(auth_result);
    return;
  }

  CallAsync(auth_data, promise,
            std::make_unique<SignUpNewUserRequest>(*auth_data->app,
                                                   GetApiKey(*auth_data)),
            PerformSignInFlow<SignUpNewUserResponse>);
}

void SignInWithEmailAndPassword(AuthData* auth_data,
                                Promise<AuthResult> promise,
                                const char* const email,
                                const char* const password) {
  RunDesktopSignInTask<VerifyPasswordRequest, VerifyPasswordResponse,
                       Promise<AuthResult>>(
      auth_data, promise,
      std::make_unique<VerifyPasswordRequest>(
          *auth_data->app, GetApiKey(*auth_data), email, password));
}

void CreateUserWithEmailAndPassword(AuthData* auth_data,
                                    Promise<AuthResult> promise,
                                    const char* const email,
                                    const char* const password) {
  RunDesktopSignInTask<SignUpNewUserRequest, SignUpNewUserResponse,
                       Promise<AuthResult>>(
      auth_data, promise,
      std::make_unique<SignUpNewUserRequest>(
          *auth_data->app, GetApiKey(*auth_data), email, password, ""));
}

void SignInAndRetrieveDataWithCredential(AuthData* auth_data,
                                         Promise<AuthResult> promise,
                                         const Credential& credential) {
  DoSignInWithCredential(promise, auth_data, credential.provider(),
                         internal::CredentialAccess::GetImpl(credential));
}

void FetchProvidersForEmail(AuthData* auth_data,
                            Promise<Auth::FetchProvidersResult> promise,
                            const char* email) {
  if (!ValidateEmail(&promise, email)) return;
  CallAsync(
      auth_data, promise,
      std::make_unique<CreateAuthUriRequest>(*auth_data->app,
                                             GetApiKey(*auth_data), email),
      +[](AuthDataHandle<Auth::FetchProvidersResult, CreateAuthUriRequest>* p) {
        const auto response = GetResponse<CreateAuthUriResponse>(*p->request);
        if (response.IsSuccessful()) {
          Auth::FetchProvidersResult result;
          result.providers = response.providers();
          p->promise.CompleteWithResult(result);
        } else {
          FailPromise(&p->promise, response.error_code());
        }
      });
}

void SendPasswordResetEmail(AuthData* auth_data, Promise<void> promise,
                            const char* email) {
  if (!ValidateEmail(&promise, email)) return;
  std::string lang;
  {
    auto auth_impl = static_cast<AuthImpl*>(auth_data->auth_impl);
    MutexLock lock(auth_impl->provider_mutex);
    lang = auth_impl->language_code;
  }
  CallAsync(
      auth_data, promise,
      GetOobConfirmationCodeRequest::CreateSendPasswordResetEmailRequest(
          *auth_data->app, GetApiKey(*auth_data), email,
          lang.empty() ? nullptr : lang.c_str()),
      +[](AuthDataHandle<void, GetOobConfirmationCodeRequest>* p) {
        const auto response =
            GetResponse<GetOobConfirmationCodeResponse>(*p->request);
        if (response.IsSuccessful()) {
          p->promise.Complete();
        } else {
          FailPromise(&p->promise, response.error_code());
        }
      });
}

std::string Auth::language_code() const {
  if (!auth_data_) return "";
  auto auth_impl = static_cast<AuthImpl*>(auth_data_->auth_impl);
  MutexLock lock(auth_impl->provider_mutex);
  return auth_impl->language_code;
}

void Auth::set_language_code(const char* language_code) {
  if (!auth_data_) return;
  auto auth_impl = static_cast<AuthImpl*>(auth_data_->auth_impl);
  MutexLock lock(auth_impl->provider_mutex);
  auth_impl->language_code = language_code ? language_code : "";
}

void Auth::UseAppLanguage() { set_language_code(nullptr); }

void Auth::UseEmulator(std::string host, uint32_t port) {
  if (!auth_data_) return;
  auto auth_impl = static_cast<AuthImpl*>(auth_data_->auth_impl);
  MutexLock lock(auth_impl->provider_mutex);
  auth_impl->assigned_emulator_url = host + ":" + std::to_string(port);
  LogInfo("Using Auth Emulator: %s", auth_impl->assigned_emulator_url.c_str());
}

void InitializeTokenRefresher(AuthData* auth_data) {
  static_cast<AuthImpl*>(auth_data->auth_impl)
      ->token_refresh_thread.Initialize(auth_data);
}

void DestroyTokenRefresher(AuthData* auth_data) {
  static_cast<AuthImpl*>(auth_data->auth_impl)->token_refresh_thread.Destroy();
}

void InitializeFunctionRegistryListener(AuthData* auth_data) {
  auto auth_impl = static_cast<AuthImpl*>(auth_data->auth_impl);
  auth_impl->internal_listeners =
      std::make_unique<FunctionRegistryAuthStateListener>();
  auth_data->auth->AddAuthStateListener(auth_impl->internal_listeners.get());
}

void DestroyFunctionRegistryListener(AuthData* auth_data) {
  auto auth_impl = static_cast<AuthImpl*>(auth_data->auth_impl);
  auth_data->auth->RemoveAuthStateListener(auth_impl->internal_listeners.get());
  auth_impl->internal_listeners.reset(nullptr);
}

void EnableTokenAutoRefresh(AuthData* auth_data) {
  static_cast<AuthImpl*>(auth_data->auth_impl)
      ->token_refresh_thread.EnableAuthRefresh();
}

void DisableTokenAutoRefresh(AuthData* auth_data) {
  static_cast<AuthImpl*>(auth_data->auth_impl)
      ->token_refresh_thread.DisableAuthRefresh();
}

void ResetTokenRefreshCounter(AuthData* auth_data) {
  static_cast<AuthImpl*>(auth_data->auth_impl)
      ->token_refresh_thread.WakeThread();
}

void InitializeUserDataPersist(AuthData* auth_data) {
  auto auth_impl = static_cast<AuthImpl*>(auth_data->auth_impl);
  auth_impl->user_data_persist = std::make_unique<UserDataPersist>(
      ::firebase::internal::CreateAppIdentifierFromOptions(
          auth_data->app->options())
          .c_str());
  {
    MutexLock lock(auth_data->listeners_mutex);
    auth_data->persistent_cache_load_pending = true;
  }
  auth_data->auth->AddAuthStateListener(auth_impl->user_data_persist.get());
  auth_impl->user_data_persist->LoadUserData(auth_data);
}

void DestroyUserDataPersist(AuthData* auth_data) {
  auto auth_impl = static_cast<AuthImpl*>(auth_data->auth_impl);
  auth_data->auth->RemoveAuthStateListener(auth_impl->user_data_persist.get());
}

void LoadFinishTriggerListeners(AuthData* auth_data) {
  MutexLock destructing_lock(auth_data->destructing_mutex);
  if (auth_data->destructing) return;
  MutexLock lock(auth_data->listeners_mutex);
  if (!auth_data->persistent_cache_load_pending) return;
  NotifyAuthStateListeners(auth_data);
  NotifyIdTokenListeners(auth_data);
}

void SignOut(AuthData* auth_data) { AuthenticationResult::SignOut(auth_data); }

void IdTokenRefreshThread::WakeThread() { wakeup_sem_.Post(); }

IdTokenRefreshThread::IdTokenRefreshThread()
    : ref_count_(0), is_shutting_down_(false), wakeup_sem_(0), auth(nullptr) {}

void IdTokenRefreshThread::Initialize(AuthData* auth_data) {
  MutexLock lock(ref_count_mutex_);
  set_is_shutting_down(false);
  auth = auth_data->auth;
  ref_count_ = 0;
  auth->AddIdTokenListener(&token_refresh_listener_);

  thread_ = firebase::Thread(
      [](IdTokenRefreshThread* refresh_thread) {
        Auth* auth = refresh_thread->auth;
        ExponentialBackoff backoff;

        while (!refresh_thread->is_shutting_down()) {
          refresh_thread->ref_count_mutex_.Acquire();
          uint64_t ms_since_last_refresh =
              ::firebase::internal::GetTimestampEpoch() -
              refresh_thread->token_refresh_listener_.GetTokenTimestamp();
          auth->auth_data_->future_impl.mutex().Acquire();
          if (auth->auth_data_->user_impl && refresh_thread->ref_count_ > 0) {
            if (ms_since_last_refresh >= kMsPerTokenRefresh) {
              Future<std::string> future =
                  refresh_thread->auth->auth_data_->current_user
                      .GetTokenInternal(true, kInternalFn_GetTokenForRefresher);
              auth->auth_data_->future_impl.mutex().Release();
              refresh_thread->ref_count_mutex_.Release();

              Semaphore future_sem(0);
              future.OnCompletion(
                  [](const firebase::Future<std::string>& /*result*/,
                     void* data) { static_cast<Semaphore*>(data)->Post(); },
                  &future_sem);
              future_sem.Wait();

              if (future.error() != 0) {
                if (!refresh_thread->is_shutting_down()) {
                  refresh_thread->wakeup_sem_.TimedWait(backoff.NextDelayMs());
                }
                continue;
              }
              backoff.Reset();
            } else {
              auth->auth_data_->future_impl.mutex().Release();
              refresh_thread->ref_count_mutex_.Release();
            }

            while (!refresh_thread->is_shutting_down()) {
              {
                MutexLock lock(refresh_thread->ref_count_mutex_);
                if (refresh_thread->ref_count_ <= 0) break;
              }
              ms_since_last_refresh =
                  ::firebase::internal::GetTimestampEpoch() -
                  refresh_thread->token_refresh_listener_.GetTokenTimestamp();
              const int wait_ms =
                  static_cast<uint64_t>(kMsPerTokenRefresh) >
                          ms_since_last_refresh
                      ? static_cast<int>(kMsPerTokenRefresh -
                                         ms_since_last_refresh)
                      : 0;
              if (!refresh_thread->wakeup_sem_.TimedWait(wait_ms)) break;
            }
          } else {
            auth->auth_data_->future_impl.mutex().Release();
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
  auth->RemoveIdTokenListener(&token_refresh_listener_);
  wakeup_sem_.Post();
  assert(thread_.Joinable());
  thread_.Join();
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
  if (ref_count_ > 0) ref_count_--;
}

}  // namespace auth
}  // namespace firebase
