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

void LogHeartbeat(Auth* const auth) {
  if (auth) {
    auth->app().LogHeartbeat();
  }
}

namespace {

template <typename ResultT>
void DoSignInWithCredential(Promise<ResultT> promise,
                                       AuthData* const auth_data,
                                       const std::string& provider,
                                       const void* const raw_credential) {
  FIREBASE_ASSERT_RETURN(Future<ResultT>(), auth_data);

  if (!ValidateCredential(&promise, provider, raw_credential)) {
  }

  auto request =
      CreateRequestFromCredential(auth_data, provider, raw_credential);
  if (!request) {
    FailPromise(&promise, kAuthErrorInvalidCredential);
  }

  if (provider == kEmailPasswordAuthProviderId) {
    CallAsync(auth_data, promise, std::move(request),
              PerformSignInFlow<VerifyPasswordResponse>);
  } else {
    CallAsync(auth_data, promise, std::move(request),
              PerformSignInFlow<VerifyAssertionResponse>);
  }
}

}  // namespace

void* CreatePlatformAuth(App* const app) {
  FIREBASE_ASSERT_RETURN(nullptr, app);

  AuthImpl* const auth = new AuthImpl();
  auth->api_key = app->options().api_key();
  auth->app_name = app->name();

  // Check environment variables for emulator if set.
  const char* emulator_host = std::getenv("FIREBASE_AUTH_EMULATOR_HOST");
  if (emulator_host != nullptr && emulator_host[0] != '\0') {
    auth->assigned_emulator_url = emulator_host;
    LogInfo("Using Auth Emulator: %s", auth->assigned_emulator_url.c_str());
  } else {
    const char* use_emulator = std::getenv("USE_AUTH_EMULATOR");
    if (use_emulator != nullptr && use_emulator[0] != '\0' &&
        std::strcmp(use_emulator, "0") != 0 &&
        std::strcmp(use_emulator, "false") != 0 &&
        std::strcmp(use_emulator, "FALSE") != 0) {
      auth->assigned_emulator_url.append(kEmulatorLocalHost);
      auth->assigned_emulator_url.append(":");
      const char* emulator_port = std::getenv("AUTH_EMULATOR_PORT");
      if (emulator_port == nullptr || emulator_port[0] == '\0') {
        auth->assigned_emulator_url.append(kEmulatorPort);
      } else {
        auth->assigned_emulator_url.append(emulator_port);
      }
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
  // Note: Make sure to always make future_impl.mutex the innermost lock,
  // to prevent deadlocks! UserView::TryRead locks future_impl.mutex().
  MutexLock lock(mutex_);
  std::string new_token;
  const bool is_signed_in = UserView::TryRead(
      auth->auth_data_,
      [&](const UserView::Reader& reader) { new_token = reader->id_token; });
  if (is_signed_in) {
    ResetTokenRefreshCounter(auth->auth_data_);
    current_token_ = std::move(new_token);
    token_timestamp_ = internal::GetTimestampEpoch();
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

// This is the static version of GetAuthToken, with a function signature
// appropriate for the function registry. It returns the cached auth token on
// the current auth object.
bool Auth::GetAuthTokenForRegistry(App* app, void* /*unused*/, void* out) {
  if (!app || !out) return false;
  Auth* auth = Auth::FindAuth(app);
  if (auth) {
    // Make sure the persistent cache is loaded.
    (void)auth->current_user();

    auto result = static_cast<std::string*>(out);
    auto auth_impl = static_cast<AuthImpl*>(auth->auth_data_->auth_impl);
    *result = auth_impl->token_refresh_thread.CurrentAuthToken();
    return true;
  }
  return false;
}

// It basically just calls the public GetToken function on the current user and
// output the Future.
bool Auth::GetAuthTokenAsyncForRegistry(App* app, void* force_refresh,
                                        void* out) {
  Future<std::string>* out_future = static_cast<Future<std::string>*>(out);
  // Reset the future
  if (out_future) *out_future = Future<std::string>();
  const bool refresh =
      force_refresh ? *static_cast<bool*>(force_refresh) : false;

  if (!app) return false;

  Auth* auth = Auth::FindAuth(app);
  if (auth) {
    User user = auth->current_user();
    if (user.is_valid()) {
      Future<std::string> future = user.GetTokenInternal(
          refresh, kInternalFn_GetTokenForFunctionRegistry);
      if (out_future) {
        *out_future = future;
      }
      return true;
    }
  }
  return false;
}

bool Auth::GetCurrentUserUidForRegistry(App* app, void* /*unused*/, void* out) {
  auto* out_string = static_cast<std::string*>(out);
  if (out_string) {
    // Reset the output regardless of outcome.
    out_string->clear();
  }

  Auth* auth = Auth::FindAuth(app);
  if (!auth) return false;

  User user = auth->current_user();
  if (!user.is_valid()) return false;

  if (out_string) {
    *out_string = user.uid();
  }
  return true;
}

bool Auth::StartTokenRefreshThreadForRegistry(App* app, void* /*unused*/,
                                              void* /*unused*/) {
  Auth* auth = Auth::FindAuth(app);
  if (auth) {
    EnableTokenAutoRefresh(auth->auth_data_);
    return true;
  }
  return false;
}

bool Auth::StopTokenRefreshThreadForRegistry(App* app, void* /*unused*/,
                                             void* /*unused*/) {
  Auth* auth = Auth::FindAuth(app);
  if (auth) {
    DisableTokenAutoRefresh(auth->auth_data_);
    return true;
  }
  return false;
}

using FunctionRegistryCallback = void (*)(void*);

void FunctionRegistryAuthStateListener::AddListener(
    FunctionRegistryCallback callback, void* context) {
  if (!callback) return;
  MutexLock lock(mutex_);
  callbacks_.emplace_back(callback, context);
}

void FunctionRegistryAuthStateListener::RemoveListener(
    FunctionRegistryCallback callback, void* context) {
  MutexLock lock(mutex_);
  Entry entry = {callback, context};

  auto iter = std::find(callbacks_.begin(), callbacks_.end(), entry);
  if (iter != callbacks_.end()) {
    callbacks_.erase(iter);
  }
}

void FunctionRegistryAuthStateListener::OnAuthStateChanged(Auth* /*auth*/) {
  std::vector<Entry> callbacks_copy;
  {
    MutexLock lock(mutex_);
    callbacks_copy = callbacks_;
  }
  for (const Entry& entry : callbacks_copy) {
    if (entry.first) {
      entry.first(entry.second);
    }
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
  auth_data->app->function_registry()->RegisterFunction(
      internal::FnAuthGetCurrentToken, Auth::GetAuthTokenForRegistry);
  auth_data->app->function_registry()->RegisterFunction(
      internal::FnAuthGetCurrentUserUid, Auth::GetCurrentUserUidForRegistry);
  auth_data->app->function_registry()->RegisterFunction(
      internal::FnAuthStartTokenListener,
      Auth::StartTokenRefreshThreadForRegistry);
  auth_data->app->function_registry()->RegisterFunction(
      internal::FnAuthStopTokenListener,
      Auth::StopTokenRefreshThreadForRegistry);
  auth_data->app->function_registry()->RegisterFunction(
      internal::FnAuthGetTokenAsync, Auth::GetAuthTokenAsyncForRegistry);
  auth_data->app->function_registry()->RegisterFunction(
      internal::FnAuthAddAuthStateListener,
      Auth::AddAuthStateListenerForRegistry);
  auth_data->app->function_registry()->RegisterFunction(
      internal::FnAuthRemoveAuthStateListener,
      Auth::RemoveAuthStateListenerForRegistry);

  // Register internal function registry listeners first so they are notified
  // before any user-supplied or persistence listeners.
  InitializeFunctionRegistryListener(auth_data);

  // Load existing UserData
  InitializeUserDataPersist(auth_data);

  InitializeTokenRefresher(auth_data);
}

void Auth::DestroyPlatformAuth(AuthData* const auth_data) {
  FIREBASE_ASSERT_RETURN_VOID(auth_data);
  auto auth_impl = static_cast<AuthImpl*>(auth_data->auth_impl);

  // Unregister from the function registry first so no new internal calls arrive.
  auth_data->app->function_registry()->UnregisterFunction(
      internal::FnAuthRemoveAuthStateListener);
  auth_data->app->function_registry()->UnregisterFunction(
      internal::FnAuthAddAuthStateListener);
  auth_data->app->function_registry()->UnregisterFunction(
      internal::FnAuthGetCurrentToken);
  auth_data->app->function_registry()->UnregisterFunction(
      internal::FnAuthGetCurrentUserUid);
  auth_data->app->function_registry()->UnregisterFunction(
      internal::FnAuthStartTokenListener);
  auth_data->app->function_registry()->UnregisterFunction(
      internal::FnAuthStopTokenListener);
  auth_data->app->function_registry()->UnregisterFunction(
      internal::FnAuthGetTokenAsync);

  // Shut down the token refresher thread BEFORE cancelling the scheduler so
  // any in-flight refresh task on the scheduler completes its future_sem.Wait()
  // instead of being dropped uncompleted and deadlocking thread_.Join().
  DestroyTokenRefresher(auth_data);

  auth_impl->scheduler_.CancelAllAndShutdownWorkerThread();

  DestroyFunctionRegistryListener(auth_data);

  DestroyUserDataPersist(auth_data);

  UserView::ClearUser(auth_data);

  delete static_cast<AuthImpl*>(auth_data->auth_impl);
  auth_data->auth_impl = nullptr;
  firebase::rest::CleanupTransportCurl();
}

// RPCs
namespace internal {
void SignInWithCustomToken(AuthData* auth_data, Promise<AuthResult> promise, const char* custom_token) {
  Promise<AuthResult> promise(&auth_data->future_impl,
                              kAuthFn_SignInWithCustomToken);
  if (!custom_token || strlen(custom_token) == 0) {
    FailPromise(&promise, kAuthErrorInvalidCustomToken);
  }

  typedef VerifyCustomTokenRequest RequestT;
  auto request = std::make_unique<RequestT>(*auth_data->app,
                                            GetApiKey(*auth_data),
                                            custom_token);

  CallAsync(auth_data, promise, std::move(request),
                   PerformSignInFlow<VerifyCustomTokenResponse>);
}
}  // namespace internal

namespace internal {
void SignInWithCredential(AuthData* auth_data, Promise<User> promise, const Credential& credential) {
  DoSignInWithCredential(promise, auth_data, credential.provider(),
                                internal::CredentialAccess::GetImpl(credential));
}
}  // namespace internal

namespace internal {
void SignInWithProvider(AuthData* auth_data, Promise<AuthResult> promise, FederatedAuthProvider* provider) {
  FIREBASE_ASSERT_RETURN_VOID(provider);
  provider->SignIn(auth_data);
}
}  // namespace internal

namespace internal {
void SignInAnonymously(AuthData* auth_data, Promise<AuthResult> promise) {
  Promise<AuthResult> promise(&auth_data->future_impl,
                              kAuthFn_SignInAnonymously);

  // If user is already signed in anonymously, return immediately.
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
  }

  typedef SignUpNewUserRequest RequestT;
  auto request =
      std::make_unique<RequestT>(*auth_data->app, GetApiKey(*auth_data));

  CallAsync(auth_data, promise, std::move(request),
                   PerformSignInFlow<SignUpNewUserResponse>);
}
}  // namespace internal

namespace internal {
void SignInWithEmailAndPassword(AuthData* auth_data, Promise<AuthResult> promise, const char* const email, const char* const password) {
  Promise<AuthResult> promise(&auth_data->future_impl,
                              kAuthFn_SignInWithEmailAndPassword);
  if (!ValidateEmailAndPassword(&promise, email, password)) {
  }

  typedef VerifyPasswordRequest RequestT;
  auto request = std::make_unique<RequestT>(
      *auth_data->app, GetApiKey(*auth_data), email, password);

  CallAsync(auth_data, promise, std::move(request),
                   PerformSignInFlow<VerifyPasswordResponse>);
}
}  // namespace internal

namespace internal {
void CreateUserWithEmailAndPassword(AuthData* auth_data, Promise<AuthResult> promise, const char* const email, const char* const password) {
  Promise<AuthResult> promise(&auth_data->future_impl,
                              kAuthFn_CreateUserWithEmailAndPassword);
  if (!ValidateEmailAndPassword(&promise, email, password)) {
  }

  typedef SignUpNewUserRequest RequestT;
  auto request = std::make_unique<RequestT>(
      *auth_data->app, GetApiKey(*auth_data), email, password, "");

  CallAsync(auth_data, promise, std::move(request),
                   PerformSignInFlow<SignUpNewUserResponse>);
}
}  // namespace internal

namespace internal {
void SignInAndRetrieveDataWithCredential(AuthData* auth_data, Promise<AuthResult> promise, const Credential& credential) {
  Promise<AuthResult> promise(&auth_data->future_impl,
                              kAuthFn_SignInAndRetrieveDataWithCredential);
  DoSignInWithCredential(promise, auth_data, credential.provider(),
                                internal::CredentialAccess::GetImpl(credential));
}
}  // namespace internal

namespace internal {
void FetchProvidersForEmail(AuthData* auth_data, Promise<Auth::FetchProvidersResult> promise, const char* email) {
  Promise<FetchProvidersResult> promise(&auth_data->future_impl,
                                        kAuthFn_FetchProvidersForEmail);
  if (!ValidateEmail(&promise, email)) {
  }

  typedef CreateAuthUriRequest RequestT;
  auto request = std::make_unique<RequestT>(*auth_data->app,
                                            GetApiKey(*auth_data), email);

  const auto callback =
      [](AuthDataHandle<FetchProvidersResult, RequestT>* promise.handle()) {
        const auto response =
            GetResponse<CreateAuthUriResponse>(*promise.handle()->request);
        if (response.IsSuccessful()) {
          FetchProvidersResult result;
          result.providers = response.providers();
          promise.handle()->promise.CompleteWithResult(result);
        } else {
          FailPromise(&promise.handle()->promise, response.error_code());
        }
      };

  CallAsync(auth_data, promise, std::move(request), callback);
}
}  // namespace internal

namespace internal {
void SendPasswordResetEmail(AuthData* auth_data, Promise<void> promise, const char* email) {
  Promise<void> promise(&auth_data->future_impl,
                        kAuthFn_SendPasswordResetEmail);
  if (!ValidateEmail(&promise, email)) {
  }

  std::string language_code_str;
  {
    auto auth_impl = static_cast<AuthImpl*>(auth_data->auth_impl);
    MutexLock lock(auth_impl->provider_mutex);
    language_code_str = auth_impl->language_code;
  }
  const char* language_code =
      language_code_str.empty() ? nullptr : language_code_str.c_str();

  typedef GetOobConfirmationCodeRequest RequestT;
  auto request = RequestT::CreateSendPasswordResetEmailRequest(
      *auth_data->app, GetApiKey(*auth_data), email, language_code);

  const auto callback = [](AuthDataHandle<void, RequestT>* promise.handle()) {
    const auto response =
        GetResponse<GetOobConfirmationCodeResponse>(*promise.handle()->request);
    if (response.IsSuccessful()) {
      promise.handle()->promise.Complete();
    } else {
      FailPromise(&promise.handle()->promise, response.error_code());
    }
  };

  CallAsync(auth_data, promise, std::move(request), callback);
}
}  // namespace internal

void Auth::SignOut() {
  // No REST request. So we can do this in the main thread.
  AuthenticationResult::SignOut(auth_data_);
}

// AuthStateListener to wait for current_user() until persistent
// cache load is finished.
class CurrentUserBlockListener : public firebase::auth::AuthStateListener {
 public:
  CurrentUserBlockListener() : semaphore_(0) {}
  ~CurrentUserBlockListener() override {}

  void OnAuthStateChanged(Auth* auth) override { semaphore_.Post(); }

  void WaitForEvent() { semaphore_.Wait(); }

 private:
  Semaphore semaphore_;
};

User Auth::current_user() {

  // Add a listener and wait for the first trigger.
  CurrentUserBlockListener listener;
  AddAuthStateListener(&listener);
  // If the persistent cache has not be loaded, this would wait until the
  // loading is finished and OnAuthStateChanged() is triggered.
  // If it has been loaded, OnAuthStateChanged() should be triggered recursively
  // and synchronously when AddAuthStateListener() is called.
  listener.WaitForEvent();
  RemoveAuthStateListener(&listener);

  {
    MutexLock lock(auth_data_->future_impl.mutex());
    if (auth_data_->user_impl == nullptr) {
      return User(nullptr);
    }
    return auth_data_->current_user;
  }
}

std::string Auth::language_code() const {
  auto auth_impl = static_cast<AuthImpl*>(auth_data_->auth_impl);
  MutexLock lock(auth_impl->provider_mutex);
  return auth_impl->language_code;
}

void Auth::set_language_code(const char* language_code) {

  auto auth_impl = static_cast<AuthImpl*>(auth_data_->auth_impl);
  MutexLock lock(auth_impl->provider_mutex);
  auth_impl->language_code = language_code != nullptr ? language_code : "";
}

void Auth::UseAppLanguage() {
  auto auth_impl = static_cast<AuthImpl*>(auth_data_->auth_impl);
  MutexLock lock(auth_impl->provider_mutex);
  auth_impl->language_code.clear();
}

void Auth::UseEmulator(std::string host, uint32_t port) {
  auto auth_impl = static_cast<AuthImpl*>(auth_data_->auth_impl);
  MutexLock lock(auth_impl->provider_mutex);
  auth_impl->assigned_emulator_url = host + ":" + std::to_string(port);
  LogInfo("Using Auth Emulator: %s", auth_impl->assigned_emulator_url.c_str());
}

namespace internal {
void InitializeTokenRefresher(AuthData* auth_data) {
  auto auth_impl = static_cast<AuthImpl*>(auth_data->auth_impl);
  auth_impl->token_refresh_thread.Initialize(auth_data);
}
}  // namespace internal

namespace internal {
void DestroyTokenRefresher(AuthData* auth_data) {
  auto auth_impl = static_cast<AuthImpl*>(auth_data->auth_impl);
  auth_impl->token_refresh_thread.Destroy();
}
}  // namespace internal

namespace internal {
void InitializeFunctionRegistryListener(AuthData* auth_data) {
  auto auth_impl = static_cast<AuthImpl*>(auth_data->auth_impl);
  auth_impl->internal_listeners =
      std::make_unique<FunctionRegistryAuthStateListener>();
  auth_data->auth->AddAuthStateListener(auth_impl->internal_listeners.get());
}
}  // namespace internal

namespace internal {
void DestroyFunctionRegistryListener(AuthData* auth_data) {
  auto auth_impl = static_cast<AuthImpl*>(auth_data->auth_impl);
  auth_data->auth->RemoveAuthStateListener(auth_impl->internal_listeners.get());
  auth_impl->internal_listeners.reset(nullptr);
}
}  // namespace internal

namespace internal {
void EnableTokenAutoRefresh(AuthData* auth_data) {
  auto auth_impl = static_cast<AuthImpl*>(auth_data->auth_impl);
  auth_impl->token_refresh_thread.EnableAuthRefresh();
}
}  // namespace internal

namespace internal {
void DisableTokenAutoRefresh(AuthData* auth_data) {
  // We don't actually directly stop the thread here - we just decrease the ref
  // count and it the thread will remove itself next time it fires, if it is
  // no longer needed.
  auto auth_impl = static_cast<AuthImpl*>(auth_data->auth_impl);
  auth_impl->token_refresh_thread.DisableAuthRefresh();
}
}  // namespace internal

// Called automatically whenever anyone refreshes the auth token.
namespace internal {
void ResetTokenRefreshCounter(AuthData* auth_data) {
  auto auth_impl = static_cast<AuthImpl*>(auth_data->auth_impl);
  auth_impl->token_refresh_thread.WakeThread();
}
}  // namespace internal

namespace internal {
void InitializeUserDataPersist(AuthData* auth_data) {
  auto auth_impl = static_cast<AuthImpl*>(auth_data->auth_impl);
  auth_impl->user_data_persist = std::make_unique<UserDataPersist>(
      internal::CreateAppIdentifierFromOptions(auth_data->app->options())
          .c_str());

  {
    MutexLock lock(auth_data->listeners_mutex);
    auth_data->persistent_cache_load_pending = true;
  }
  auth_data->auth->AddAuthStateListener(auth_impl->user_data_persist.get());
  auth_impl->user_data_persist->LoadUserData(auth_data);
}
}  // namespace internal

namespace internal {
void DestroyUserDataPersist(AuthData* auth_data) {
  auto auth_impl = static_cast<AuthImpl*>(auth_data->auth_impl);
  auth_data->auth->RemoveAuthStateListener(auth_impl->user_data_persist.get());
}
}  // namespace internal

namespace internal {
void LoadFinishTriggerListeners(AuthData* auth_data) {
  MutexLock destructing_lock(auth_data->destructing_mutex);
  if (auth_data->destructing) {
    // If auth is destructing, abort.
    return;
  }
  // We would have to block other listener changes to protect race condition
  // on how many times a listener should be triggered. We would rely on first
  // listener trigger to flip the persistence loading bit.
  MutexLock lock(auth_data->listeners_mutex);
  if (!auth_data->persistent_cache_load_pending) {
    return;
  }
  NotifyAuthStateListeners(auth_data);
  NotifyIdTokenListeners(auth_data);
}
}  // namespace internal

void IdTokenRefreshThread::WakeThread() { wakeup_sem_.Post(); }

IdTokenRefreshThread::IdTokenRefreshThread()
    : ref_count_(0), is_shutting_down_(false), wakeup_sem_(0), auth(nullptr) {}

// Called once, at startup.
// Should only be used by the Auth object, on construction.
void IdTokenRefreshThread::Initialize(AuthData* auth_data) {
  MutexLock lock(ref_count_mutex_);
  set_is_shutting_down(false);
  auth = auth_data->auth;
  ref_count_ = 0;
  IdTokenListener* listener = &token_refresh_listener_;
  auth->AddIdTokenListener(listener);

  thread_ = firebase::Thread(
      [](IdTokenRefreshThread* refresh_thread) {
        Auth* auth = refresh_thread->auth;
        ExponentialBackoff backoff;

        while (!refresh_thread->is_shutting_down()) {
          // Note:  Make sure to always make future_impl.mutex the innermost
          // lock, to prevent deadlocks!
          refresh_thread->ref_count_mutex_.Acquire();
          auth->auth_data_->future_impl.mutex().Acquire();
          if (auth->auth_data_->user_impl && refresh_thread->ref_count_ > 0) {
            // The internal identifier kInternalFn_GetTokenForRefresher,
            // ensures that we won't mess with the LastResult for the
            // user-facing one.

            uint64_t ms_since_last_refresh =
                internal::GetTimestampEpoch() -
                refresh_thread->token_refresh_listener_.GetTokenTimestamp();

            if (ms_since_last_refresh >= kMsPerTokenRefresh) {
              Future<std::string> future =
                  refresh_thread->auth->auth_data_->current_user
                      .GetTokenInternal(true, kInternalFn_GetTokenForRefresher);
              auth->auth_data_->future_impl.mutex().Release();
              refresh_thread->ref_count_mutex_.Release();

              Semaphore future_sem(0);
              future.OnCompletion(
                  [](const firebase::Future<std::string>& result, void* data) {
                    Semaphore* sem = static_cast<Semaphore*>(data);
                    sem->Post();
                  },
                  &future_sem);
              // We need to wait for this future to finish because otherwise
              // something might force the thread to shut down before the future
              // is completed.
              future_sem.Wait();

              if (future.error() != 0) {
                // Token refresh failed (e.g. network outage). Wait with
                // exponential backoff to prevent tight retry loops and avoid
                // overloading backend servers upon reconnect. Matches Android
                // DefaultTokenRefresher (30s initial, doubling up to 16m max).
                if (!refresh_thread->is_shutting_down()) {
                  refresh_thread->wakeup_sem_.TimedWait(backoff.NextDelayMs());
                }
                continue;
              } else {
                // Refresh succeeded! Reset backoff to minimum interval.
                backoff.Reset();
              }
            } else {
              auth->auth_data_->future_impl.mutex().Release();
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
              const int wait_ms =
                  static_cast<uint64_t>(kMsPerTokenRefresh) >
                          ms_since_last_refresh
                      ? static_cast<int>(kMsPerTokenRefresh -
                                         ms_since_last_refresh)
                      : 0;

              // If the timed-wait returns true, then it means we were
              // interrupted early - either it's time to shut down, or we
              // got a new token and should restart the clock.
              if (!refresh_thread->wakeup_sem_.TimedWait(wait_ms)) {
                break;
              }
            }

          } else {
            auth->auth_data_->future_impl.mutex().Release();
            refresh_thread->ref_count_mutex_.Release();

            // No user, so we just wait for something to wake up the thread.
            if (!refresh_thread->is_shutting_down()) {
              refresh_thread->wakeup_sem_.Wait();
            }
          }
        }
      },
      this);
}

// Only called by the system, when it's time to shut down the thread.
// Should only be used by the Auth object, on destruction.
void IdTokenRefreshThread::Destroy() {
  assert(!is_shutting_down());
  set_is_shutting_down(true);
  auth->RemoveIdTokenListener(&token_refresh_listener_);

  // All thread pauses are done via timed-wait semaphores, so this will
  // wake the thread up, whatever it's doing.
  wakeup_sem_.Post();

  assert(thread_.Joinable());
  thread_.Join();
}

void IdTokenRefreshThread::EnableAuthRefresh() {
  {
    MutexLock lock(ref_count_mutex_);
    ref_count_++;
  }
  // Need to force a wakeup so the thread can check if it needs to refresh the
  // auth token now.
  wakeup_sem_.Post();
}

void IdTokenRefreshThread::DisableAuthRefresh() {
  MutexLock lock(ref_count_mutex_);
  if (ref_count_ > 0) {
    ref_count_--;
  }
}

}  // namespace auth
}  // namespace firebase
