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

#ifndef FIREBASE_AUTH_CLIENT_CPP_TESTS_DESKTOP_TEST_UTILS_H_
#define FIREBASE_AUTH_CLIENT_CPP_TESTS_DESKTOP_TEST_UTILS_H_

#include "app/src/include/firebase/future.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "auth/src/desktop/auth_desktop.h"
#include "auth/src/include/firebase/auth.h"
#include "auth/src/include/firebase/auth/types.h"

namespace firebase {
namespace auth {
namespace test {

namespace detail {
// Base class to test how many times a listener was called.
// Register one of the implementations below with the Auth class
// (IdToken/AuthStateChangesCounter), then call ExpectChanges(number) on it. By
// default, the check will be done in the destructor, but you can call
// VerifyAndReset to force the check while the test is still running, which is
// useful if the test involves several sign in operations.
class ListenerChangeCounter {
 public:
  ListenerChangeCounter();
  virtual ~ListenerChangeCounter();

  void ExpectChanges(int num);
  void VerifyAndReset();

 protected:
  int actual_changes_;

 private:
  void Verify();

  int expected_changes_;
};
}  // namespace detail

inline FederatedAuthProvider::AuthenticatedUserData
GetFakeAuthenticatedUserData() {
  FederatedAuthProvider::AuthenticatedUserData user_data;
  user_data.uid = "localid123";
  user_data.email = "testsignin@example.com";
  user_data.display_name = "";
  user_data.photo_url = "";
  user_data.provider_id = "Firebase";
  user_data.is_email_verified = false;
  user_data.raw_user_info["login"] = Variant("test_login@example.com");
  user_data.raw_user_info["screen_name"] = Variant("test_screen_name");
  user_data.access_token = "12345ABC";
  user_data.refresh_token = "67890DEF";
  user_data.token_expires_in_seconds = 60;
  return user_data;
}

inline void VerifySignInResult(const Future<SignInResult>& future,
                               AuthError auth_error,
                               const char* error_message) {
  EXPECT_EQ(future.status(), kFutureStatusComplete);
  EXPECT_EQ(future.error(), auth_error);
  if (error_message != nullptr) {
    EXPECT_STREQ(future.error_message(), error_message);
  }
}

inline void VerifySignInResult(const Future<SignInResult>& future,
                               AuthError auth_error) {
  VerifySignInResult(future, auth_error,
                     /*error_message=*/nullptr);
  EXPECT_EQ(future.error(), auth_error);
}

inline FederatedOAuthProviderData GetFakeOAuthProviderData() {
  FederatedOAuthProviderData provider_data;
  provider_data.provider_id =
      firebase::auth::GitHubAuthProvider::kProviderId;
  provider_data.scopes = {"read:user", "user:email"};
  provider_data.custom_parameters = {{"req_id", "1234"}};
  return provider_data;
}

// OAuthProviderHandler to orchestrate Auth::SignInWithProvider,
// User::LinkWithProvider and User::ReauthenticateWithProver tests.  Provides
// a mechanism to test the callback surface of the FederatedAuthProvider.
// Additionally the class provides option checks (extra_integrity_checks) to
// ensure the validity of the data that the Auth implementation passes
// to the handler, such as a non-null auth completion handle.
class OAuthProviderTestHandler
    : public FederatedAuthProvider::Handler<FederatedOAuthProviderData> {
 public:
  explicit OAuthProviderTestHandler(bool extra_integrity_checks = false) {
    extra_integrity_checks_ = extra_integrity_checks;
    authenticated_user_data_ = GetFakeAuthenticatedUserData();
    sign_in_auth_completion_handle_ = nullptr;
    link_auth_completion_handle_ = nullptr;
    reauthenticate_auth_completion_handle_ = nullptr;
  }

  explicit OAuthProviderTestHandler(
      const FederatedAuthProvider::AuthenticatedUserData&
          authenticated_user_data,
      bool extra_integrity_checks = false) {
    extra_integrity_checks_ = extra_integrity_checks;
    authenticated_user_data_ = authenticated_user_data;
    sign_in_auth_completion_handle_ = nullptr;
  }

  void SetAuthenticatedUserData(
      const FederatedAuthProvider::AuthenticatedUserData& user_data) {
    authenticated_user_data_ = user_data;
  }

  FederatedAuthProvider::AuthenticatedUserData* GetAuthenticatedUserData() {
    return &authenticated_user_data_;
  }

  // Caches the auth_completion_handler, which will be invoked via
  // the test framework's inovcation of the TriggerSignInComplete method.
  void OnSignIn(const FederatedOAuthProviderData& provider_data,
                        AuthCompletionHandle* completion_handle) override {
    // ensure we're not invoking this handler twice, thereby overwritting the
    // sign_in_auth_completion_handle_
    assert(sign_in_auth_completion_handle_ == nullptr);
    sign_in_auth_completion_handle_ = completion_handle;
    PerformIntegrityChecks(provider_data, completion_handle);
  }

  // Invokes SignInComplete with the auth completion handler provided to this
  // during the Auth::SignInWithProvider flow. The ability to trigger this from
  // the test framework, instead of immediately from OnSignIn, provides
  // mechanisms to test multiple on-going authentication/sign-in requests on
  // the Auth object.
  void TriggerSignInComplete() {
    assert(sign_in_auth_completion_handle_);
    SignInComplete(sign_in_auth_completion_handle_, authenticated_user_data_,
                   /*auth_error=*/kAuthErrorNone, "");
  }

  // Invokes SignInComplete with specific auth error codes and error messages.
  void TriggerSignInCompleteWithError(AuthError auth_error,
                                      const char* error_message) {
    assert(sign_in_auth_completion_handle_);
    SignInComplete(sign_in_auth_completion_handle_, authenticated_user_data_,
                   auth_error, error_message);
  }

  // Caches the auth_completion_handler, which will be invoked via
  // the test framework's inovcation of the TriggerLinkComplete method.
  void OnLink(const FederatedOAuthProviderData& provider_data,
                      AuthCompletionHandle* completion_handle) override {
    assert(link_auth_completion_handle_ == nullptr);
    link_auth_completion_handle_ = completion_handle;
    PerformIntegrityChecks(provider_data, completion_handle);
  }

  // Invokes LinkComplete with the auth completion handler provided to this
  // during the User::LinkWithProvider flow. The ability to trigger this from
  // the test framework, instead of immediately from OnLink, provides
  // mechanisms to test multiple on-going authentication/link requests on
  // the User object.
  void TriggerLinkComplete() {
    assert(link_auth_completion_handle_);
    LinkComplete(link_auth_completion_handle_, authenticated_user_data_,
                 /*auth_error=*/kAuthErrorNone, "");
  }

  // Invokes Link Complete with a specific auth error code and error message
  void TriggerLinkCompleteWithError(AuthError auth_error,
                                    const char* error_message) {
    assert(link_auth_completion_handle_);
    LinkComplete(link_auth_completion_handle_, authenticated_user_data_,
                   auth_error, error_message);
  }

  // Caches the auth_completion_handler, which will be invoked via
  // the test framework's inovcation of the TriggerReauthenticateComplete
  // method.
  void OnReauthenticate(const FederatedOAuthProviderData& provider_data,
                        AuthCompletionHandle* completion_handle) override {
    assert(reauthenticate_auth_completion_handle_ == nullptr);
    reauthenticate_auth_completion_handle_ = completion_handle;
    PerformIntegrityChecks(provider_data, completion_handle);
  }

  // Invokes ReauthenticateComplete with the auth completion handler provided to
  // this during the User::ReauthenticateWithProvider flow. The ability to
  // trigger this from the test framework, instead of immediately from
  // OnReauthneticate, provides mechanisms to test multiple on-going
  // re-authentication requests on the User object.
  void TriggerReauthenticateComplete() {
    assert(reauthenticate_auth_completion_handle_);
    ReauthenticateComplete(reauthenticate_auth_completion_handle_,
                           authenticated_user_data_,
                           /*auth_error=*/kAuthErrorNone, "");
  }

  // Invokes ReauthenticateComplete with a specific auth error code and error
  // message
  void TriggerReauthenticateCompleteWithError(AuthError auth_error,
                                              const char* error_message) {
    assert(reauthenticate_auth_completion_handle_);
    ReauthenticateComplete(reauthenticate_auth_completion_handle_,
                           authenticated_user_data_, auth_error, error_message);
  }

 private:
  void PerformIntegrityChecks(const FederatedOAuthProviderData& provider_data,
                              const AuthCompletionHandle* completion_handle) {
    if (extra_integrity_checks_) {
      // check the auth_completion_handle the implementation provided.
      // note that the auth completion handle is an opaque type for our users,
      // and normal applications wouldn't get a chance to do these sorts of
      // checks.
      EXPECT_NE(completion_handle, nullptr);

      // ensure that the auth data object has been configured in the handle.
      assert(completion_handle->auth_data);
      EXPECT_EQ(completion_handle->auth_data->future_impl.GetFutureStatus(
                    completion_handle->future_handle.get()),
                kFutureStatusPending);
      FederatedOAuthProviderData expected_provider_data =
          GetFakeOAuthProviderData();
      EXPECT_EQ(provider_data.provider_id, expected_provider_data.provider_id);
      EXPECT_EQ(provider_data.scopes, expected_provider_data.scopes);
      EXPECT_EQ(provider_data.custom_parameters,
                expected_provider_data.custom_parameters);
    }
  }

  AuthCompletionHandle* sign_in_auth_completion_handle_;
  AuthCompletionHandle* link_auth_completion_handle_;
  AuthCompletionHandle* reauthenticate_auth_completion_handle_;
  FederatedAuthProvider::AuthenticatedUserData authenticated_user_data_;
  bool extra_integrity_checks_;
};

class IdTokenChangesCounter : public detail::ListenerChangeCounter,
                              public IdTokenListener {
 public:
  void OnIdTokenChanged(Auth* /*unused*/) override;
};

class AuthStateChangesCounter : public detail::ListenerChangeCounter,
                                public AuthStateListener {
 public:
  void OnAuthStateChanged(Auth* /*unused*/) override;
};

// Waits until the given future is complete and asserts that it completed with
// the given error (no error by default). Returns the future's result.
template <typename T>
T WaitForFuture(const firebase::Future<T>& future,
                const firebase::auth::AuthError expected_error =
                    firebase::auth::kAuthErrorNone) {
  while (future.status() == firebase::kFutureStatusPending) {
  }
  // This is wrapped in a lambda to work around the assertion macro expecting
  // the function to return void.
  [&] {
    ASSERT_EQ(firebase::kFutureStatusComplete, future.status());
    EXPECT_EQ(expected_error, future.error());
    if (expected_error != kAuthErrorNone) {
      EXPECT_THAT(future.error_message(), ::testing::NotNull());
      EXPECT_THAT(future.error_message(), ::testing::StrNe(""));
    }
  }();
  return *future.result();
}

// Waits until the given future is complete and asserts that it completed with
// the given error (no error by default).
void WaitForFuture(
    const firebase::Future<void>& future,
    firebase::auth::AuthError expected_error = firebase::auth::kAuthErrorNone);

}  // namespace test
}  // namespace auth
}  // namespace firebase

#endif  // FIREBASE_AUTH_CLIENT_CPP_TESTS_DESKTOP_TEST_UTILS_H_
