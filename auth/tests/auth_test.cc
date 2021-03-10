/*
 * Copyright 2017 Google LLC
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

#include <string.h>

#if defined(FIREBASE_ANDROID_FOR_DESKTOP)
#define __ANDROID__
#include <jni.h>
#include "testing/run_all_tests.h"
#endif  // defined(FIREBASE_ANDROID_FOR_DESKTOP)

#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/internal/platform.h"
#include "app/tests/include/firebase/app_for_testing.h"
#include "auth/src/include/firebase/auth.h"

#if defined(FIREBASE_ANDROID_FOR_DESKTOP)
#undef __ANDROID__
#endif  // defined(FIREBASE_ANDROID_FOR_DESKTOP)

#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "testing/config.h"
#include "testing/ticker.h"

#if defined(FIREBASE_WAIT_ASYNC_IN_TEST)
#include "app/rest/transport_builder.h"
#include "app/rest/transport_mock.h"
#endif  // defined(FIREBASE_WAIT_ASYNC_IN_TEST)

namespace firebase {
namespace auth {

namespace {

// Wait for the Future completed when necessary. We do not do so for Android nor
// iOS since their test is based on Ticker-based fake. We do not do so for
// desktop stub since its Future completes immediately.
template <typename T>
inline void MaybeWaitForFuture(const Future<T>& future) {
// Desktop developer sdk has a small delay due to async calls.
#if defined(FIREBASE_WAIT_ASYNC_IN_TEST)
  // Once REST implementation is in, we should be able to check this. Almost
  // always the return of last-result is ahead of the future completion. But
  // right now, the return of last-result actually happens after future is
  // completed.
  // EXPECT_EQ(firebase::kFutureStatusPending, future.status());
  while (firebase::kFutureStatusPending == future.status()) {}
#endif  // defined(FIREBASE_WAIT_ASYNC_IN_TEST)
}

// Helper functions to verify the auth future result.
template <typename T>
void Verify(const AuthError error, const Future<T>& result,
                   bool check_result_not_null) {
// Desktop stub returns result immediately and thus we skip the ticker elapse.
#if defined(FIREBASE_ANDROID_FOR_DESKTOP) || FIREBASE_PLATFORM_IOS
  EXPECT_EQ(firebase::kFutureStatusPending, result.status());
  firebase::testing::cppsdk::TickerElapse();
#endif  // defined(FIREBASE_ANDROID_FOR_DESKTOP) || FIREBASE_PLATFORM_IOS
  MaybeWaitForFuture(result);
  EXPECT_EQ(firebase::kFutureStatusComplete, result.status());
  EXPECT_EQ(error, result.error());
  if (check_result_not_null) {
    EXPECT_NE(nullptr, result.result());
  }
}

template <typename T>
void Verify(const AuthError error, const Future<T>& result) {
  Verify(error, result, true /* check_result_not_null */);
}

template <>
void Verify(const AuthError error, const Future<void>& result) {
  Verify(error, result, false /* check_result_not_null */);
}

}  // anonymous namespace

class AuthTest : public ::testing::Test {
 protected:
  void SetUp() override {
#if defined(FIREBASE_WAIT_ASYNC_IN_TEST)
    rest::SetTransportBuilder([]() -> flatbuffers::unique_ptr<rest::Transport> {
      return flatbuffers::unique_ptr<rest::Transport>(
          new rest::TransportMock());
    });
#endif  // defined(FIREBASE_WAIT_ASYNC_IN_TEST)

    firebase::testing::cppsdk::TickerReset();
    firebase::testing::cppsdk::ConfigSet("{}");
  }

  void TearDown() override {
    delete firebase_auth_;
    firebase_auth_ = nullptr;
    delete firebase_app_;
    firebase_app_ = nullptr;
    // cppsdk needs to be the last thing torn down, because the mocks are still
    // needed for parts of the firebase destructors.
    firebase::testing::cppsdk::ConfigReset();
  }

  // Helper function for those test case that needs an Auth but not care on the
  // creation of that.
  void MakeAuth() {
    firebase_app_ = testing::CreateApp();
    firebase_auth_ = Auth::GetAuth(firebase_app_);
  }

  App* firebase_app_ = nullptr;
  Auth* firebase_auth_ = nullptr;
};

TEST_F(AuthTest, TestAuthCreation) {
  // This test verifies the creation of an Auth object.
  App* firebase_app = testing::CreateApp();
  EXPECT_NE(nullptr, firebase_app);

  Auth* firebase_auth = Auth::GetAuth(firebase_app);
  EXPECT_NE(nullptr, firebase_auth);

  // Calling again does not create a new Auth object.
  Auth* firebase_auth_again = Auth::GetAuth(firebase_app);
  EXPECT_EQ(firebase_auth, firebase_auth_again);

  delete firebase_auth;
  delete firebase_app;
}

// Creates and destroys multiple auth objects to ensure destruction doesn't
// result in data races due to callbacks from the Java layer.
TEST_F(AuthTest, TestAuthCreateDestroy) {
  static int kTestIterations = 100;
  // Pipeline of app and auth objects that are all active at once.
  struct {
    App *app;
    Auth *auth;
  } created_queue[10];
  memset(created_queue, 0, sizeof(created_queue));
  size_t created_queue_items = sizeof(created_queue) / sizeof(created_queue[0]);

  // Create and destroy app and auth objects keeping up to created_queue_items
  // alive at a time.
  for (size_t i = 0; i < kTestIterations; ++i) {
    auto* queue_entry = &created_queue[i % created_queue_items];
    delete queue_entry->auth;
    delete queue_entry->app;
    queue_entry->app =
        testing::CreateApp(testing::MockAppOptions(),
                           (std::string("app") + std::to_string(i)).c_str());
    queue_entry->auth = Auth::GetAuth(queue_entry->app);
    EXPECT_NE(nullptr, queue_entry->auth);
  }

  // Clean up the queue.
  for (size_t i = 0; i < created_queue_items; ++i) {
    auto* queue_entry = &created_queue[i % created_queue_items];
    delete queue_entry->auth;
    queue_entry->auth = nullptr;
    delete queue_entry->app;
    queue_entry->app = nullptr;
  }
}

#if defined(FIREBASE_ANDROID_FOR_DESKTOP)
TEST_F(AuthTest, TestAuthCreationWithNoGooglePlay) {
  // This test is specific to Android platform. Without Google Play, we cannot
  // create an Auth object.
  firebase::testing::cppsdk::ConfigSet(
      "{"
      "  config:["
      "    {fake:'GoogleApiAvailability.isGooglePlayServicesAvailable',"
      "     futureint:{value:1}}"
      "  ]"
      "}");
  App* firebase_app = testing::CreateApp();
  EXPECT_NE(nullptr, firebase_app);

  Auth* firebase_auth = Auth::GetAuth(firebase_app);
  EXPECT_EQ(nullptr, firebase_auth);

  delete firebase_app;
}
#endif  // defined(FIREBASE_ANDROID_FOR_DESKTOP)

// Below are tests for testing different login methods and in different status.

TEST_F(AuthTest, TestSignInWithCustomTokenSucceeded) {
  firebase::testing::cppsdk::ConfigSet(
      "{"
      "  config:["
      "    {fake:'FirebaseAuth.signInWithCustomToken',"
      "     futuregeneric:{ticker:1}},"
      "    {fake:'FIRAuth.signInWithCustomToken:completion:',"
      "     futuregeneric:{ticker:1}},"
      "    {fake:'https://www.googleapis.com/identitytoolkit/v3/relyingparty/"
      "verifyCustomToken?key=not_a_real_api_key',"
      "     httpresponse: {"
      "       header: ['HTTP/1.1 200 Ok','Server:mock server 101'],"
      "     }"
      "    },"
      "    {fake:'https://www.googleapis.com/identitytoolkit/v3/relyingparty/"
      "getAccountInfo?key=not_a_real_api_key',"
      "     httpresponse: {"
      "       header: ['HTTP/1.1 200 Ok','Server:mock server 101'],"
      "       body: ['{\"users\": [{},]}',]"
      "     }"
      "    }"
      "  ]"
      "}");
  MakeAuth();
  Future<User*> result = firebase_auth_->SignInWithCustomToken("its-a-token");
  Verify(kAuthErrorNone, result);
}

TEST_F(AuthTest, TestSignInWithCredentialSucceeded) {
  firebase::testing::cppsdk::ConfigSet(
      "{"
      "  config:["
      "    {fake:'FirebaseAuth.signInWithCredential',"
      "     futuregeneric:{ticker:1}},"
      "    {fake:'FIRAuth.signInWithCredential:completion:',"
      "     futuregeneric:{ticker:1}},"
      "    {fake:'https://www.googleapis.com/identitytoolkit/v3/relyingparty/"
      "verifyPassword?key=not_a_real_api_key',"
      "     httpresponse: {"
      "       header: ['HTTP/1.1 200 Ok','Server:mock server 101'],"
      "     }"
      "    },"
      "    {fake:'https://www.googleapis.com/identitytoolkit/v3/relyingparty/"
      "getAccountInfo?key=not_a_real_api_key',"
      "     httpresponse: {"
      "       header: ['HTTP/1.1 200 Ok','Server:mock server 101'],"
      "       body: ['{\"users\": [{},]}',]"
      "     }"
      "    }"
      "  ]"
      "}");
  MakeAuth();
  Credential credential = EmailAuthProvider::GetCredential("abc@g.com", "abc");
  Future<User*> result = firebase_auth_->SignInWithCredential(credential);
  Verify(kAuthErrorNone, result);
}

TEST_F(AuthTest, TestSignInAnonymouslySucceeded) {
  firebase::testing::cppsdk::ConfigSet(
      "{"
      "  config:["
      "    {fake:'FirebaseAuth.signInAnonymously',"
      "     futuregeneric:{ticker:1}},"
      "    {fake:'FIRAuth.signInAnonymouslyWithCompletion:',"
      "     futuregeneric:{ticker:1}},"
      "    {fake:'https://www.googleapis.com/identitytoolkit/v3/relyingparty/"
      "signupNewUser?key=not_a_real_api_key',"
      "     httpresponse: {"
      "       header: ['HTTP/1.1 200 Ok','Server:mock server 101'],"
      "     }"
      "    },"
      "    {fake:'https://www.googleapis.com/identitytoolkit/v3/relyingparty/"
      "getAccountInfo?key=not_a_real_api_key',"
      "     httpresponse: {"
      "       header: ['HTTP/1.1 200 Ok','Server:mock server 101'],"
      "       body: ['{\"users\": [{},]}',]"
      "     }"
      "    }"
      "  ]"
      "}");
  MakeAuth();
  Future<User*> result = firebase_auth_->SignInAnonymously();
  Verify(kAuthErrorNone, result);
}

TEST_F(AuthTest, TestSignInWithEmailAndPasswordSucceeded) {
  firebase::testing::cppsdk::ConfigSet(
      "{"
      "  config:["
      "    {fake:'FirebaseAuth.signInWithEmailAndPassword',"
      "     futuregeneric:{ticker:1}},"
      "    {fake:'FIRAuth.signInWithEmail:password:completion:',"
      "     futuregeneric:{ticker:1}},"
      "    {fake:'https://www.googleapis.com/identitytoolkit/v3/relyingparty/"
      "verifyPassword?key=not_a_real_api_key',"
      "     httpresponse: {"
      "       header: ['HTTP/1.1 200 Ok','Server:mock server 101'],"
      "     }"
      "    },"
      "    {fake:'https://www.googleapis.com/identitytoolkit/v3/relyingparty/"
      "getAccountInfo?key=not_a_real_api_key',"
      "     httpresponse: {"
      "       header: ['HTTP/1.1 200 Ok','Server:mock server 101'],"
      "       body: ['{\"users\": [{},]}',]"
      "     }"
      "    }"
      "  ]"
      "}");
  MakeAuth();
  Future<User*> result =
      firebase_auth_->SignInWithEmailAndPassword("abc@xyz.com", "password");
  Verify(kAuthErrorNone, result);
}

TEST_F(AuthTest, TestCreateUserWithEmailAndPasswordSucceeded) {
  firebase::testing::cppsdk::ConfigSet(
      "{"
      "  config:["
      "    {fake:'FirebaseAuth.createUserWithEmailAndPassword',"
      "     futuregeneric:{ticker:1}},"
      "    {fake:'FIRAuth.createUserWithEmail:password:completion:',"
      "     futuregeneric:{ticker:1}},"
      "    {fake:'https://www.googleapis.com/identitytoolkit/v3/relyingparty/"
      "signupNewUser?key=not_a_real_api_key',"
      "     httpresponse: {"
      "       header: ['HTTP/1.1 200 Ok','Server:mock server 101'],"
      "     }"
      "    },"
      "    {fake:'https://www.googleapis.com/identitytoolkit/v3/relyingparty/"
      "getAccountInfo?key=not_a_real_api_key',"
      "     httpresponse: {"
      "       header: ['HTTP/1.1 200 Ok','Server:mock server 101'],"
      "       body: ['{\"users\": [{},]}',]"
      "     }"
      "    }"
      "  ]"
      "}");
  MakeAuth();
  Future<User*> result =
      firebase_auth_->CreateUserWithEmailAndPassword("abc@xyz.com", "password");
  Verify(kAuthErrorNone, result);
}

// Right now the desktop stub always succeeded. We could potentially test it by
// adding a desktop fake, which does not provide much value for the specific
// case of Auth since the C++ code is only a thin wraper.
#if defined(FIREBASE_ANDROID_FOR_DESKTOP) || FIREBASE_PLATFORM_IOS

TEST_F(AuthTest, TestSignInWithCustomTokenFailed) {
  firebase::testing::cppsdk::ConfigSet(
      "{"
      "  config:["
      "    {fake:'FirebaseAuth.signInWithCustomToken',"
      "     futuregeneric:{throwexception:true,"
      "                    "
      "exceptionmsg:'[FirebaseAuthInvalidCredentialsException:ERROR_INVALID_"
      "CUSTOM_TOKEN] sign-in with custom token failed',"
      "                    ticker:1}},"
      "    {fake:'FIRAuth.signInWithCustomToken:completion:',"
      "     futuregeneric:{throwexception:true,"
      "                    "
      "exceptionmsg:'[FirebaseAuthInvalidCredentialsException:ERROR_INVALID_"
      "CUSTOM_TOKEN] sign-in with custom token failed',"
      "                    ticker:1}}"
      "  ]"
      "}");
  MakeAuth();
  Future<User*> result = firebase_auth_->SignInWithCustomToken("its-a-token");
  Verify(kAuthErrorInvalidCustomToken, result);
}

TEST_F(AuthTest, TestSignInWithCredentialFailed) {
  firebase::testing::cppsdk::ConfigSet(
      "{"
      "  config:["
      "    {fake:'FirebaseAuth.signInWithCredential',"
      "     futuregeneric:{throwexception:true,"
      "                    "
      "exceptionmsg:'[FirebaseAuthInvalidCredentialsException:ERROR_INVALID_"
      "EMAIL] sign-in with credential failed',"
      "                    ticker:1}},"
      "    {fake:'FIRAuth.signInWithCredential:completion:',"
      "     futuregeneric:{throwexception:true,"
      "                    "
      "exceptionmsg:'[FirebaseAuthInvalidCredentialsException:ERROR_INVALID_"
      "EMAIL] sign-in with credential failed',"
      "                    ticker:1}}"
      "  ]"
      "}");
  MakeAuth();
  Credential credential = EmailAuthProvider::GetCredential("abc@g.com", "abc");
  Future<User*> result = firebase_auth_->SignInWithCredential(credential);
  Verify(kAuthErrorInvalidEmail, result);
}

TEST_F(AuthTest, TestSignInAnonymouslyFailed) {
  firebase::testing::cppsdk::ConfigSet(
      "{"
      "  config:["
      "    {fake:'FirebaseAuth.signInAnonymously',"
      "     futuregeneric:{throwexception:true,"
      "                    "
      "exceptionmsg:'[FirebaseAuthException:ERROR_OPERATION_NOT_ALLOWED] "
      "sign-in anonymously failed',"
      "                    ticker:1}},"
      "    {fake:'FIRAuth.signInAnonymouslyWithCompletion:',"
      "     futuregeneric:{throwexception:true,"
      "                    "
      "exceptionmsg:'[FirebaseAuthException:ERROR_OPERATION_NOT_ALLOWED] "
      "sign-in anonymously failed',"
      "                    ticker:1}}"
      "  ]"
      "}");
  MakeAuth();
  Future<User*> result = firebase_auth_->SignInAnonymously();
  Verify(kAuthErrorOperationNotAllowed, result);
}

TEST_F(AuthTest, TestSignInWithEmailAndPasswordFailed) {
  firebase::testing::cppsdk::ConfigSet(
      "{"
      "  config:["
      "    {fake:'FirebaseAuth.signInWithEmailAndPassword',"
      "     futuregeneric:{throwexception:true,"
      "                    "
      "exceptionmsg:'[FirebaseAuthInvalidCredentialsException:ERROR_WRONG_"
      "PASSWORD] sign-in with email/password failed',"
      "                    ticker:1}},"
      "    {fake:'FIRAuth.signInWithEmail:password:completion:',"
      "     futuregeneric:{throwexception:true,"
      "                    "
      "exceptionmsg:'[FirebaseAuthInvalidCredentialsException:ERROR_WRONG_"
      "PASSWORD] sign-in with email/password failed',"
      "                    ticker:1}}"
      "  ]"
      "}");
  MakeAuth();
  Future<User*> result =
      firebase_auth_->SignInWithEmailAndPassword("abc@xyz.com", "password");
  Verify(kAuthErrorWrongPassword, result);
}

TEST_F(AuthTest, TestCreateUserWithEmailAndPasswordFailed) {
  firebase::testing::cppsdk::ConfigSet(
      "{"
      "  config:["
      "    {fake:'FirebaseAuth.createUserWithEmailAndPassword',"
      "     futuregeneric:{throwexception:true,"
      "                    "
      "exceptionmsg:'[FirebaseAuthUserCollisionException:ERROR_EMAIL_ALREADY_"
      "IN_USE] create user with email/pwd failed',"
      "                    ticker:1}},"
      "    {fake:'FIRAuth.createUserWithEmail:password:completion:',"
      "     futuregeneric:{throwexception:true,"
      "                    "
      "exceptionmsg:'[FirebaseAuthUserCollisionException:ERROR_EMAIL_ALREADY_"
      "IN_USE] create user with email/pwd failed',"
      "                    ticker:1}}"
      "  ]"
      "}");
  MakeAuth();
  Future<User*> result =
      firebase_auth_->CreateUserWithEmailAndPassword("abc@xyz.com", "password");
  Verify(kAuthErrorEmailAlreadyInUse, result);
}

#endif  // defined(FIREBASE_ANDROID_FOR_DESKTOP) || FIREBASE_PLATFORM_IOS

TEST_F(AuthTest, TestCurrentUserAndSignOut) {
  // Here we let mock sign-in-anonymously succeed immediately (ticker = 0).
  firebase::testing::cppsdk::ConfigSet(
      "{"
      "  config:["
      "    {fake:'FirebaseAuth.signInAnonymously',"
      "     futuregeneric:{ticker:0}},"
      "    {fake:'FIRAuth.FIRAuth.signInAnonymouslyWithCompletion:',"
      "     futuregeneric:{ticker:0}},"
      "    {fake:'https://www.googleapis.com/identitytoolkit/v3/relyingparty/"
      "signupNewUser?key=not_a_real_api_key',"
      "     httpresponse: {"
      "       header: ['HTTP/1.1 200 Ok','Server:mock server 101'],"
      "     }"
      "    },"
      "    {fake:'https://www.googleapis.com/identitytoolkit/v3/relyingparty/"
      "getAccountInfo?key=not_a_real_api_key',"
      "     httpresponse: {"
      "       header: ['HTTP/1.1 200 Ok','Server:mock server 101'],"
      "       body: ['{\"users\": [{},]}',]"
      "     }"
      "    }"
      "  ]"
      "}");
  MakeAuth();

  // No user is signed in.
  EXPECT_EQ(nullptr, firebase_auth_->current_user());

  // Now sign-in, say anonymously.
  Future<User*> result = firebase_auth_->SignInAnonymously();
  MaybeWaitForFuture(result);
  EXPECT_NE(nullptr, firebase_auth_->current_user());

  // Now sign-out.
  firebase_auth_->SignOut();
  EXPECT_EQ(nullptr, firebase_auth_->current_user());
}

TEST_F(AuthTest, TestSendPasswordResetEmailSucceeded) {
  firebase::testing::cppsdk::ConfigSet(
      "{"
      "  config:["
      "    {fake:'FirebaseAuth.sendPasswordResetEmail',"
      "     futuregeneric:{ticker:1}},"
      "    {fake:'FIRAuth.sendPasswordResetWithEmail:completion:',"
      "     futuregeneric:{ticker:1}},"
      "    {fake:'https://www.googleapis.com/identitytoolkit/v3/relyingparty/"
      "getOobConfirmationCode?key=not_a_real_api_key',"
      "     httpresponse: {"
      "       header: ['HTTP/1.1 200 Ok','Server:mock server 101'],"
      "       body: ['{\"email\": \"my@email.com\"}']"
      "     }"
      "    }"
      "  ]"
      "}");
  MakeAuth();
  Future<void> result = firebase_auth_->SendPasswordResetEmail("my@email.com");
  Verify(kAuthErrorNone, result);
}

#if defined(FIREBASE_ANDROID_FOR_DESKTOP) || FIREBASE_PLATFORM_IOS
TEST_F(AuthTest, TestSendPasswordResetEmailFailed) {
  firebase::testing::cppsdk::ConfigSet(
      "{"
      "  config:["
      "    {fake:'FirebaseAuth.sendPasswordResetEmail',"
      "     futuregeneric:{throwexception:true,"
      "                    "
      "exceptionmsg:'[FirebaseAuthEmailException:ERROR_INVALID_MESSAGE_PAYLOAD]"
      " failed to send password reset email',"
      "                    ticker:1}},"
      "    {fake:'FIRAuth.sendPasswordResetWithEmail:completion:',"
      "     futuregeneric:{throwexception:true,"
      "                    "
      "exceptionmsg:'[FirebaseAuthEmailException:ERROR_INVALID_MESSAGE_PAYLOAD]"
      " failed to send password reset email',"
      "                    ticker:1}}"
      "  ]"
      "}");
  MakeAuth();
  Future<void> result = firebase_auth_->SendPasswordResetEmail("my@email.com");
  Verify(kAuthErrorInvalidMessagePayload, result);
}
#endif  // defined(FIREBASE_ANDROID_FOR_DESKTOP) || FIREBASE_PLATFORM_IOS

}  // namespace auth
}  // namespace firebase
