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

}  // namespace

void* CreatePlatformAuth(App* const app) {
  FIREBASE_ASSERT_RETURN(nullptr, app);

  AuthImpl* const auth = new AuthImpl();
  auth->api_key = app->options().api_key();
  auth->app_name = app->name();
  return auth;
}

IdTokenRefreshListener::IdTokenRefreshListener() : token_timestamp_(0) {}

IdTokenRefreshListener::~IdTokenRefreshListener() {}

void IdTokenRefreshListener::OnIdTokenChanged(Auth* auth) {
  // Note:  Make sure to always make future_impl.mutex the innermost lock,
  // to prevent deadlocks!
  MutexLock lock(mutex_);
  MutexLock future_lock(auth->auth_data_->future_impl.mutex());
  if (auth->current_user()) {
    ResetTokenRefreshCounter(auth->auth_data_);

    // Retrieve id_token from auth_data
    {
      UserView::Reader reader = UserView::GetReader(auth->auth_data_);
      assert(reader.IsValid());
      current_token_ = reader->id_token;
    }
    token_timestamp_ = internal::GetTimestampEpoch();
  } else {
    current_token_ = "";
  }
}

std::string IdTokenRefreshListener::GetCurrentToken() {
  std::string current_token;
  {
    MutexLock lock(mutex_);
    current_token = current_token_;
  }
  return current_token;
}

uint64_t IdTokenRefreshListener::GetTokenTimestamp() {
  uint64_t token_timestamp;
  {
    MutexLock lock(mutex_);
    token_timestamp = token_timestamp_;
  }
  return token_timestamp;
}

// This is the static version of GetAuthToken, with a function signature
// appropriate for the function registry.  It basically just calls the public
// GetAuthToken function on the current auth object.
bool Auth::GetAuthTokenForRegistry(App* app, void* /*unused*/, void* out) {
  if (!app) return false;
  Auth* auth = Auth::FindAuth(app);
  if (auth) {
    // Make sure the persistent cache is loaded.
    auth->current_user();

    auto result = static_cast<std::string*>(out);
    MutexLock lock(auth->auth_data_->token_listener_mutex);
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
  bool* in_force_refresh = static_cast<bool*>(force_refresh);

  if (!app) return false;
  assert(force_refresh);

  Auth* auth = Auth::FindAuth(app);
  if (auth) {
    User* user = auth->current_user();
    if (user) {
      Future<std::string> future = user->GetTokenInternal(
          *in_force_refresh, kInternalFn_GetTokenForFunctionRegistry);
      if (out_future) {
        *out_future = future;
      }
      return true;
    }
  }
  return false;
}

bool Auth::StartTokenRefreshThreadForRegistry(App* app, void* /*unused*/,
                                              void* /*unused*/) {
  if (!app) return false;
  Auth* auth = Auth::FindAuth(app);
  if (auth) {
    EnableTokenAutoRefresh(auth->auth_data_);
    return true;
  }
  return false;
}

bool Auth::StopTokenRefreshThreadForRegistry(App* app, void* /*unused*/,
                                             void* /*unused*/) {
  if (!app) return false;
  Auth* auth = Auth::FindAuth(app);
  if (auth) {
    DisableTokenAutoRefresh(auth->auth_data_);
    return true;
  }
  return false;
}

void Auth::InitPlatformAuth(AuthData* const auth_data) {
  firebase::rest::InitTransportCurl();
  auth_data->app->function_registry()->RegisterFunction(
      internal::FnAuthGetCurrentToken, Auth::GetAuthTokenForRegistry);
  auth_data->app->function_registry()->RegisterFunction(
      internal::FnAuthStartTokenListener,
      Auth::StartTokenRefreshThreadForRegistry);
  auth_data->app->function_registry()->RegisterFunction(
      internal::FnAuthStopTokenListener,
      Auth::StopTokenRefreshThreadForRegistry);
  auth_data->app->function_registry()->RegisterFunction(
      internal::FnAuthGetTokenAsync, Auth::GetAuthTokenAsyncForRegistry);

  // Load existing UserData
  InitializeUserDataPersist(auth_data);

  InitializeTokenRefresher(auth_data);
}

void Auth::DestroyPlatformAuth(AuthData* const auth_data) {
  FIREBASE_ASSERT_RETURN_VOID(auth_data);
  auto auth_impl = static_cast<AuthImpl*>(auth_data->auth_impl);
  auth_impl->scheduler_.CancelAllAndShutdownWorkerThread();
  // Unregister from the function registry.
  auth_data->app->function_registry()->UnregisterFunction(
      internal::FnAuthGetCurrentToken);
  auth_data->app->function_registry()->UnregisterFunction(
      internal::FnAuthStartTokenListener);
  auth_data->app->function_registry()->UnregisterFunction(
      internal::FnAuthStopTokenListener);
  auth_data->app->function_registry()->UnregisterFunction(
      internal::FnAuthGetTokenAsync);

  DestroyTokenRefresher(auth_data);

  DestroyUserDataPersist(auth_data);

  UserView::ClearUser(auth_data);

  delete static_cast<AuthImpl*>(auth_data->auth_impl);
  auth_data->auth_impl = nullptr;
  firebase::rest::CleanupTransportCurl();
}

// RPCs

Future<User*> Auth::SignInWithCustomToken(const char* const custom_token) {
  Promise<User*> promise(&auth_data_->future_impl,
                         kAuthFn_SignInWithCustomToken);
  if (!custom_token || strlen(custom_token) == 0) {
    FailPromise(&promise, kAuthErrorInvalidCustomToken);
    return promise.LastResult();
  }

  typedef VerifyCustomTokenRequest RequestT;
  // Note: std::make_unique is not supported by Visual Studio 2012, which is
  // among our target compilers.
  auto request = std::unique_ptr<RequestT>(  // NOLINT
      new RequestT(GetApiKey(*auth_data_), custom_token));

  return CallAsync(auth_data_, promise, std::move(request),
                   PerformSignInFlow<VerifyCustomTokenResponse>);
}

Future<User*> Auth::SignInWithCredential(const Credential& credential) {
  Promise<User*> promise(&auth_data_->future_impl,
                         kAuthFn_SignInWithCredential);
  if (!ValidateCredential(&promise, credential.provider(), credential.impl_)) {
    return promise.LastResult();
  }

  return DoSignInWithCredential(promise, auth_data_, credential.provider(),
                                credential.impl_);
}

Future<SignInResult> Auth::SignInWithProvider(
      FederatedAuthProvider* provider) {
  FIREBASE_ASSERT_RETURN(Future<SignInResult>(), provider);
  // TODO(b/139363200)
  // return provider->SignIn(auth_data_);

  SafeFutureHandle<SignInResult> handle =
      auth_data_->future_impl.SafeAlloc<SignInResult>(
          kAuthFn_SignInWithProvider);
  auth_data_->future_impl.CompleteWithResult(
      handle, kAuthErrorUnimplemented,
      "Operation is not supported on non-mobile systems.",
      /*result=*/{});
  return MakeFuture(&auth_data_->future_impl, handle);
}

Future<User*> Auth::SignInAnonymously() {
  Promise<User*> promise(&auth_data_->future_impl, kAuthFn_SignInAnonymously);

  // If user is already signed in anonymously, return immediately.
  bool is_anonymous = false;
  User* api_user_to_return = nullptr;
  UserView::TryRead(auth_data_, [&](const UserView::Reader& reader) {
    is_anonymous = reader->is_anonymous;
    api_user_to_return = &auth_data_->current_user;
  });

  if (is_anonymous) {
    promise.CompleteWithResult(api_user_to_return);
    return promise.LastResult();
  }

  typedef SignUpNewUserRequest RequestT;
  auto request = std::unique_ptr<RequestT>(  // NOLINT
      new RequestT(GetApiKey(*auth_data_)));

  return CallAsync(auth_data_, promise, std::move(request),
                   PerformSignInFlow<SignUpNewUserResponse>);
}

Future<User*> Auth::SignInWithEmailAndPassword(const char* const email,
                                               const char* const password) {
  Promise<User*> promise(&auth_data_->future_impl,
                         kAuthFn_SignInWithEmailAndPassword);
  if (!ValidateEmailAndPassword(&promise, email, password)) {
    return promise.LastResult();
  }

  typedef VerifyPasswordRequest RequestT;
  auto request = std::unique_ptr<RequestT>(  // NOLINT
      new RequestT(GetApiKey(*auth_data_), email, password));

  return CallAsync(auth_data_, promise, std::move(request),
                   PerformSignInFlow<VerifyPasswordResponse>);
}

Future<User*> Auth::CreateUserWithEmailAndPassword(const char* const email,
                                                   const char* const password) {
  Promise<User*> promise(&auth_data_->future_impl,
                         kAuthFn_CreateUserWithEmailAndPassword);
  if (!ValidateEmailAndPassword(&promise, email, password)) {
    return promise.LastResult();
  }

  typedef SignUpNewUserRequest RequestT;
  auto request = std::unique_ptr<RequestT>(  // NOLINT
      new RequestT(GetApiKey(*auth_data_), email, password, ""));

  return CallAsync(auth_data_, promise, std::move(request),
                   PerformSignInFlow<SignUpNewUserResponse>);
}

Future<SignInResult> Auth::SignInAndRetrieveDataWithCredential(
    const Credential& credential) {
  Promise<SignInResult> promise(&auth_data_->future_impl,
                                kAuthFn_SignInAndRetrieveDataWithCredential);
  return DoSignInWithCredential(promise, auth_data_, credential.provider(),
                                credential.impl_);
}

Future<Auth::FetchProvidersResult> Auth::FetchProvidersForEmail(
    const char* email) {
  Promise<FetchProvidersResult> promise(&auth_data_->future_impl,
                                        kAuthFn_FetchProvidersForEmail);
  if (!ValidateEmail(&promise, email)) {
    return promise.LastResult();
  }

  typedef CreateAuthUriRequest RequestT;
  auto request = std::unique_ptr<RequestT>(  // NOLINT
      new RequestT(GetApiKey(*auth_data_), email));

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

  return CallAsync(auth_data_, promise, std::move(request), callback);
}

Future<void> Auth::SendPasswordResetEmail(const char* email) {
  Promise<void> promise(&auth_data_->future_impl,
                        kAuthFn_SendPasswordResetEmail);
  if (!ValidateEmail(&promise, email)) {
    return promise.LastResult();
  }

  typedef GetOobConfirmationCodeRequest RequestT;
  auto request = RequestT::CreateSendPasswordResetEmailRequest(
      GetApiKey(*auth_data_), email);
  const auto callback = [](AuthDataHandle<void, RequestT>* handle) {
    const auto response =
        GetResponse<GetOobConfirmationCodeResponse>(*handle->request);
    if (response.IsSuccessful()) {
      handle->promise.Complete();
    } else {
      FailPromise(&handle->promise, response.error_code());
    }
  };

  return CallAsync(auth_data_, promise, std::move(request), callback);
}

void Auth::SignOut() {
  // No REST request. So we can do this in the main thread.
  AuthenticationResult::SignOut(auth_data_);
}

// AuthStateListener to wait for current_user() until persistent cache load is
// finished.
class CurrentUserBlockListener : public firebase::auth::AuthStateListener {
 public:
  explicit CurrentUserBlockListener() : semaphore_(0) {}
  ~CurrentUserBlockListener() override {}

  void OnAuthStateChanged(Auth* auth) override { semaphore_.Post(); }

  void WaitForEvent() { semaphore_.Wait(); }

 private:
  Semaphore semaphore_;
};

// It's safe to return a direct pointer to `current_user` because that class
// holds nothing but a pointer to AuthData, which never changes.
// All User functions that require synchronization go through AuthData's mutex.
User* Auth::current_user() {
  if (!auth_data_) return nullptr;

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
    return auth_data_->user_impl == nullptr ? nullptr
                                            : &auth_data_->current_user;
  }
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
  auth_impl->token_refresh_thread.EnableAuthRefresh();
}

void DisableTokenAutoRefresh(AuthData* auth_data) {
  // We don't actually directly stop the thread here - we just decrease the ref
  // count and it the thread will remove itself next time it fires, if it is
  // no longer needed.
  auto auth_impl = static_cast<AuthImpl*>(auth_data->auth_impl);
  auth_impl->token_refresh_thread.DisableAuthRefresh();
}

// Called automatically whenever anyone refreshes the auth token.
void ResetTokenRefreshCounter(AuthData* auth_data) {
  auto auth_impl = static_cast<AuthImpl*>(auth_data->auth_impl);
  auth_impl->token_refresh_thread.WakeThread();
}

void InitializeUserDataPersist(AuthData* auth_data) {
  auto auth_impl = static_cast<AuthImpl*>(auth_data->auth_impl);
  auth_impl->user_data_persist =
      MakeUnique<UserDataPersist>(
          internal::CreateAppIdentifierFromOptions(
              auth_data->app->options()).c_str());

  auth_data->auth->AddAuthStateListener(auth_impl->user_data_persist.get());
  auth_impl->user_data_persist->LoadUserData(auth_data);
}

void DestroyUserDataPersist(AuthData* auth_data) {
  auto auth_impl = static_cast<AuthImpl*>(auth_data->auth_impl);
  auth_data->auth->RemoveAuthStateListener(auth_impl->user_data_persist.get());
}

void LoadFinishTriggerListeners(AuthData* auth_data) {
  MutexLock destructing_lock(auth_data->desctruting_mutex);
  if (auth_data->destructing) {
    // If auth is destructing, abort.
    return;
  }
  // We would have to block other listener changes to protect race condition
  // on how many times a listener should be triggered. We would rely on first
  // listener trigger to flip the persistence loading bit.
  MutexLock lock(auth_data->listeners_mutex);
  NotifyAuthStateListeners(auth_data);
  NotifyIdTokenListeners(auth_data);
}

void IdTokenRefreshThread::WakeThread() { wakeup_sem_.Post(); }

IdTokenRefreshThread::IdTokenRefreshThread()
    : ref_count_(0), is_shutting_down_(false), wakeup_sem_(0) {}

// Called once, at startup.
// Should only be used by the Auth object, on construction.
void IdTokenRefreshThread::Initialize(AuthData* auth_data) {
  MutexLock lock(ref_count_mutex_);
  set_is_shutting_down(false);
  auth = auth_data->auth;
  IdTokenListener* listener = &token_refresh_listener_;
  auth->AddIdTokenListener(listener);
  ref_count_ = 0;

  thread_ = firebase::Thread(
      [](IdTokenRefreshThread* refresh_thread) {
        Auth* auth = refresh_thread->auth;
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

              // (We don't actually care about the results of the token request.
              // The token listener will handle that.)
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

              // If the timed-wait returns true, then it means we were
              // interrupted early - either it's time to shut down, or we
              // got a new token and should restart the clock.
              if (!refresh_thread->wakeup_sem_.TimedWait(
                      kMsPerTokenRefresh - ms_since_last_refresh)) {
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
  ref_count_--;
}

}  // namespace auth
}  // namespace firebase
