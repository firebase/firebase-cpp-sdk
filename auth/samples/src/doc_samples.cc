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

// WARNING: Code from this file is included verbatim in the Auth C++
//          documentation. Only change existing code if it is safe to release
//          to the public. Otherwise, a tech writer may make an unrelated
//          modification, regenerate the docs, and unwittingly release an
//          unannounced modification to the public.

// [START auth_includes]
#include "firebase/app.h"
#include "firebase/auth.h"
// [END auth_includes]

// Stub functions to allow sample functions to compile.
void Wait(int /*time*/) {}
void ShowTextBox(const char* /*message*/, ...) {}
bool ShowTextButton(const char* /*message*/, ...) { return false; }
std::string ShowInputBox(const char* /*message*/, ...) { return ""; }
void ShowImage(const char* /*image_file_name*/) {}
struct Mutex {};
struct MutexLock {
  explicit MutexLock(const Mutex& /*mutex*/) {}
};

// Stub values to allow sample functions to compile.
struct MyProgramContext {
  char* display_name;
};
#if defined(__ANDROID__)
JNIEnv* my_jni_env = nullptr;
jobject my_activity = nullptr;
#endif  // defined(__ANDROID__)
const char* apple_id_token = nullptr;
const char* raw_nonce = nullptr;
const char* email = nullptr;
const char* password = nullptr;
const char* google_id_token = nullptr;
const char* access_token = nullptr;
const char* server_auth_code = nullptr;
const char* custom_token = nullptr;
const char* token = nullptr;
const char* secret = nullptr;
MyProgramContext my_program_context;

firebase::App* AppCreate() {
// [START app_create]
#if defined(__ANDROID__)
  firebase::App* app =
      firebase::App::Create(firebase::AppOptions(), my_jni_env, my_activity);
#else
  firebase::App* app = firebase::App::Create(firebase::AppOptions());
#endif  // defined(__ANDROID__)
  // [END app_create]
  return app;
}

firebase::auth::Auth* AuthFromApp(firebase::App* app) {
  // [START auth_from_app]
  firebase::auth::Auth* auth = firebase::auth::Auth::GetAuth(app);
  // [END auth_from_app]
  return auth;
}

void VariousCredentials(firebase::auth::Auth* auth) {
  {
    // [START auth_credential_apple]
    firebase::auth::Credential credential =
        firebase::auth::OAuthProvider::GetCredential(
            "apple.com", apple_id_token, raw_nonce, nullptr);
    // [END auth_credential_apple]
    (void)credential;
  }
  {
    // [START auth_credential_email]
    firebase::auth::Credential credential =
        firebase::auth::EmailAuthProvider::GetCredential(email, password);
    // [END auth_credential_email]
    (void)credential;
  }
  {
    // [START auth_credential_google]
    firebase::auth::Credential credential =
        firebase::auth::GoogleAuthProvider::GetCredential(google_id_token,
                                                          nullptr);
    // [END auth_credential_google]
    (void)credential;
  }
  {
    // [START auth_credential_play_games]
    firebase::auth::Credential credential =
        firebase::auth::PlayGamesAuthProvider::GetCredential(server_auth_code);
    // [END auth_credential_play_games]
    (void)credential;
  }
  {
    // [START auth_credential_facebook]
    firebase::auth::Credential credential =
        firebase::auth::FacebookAuthProvider::GetCredential(access_token);
    // [END auth_credential_facebook]
    (void)credential;
  }
}

void VariousSignIns(firebase::auth::Auth* auth) {
  {
    // [START auth_create_user]
    firebase::Future<firebase::auth::User*> result =
        auth->CreateUserWithEmailAndPassword(email, password);
    // [END auth_create_user]
    (void)result;
  }
  {
    // [START auth_sign_in_apple]
    firebase::auth::Credential credential =
        firebase::auth::OAuthProvider::GetCredential(
            "apple.com", apple_id_token, raw_nonce, nullptr);
    firebase::Future<firebase::auth::User*> result =
        auth->SignInWithCredential(credential);
    // [END auth_sign_in_apple]
    (void)result;
  }
  {
    // [START auth_sign_in_email]
    firebase::Future<firebase::auth::User*> result =
        auth->SignInWithEmailAndPassword(email, password);
    // [END auth_sign_in_email]
    (void)result;
  }
  {
    // [START auth_sign_in_google]
    firebase::auth::Credential credential =
        firebase::auth::GoogleAuthProvider::GetCredential(google_id_token,
                                                          nullptr);
    firebase::Future<firebase::auth::User*> result =
        auth->SignInWithCredential(credential);
    // [END auth_sign_in_google]
    (void)result;
  }
  {
    // [START auth_sign_in_play_games]
    firebase::auth::Credential credential =
        firebase::auth::PlayGamesAuthProvider::GetCredential(server_auth_code);

    firebase::Future<firebase::auth::User*> result =
        auth->SignInWithCredential(credential);
    // [END auth_sign_in_play_games]
    (void)result;
  }
  {
    // [START auth_sign_in_facebook]
    firebase::auth::Credential credential =
        firebase::auth::FacebookAuthProvider::GetCredential(access_token);
    firebase::Future<firebase::auth::User*> result =
        auth->SignInWithCredential(credential);
    // [END auth_sign_in_facebook]
    (void)result;
  }
  {
    // [START auth_sign_in_github]
    firebase::auth::Credential credential =
        firebase::auth::GitHubAuthProvider::GetCredential(token);
    firebase::Future<firebase::auth::User*> result =
        auth->SignInWithCredential(credential);
    // [END auth_sign_in_github]
    (void)result;
  }
  {
    // [START auth_sign_in_twitter]
    firebase::auth::Credential credential =
        firebase::auth::TwitterAuthProvider::GetCredential(token, secret);
    firebase::Future<firebase::auth::User*> result =
        auth->SignInWithCredential(credential);
    // [END auth_sign_in_twitter]
    (void)result;
  }
  {
    // [START auth_sign_in_custom_token]
    firebase::Future<firebase::auth::User*> result =
        auth->SignInWithCustomToken(custom_token);
    // [END auth_sign_in_custom_token]
    (void)result;
  }
  {
    // [START auth_sign_in_anonymously]
    firebase::Future<firebase::auth::User*> result = auth->SignInAnonymously();
    // [END auth_sign_in_anonymously]
    (void)result;
  }
}

void VariousSignInChecks(firebase::auth::Auth* auth) {
  {
    // [START auth_create_user_check]
    firebase::Future<firebase::auth::User*> result =
        auth->CreateUserWithEmailAndPasswordLastResult();
    if (result.status() == firebase::kFutureStatusComplete) {
      if (result.error() == firebase::auth::kAuthErrorNone) {
        firebase::auth::User* user = *result.result();
        printf("Create user succeeded for email %s\n", user->email().c_str());
      } else {
        printf("Created user failed with error '%s'\n", result.error_message());
      }
    }
    // [END auth_create_user_check]
  }
  {
    // [START auth_sign_in_email_check]
    firebase::Future<firebase::auth::User*> result =
        auth->SignInWithEmailAndPasswordLastResult();
    if (result.status() == firebase::kFutureStatusComplete) {
      if (result.error() == firebase::auth::kAuthErrorNone) {
        firebase::auth::User* user = *result.result();
        printf("Sign in succeeded for email %s\n", user->email().c_str());
      } else {
        printf("Sign in failed with error '%s'\n", result.error_message());
      }
    }
    // [END auth_sign_in_email_check]
  }
  {
    // [START auth_sign_in_credential_check]
    firebase::Future<firebase::auth::User*> result =
        auth->SignInWithCredentialLastResult();
    if (result.status() == firebase::kFutureStatusComplete) {
      if (result.error() == firebase::auth::kAuthErrorNone) {
        firebase::auth::User* user = *result.result();
        printf("Sign in succeeded for `%s`\n", user->display_name().c_str());
      } else {
        printf("Sign in failed with error '%s'\n", result.error_message());
      }
    }
    // [END auth_sign_in_credential_check]
  }
  {
    // [START auth_sign_in_custom_token_check]
    firebase::Future<firebase::auth::User*> result =
        auth->SignInWithCustomTokenLastResult();
    if (result.status() == firebase::kFutureStatusComplete) {
      if (result.error() == firebase::auth::kAuthErrorNone) {
        firebase::auth::User* user = *result.result();
        printf("Sign in succeeded for `%s`\n", user->display_name().c_str());
      } else {
        printf("Sign in failed with error '%s'\n", result.error_message());
      }
    }
    // [END auth_sign_in_custom_token_check]
  }
  {
    // [START auth_sign_in_anonymously_check]
    firebase::Future<firebase::auth::User*> result =
        auth->SignInAnonymouslyLastResult();
    if (result.status() == firebase::kFutureStatusComplete) {
      if (result.error() == firebase::auth::kAuthErrorNone) {
        firebase::auth::User* user = *result.result();
        printf("Sign in succeeded for `%s`\n", user->display_name().c_str());
      } else {
        printf("Sign in failed with error '%s'\n", result.error_message());
      }
    }
    // [END auth_sign_in_anonymously_check]
  }
}

// [START user_state_change]
class MyAuthStateListener : public firebase::auth::AuthStateListener {
 public:
  void OnAuthStateChanged(firebase::auth::Auth* auth) override {
    firebase::auth::User* user = auth->current_user();
    if (user != nullptr) {
      // User is signed in
      printf("OnAuthStateChanged: signed_in %s\n", user->uid().c_str());
    } else {
      // User is signed out
      printf("OnAuthStateChanged: signed_out\n");
    }
    // ...
  }
};
// [END user_state_change]

void VariousUserManagementChecks(firebase::auth::Auth* auth) {
  {
    // [START auth_monitor_user]
    // ... initialization code
    // Test notification on registration.
    MyAuthStateListener state_change_listener;
    auth->AddAuthStateListener(&state_change_listener);
    // [END auth_monitor_user]
  }
  {
    // [START auth_user_info_check]
    firebase::auth::User* user = auth->current_user();
    if (user != nullptr) {
      std::string name = user->display_name();
      std::string email = user->email();
      std::string photo_url = user->photo_url();
      // The user's ID, unique to the Firebase project.
      // Do NOT use this value to authenticate with your backend server,
      // if you have one. Use firebase::auth::User::Token() instead.
      std::string uid = user->uid();
    }
    // [END auth_user_info_check]
  }
  {
    // [START auth_user_profile_data_check]
    firebase::auth::User* user = auth->current_user();
    if (user != nullptr) {
      for (auto it = user->provider_data().begin();
           it != user->provider_data().end(); ++it) {
        firebase::auth::UserInfoInterface* profile = *it;
        // Id of the provider (ex: google.com)
        std::string providerId = profile->provider_id();

        // UID specific to the provider
        std::string uid = profile->uid();

        // Name, email address, and profile photo Url
        std::string name = profile->display_name();
        std::string email = profile->email();
        std::string photoUrl = profile->photo_url();
      }
    }
    // [END auth_user_profile_data_check]
  }
  {
    // [START auth_profile_edit_check]
    firebase::auth::User* user = auth->current_user();
    if (user != nullptr) {
      firebase::auth::User::UserProfile profile;
      profile.display_name = "Jane Q. User";
      profile.photo_url = "https://example.com/jane-q-user/profile.jpg";
      user->UpdateUserProfile(profile).OnCompletion(
          [](const firebase::Future<void>& completed_future, void* user_data) {
            // We are probably in a different thread right now.
            if (completed_future.error() == 0) {
              printf("User profile updated.");
            }
          },
          nullptr);  // pass user_data here.
    }
    // [END auth_profile_edit_check]
  }
  {
    // [START auth_set_email_check]
    firebase::auth::User* user = auth->current_user();
    if (user != nullptr) {
      user->UpdateEmail("user@example.com")
          .OnCompletion(
              [](const firebase::Future<void>& completed_future,
                 void* user_data) {
                // We are probably in a different thread right now.
                if (completed_future.error() == 0) {
                  printf("User email address updated.");
                }
              },
              nullptr);
    }
    // [END auth_set_email_check]
  }
  {
    // [START auth_user_verify_email_check]
    firebase::auth::User* user = auth->current_user();
    if (user != nullptr) {
      user->SendEmailVerification().OnCompletion(
          [](const firebase::Future<void>& completed_future, void* user_data) {
            // We are probably in a different thread right now.
            if (completed_future.error() == 0) {
              printf("Email sent.");
            }
          },
          nullptr);
    }
    // [END auth_user_verify_email_check]
  }
  {
    // [START auth_user_update_password_check]
    firebase::auth::User* user = auth->current_user();
    std::string newPassword = "SOME-SECURE-PASSWORD";

    if (user != nullptr) {
      user->UpdatePassword(newPassword.c_str())
          .OnCompletion(
              [](const firebase::Future<void>& completed_future,
                 void* user_data) {
                // We are probably in a different thread right now.
                if (completed_future.error() == 0) {
                  printf("password updated.");
                }
              },
              nullptr);
    }
    // [END auth_user_update_password_check]
  }
  {
    // [START auth_user_reset_pass_check]
    std::string emailAddress = "user@example.com";

    auth->SendPasswordResetEmail(emailAddress.c_str())
        .OnCompletion(
            [](const firebase::Future<void>& completed_future,
               void* user_data) {
              // We are probably in a different thread right now.
              if (completed_future.error() == 0) {
                // Email sent.
              } else {
                // An error happened.
                printf("Error %d: %s", completed_future.error(),
                       completed_future.error_message());
              }
            },
            nullptr);
    // [END auth_user_reset_pass_check]
  }
  {
    // [START auth_user_delete_check]
    firebase::auth::User* user = auth->current_user();
    if (user != nullptr) {
      user->Delete().OnCompletion(
          [](const firebase::Future<void>& completed_future, void* user_data) {
            if (completed_future.error() == 0) {
              // User deleted.
            } else {
              // An error happened.
              printf("Error %d: %s", completed_future.error(),
                     completed_future.error_message());
            }
          },
          nullptr);
    }
    // [END auth_user_delete_check]
  }
  {
    // [START auth_user_reauthenticate_check]
    firebase::auth::User* user = auth->current_user();

    // Get auth credentials from the user for re-authentication. The example
    // below shows email and password credentials but there are multiple
    // possible providers, such as GoogleAuthProvider or FacebookAuthProvider.
    firebase::auth::Credential credential =
        firebase::auth::EmailAuthProvider::GetCredential("user@example.com",
                                                         "password1234");

    if (user != nullptr) {
      user->Reauthenticate(credential)
          .OnCompletion(
              [](const firebase::Future<void>& completed_future,
                 void* user_data) {
                if (completed_future.error() == 0) {
                  printf("User re-authenticated.");
                }
              },
              nullptr);
    }
    // [END auth_user_reauthenticate_check]
  }
}

// [START future_callback]
void OnCreateCallback(const firebase::Future<firebase::auth::User*>& result,
                      void* user_data) {
  // The callback is called when the Future enters the `complete` state.
  assert(result.status() == firebase::kFutureStatusComplete);

  // Use `user_data` to pass-in program context, if you like.
  MyProgramContext* program_context = static_cast<MyProgramContext*>(user_data);

  // Important to handle both success and failure situations.
  if (result.error() == firebase::auth::kAuthErrorNone) {
    firebase::auth::User* user = *result.result();
    printf("Create user succeeded for email %s\n", user->email().c_str());

    // Perform other actions on User, if you like.
    firebase::auth::User::UserProfile profile;
    profile.display_name = program_context->display_name;
    user->UpdateUserProfile(profile);

  } else {
    printf("Created user failed with error '%s'\n", result.error_message());
  }
}

void CreateUser(firebase::auth::Auth* auth) {
  // Callbacks work the same for any firebase::Future.
  firebase::Future<firebase::auth::User*> result =
      auth->CreateUserWithEmailAndPasswordLastResult();

  // `&my_program_context` is passed verbatim to OnCreateCallback().
  result.OnCompletion(OnCreateCallback, &my_program_context);
}
// [END future_callback]

// [START future_lambda]
void CreateUserUsingLambda(firebase::auth::Auth* auth) {
  // Callbacks work the same for any firebase::Future.
  firebase::Future<firebase::auth::User*> result =
      auth->CreateUserWithEmailAndPasswordLastResult();

  // The lambda has the same signature as the callback function.
  result.OnCompletion(
      [](const firebase::Future<firebase::auth::User*>& result,
         void* user_data) {
        // `user_data` is the same as &my_program_context, below.
        // Note that we can't capture this value in the [] because std::function
        // is not supported by our minimum compiler spec (which is pre C++11).
        MyProgramContext* program_context =
            static_cast<MyProgramContext*>(user_data);

        // Process create user result...
        (void)program_context;
      },
      &my_program_context);
}
// [END future_lambda]

void LinkCredential(const firebase::auth::Credential& credential,
                    firebase::auth::Auth* auth) {
  // [START user_link]
  // Link the new credential to the currently active user.
  firebase::auth::User* current_user = auth->current_user();
  firebase::Future<firebase::auth::User*> result =
      current_user->LinkWithCredential(credential);
  // [END user_link]
}

void UnLinkCredential(const char* providerId, firebase::auth::Auth* auth) {
  // [START user_unlink]
  // Unlink the sign-in provider from the currently active user.
  firebase::auth::User* current_user = auth->current_user();
  firebase::Future<firebase::auth::User*> result =
      current_user->Unlink(providerId);
  // [END user_unlink]
}

void LinkCredentialFailAppleSignIn(const firebase::auth::Credential& credential,
                                   firebase::auth::Auth* auth) {
  // [START link_credential_apple_signin]
  firebase::Future<firebase::auth::SignInResult> link_result =
      auth->current_user()->LinkAndRetrieveDataWithCredential(credential);

  // To keep example simple, wait on the current thread until call completes.
  while (link_result.status() == firebase::kFutureStatusPending) {
    Wait(100);
  }

  // Determine the result of the link attempt
  if (link_result.error() == firebase::auth::kAuthErrorNone) {
    // user linked correctly.
  } else if (link_result.error() ==
                 firebase::auth::kAuthErrorCredentialAlreadyInUse &&
             link_result.result()->info.updated_credential.is_valid()) {
    // Sign In with the new credential
    firebase::Future<firebase::auth::User*> result = auth->SignInWithCredential(
        link_result.result()->info.updated_credential);
  } else {
    // Another link error occurred.
  }
  // [END link_credential_apple_signin]
}

void MergeCredentials(const firebase::auth::Credential& credential,
                      firebase::auth::Auth* auth) {
  // [START user_merge]
  // Gather data for the currently signed in User.
  firebase::auth::User* current_user = auth->current_user();
  std::string current_email = current_user->email();
  std::string current_provider_id = current_user->provider_id();
  std::string current_display_name = current_user->display_name();
  std::string current_photo_url = current_user->photo_url();

  // Sign in with the new credentials.
  firebase::Future<firebase::auth::User*> result =
      auth->SignInWithCredential(credential);

  // To keep example simple, wait on the current thread until call completes.
  while (result.status() == firebase::kFutureStatusPending) {
    Wait(100);
  }

  // The new User is now active.
  if (result.error() == firebase::auth::kAuthErrorNone) {
    firebase::auth::User* new_user = *result.result();

    // Merge new_user with the user in details.
    // ...
    (void)new_user;
  }
  // [END user_merge]

  (void)current_email;
  (void)current_provider_id;
  (void)current_display_name;
  (void)current_photo_url;
}

void NextSteps(firebase::auth::Auth* auth) {
  // [START next_steps]
  firebase::auth::User* user = auth->current_user();
  if (user != nullptr) {
    std::string name = user->display_name();
    std::string email = user->email();
    std::string photo_url = user->photo_url();
    // The user's ID, unique to the Firebase project.
    // Do NOT use this value to authenticate with your backend server,
    // if you have one. Use firebase::auth::User::Token() instead.
    std::string uid = user->uid();
  }
  // [END next_steps]
}

void SendIdTokenToBackend(firebase::auth::Auth* auth) {
  // [START send_id_token_to_backend]
  firebase::auth::User* user = auth->current_user();
  if (user != nullptr) {
    firebase::Future<std::string> idToken = user->GetToken(true);

    // Send token to your backend via HTTPS
    // ...
  }
  // [END send_id_token_to_backend]
}

firebase::auth::Auth* AuthOverview(firebase::App* app) {
  /// [Auth overview]
  // Get the Auth class for your App.
  firebase::auth::Auth* auth = firebase::auth::Auth::GetAuth(app);

  // Request anonymous sign-in and wait until asynchronous call completes.
  firebase::Future<firebase::auth::User*> sign_in_future =
      auth->SignInAnonymously();
  while (sign_in_future.status() == firebase::kFutureStatusPending) {
    Wait(100);
    printf("Signing in...\n");
  }

  // Print sign in results.
  const firebase::auth::AuthError error =
      static_cast<firebase::auth::AuthError>(sign_in_future.error());
  if (error != firebase::auth::kAuthErrorNone) {
    printf("Sign in failed with error `%s`\n", sign_in_future.error_message());
  } else {
    firebase::auth::User* user = *sign_in_future.result();
    printf("Signed in as %s user.\n",
           user->is_anonymous() ? "an anonymous" : "a non-anonymous");
  }
  /// [Auth overview]

  return auth;
}

/// [Providers]
// This function is called every frame to display the login screen.
// Returns the identity provider name, or "" if none selected.
const char* DisplayIdentityProviders(const char* email,
                                     firebase::auth::Auth* auth) {
  // Get results of most recently call to FetchProvidersForEmail().
  firebase::Future<firebase::auth::Auth::FetchProvidersResult> future =
      auth->FetchProvidersForEmailLastResult();
  const firebase::auth::Auth::FetchProvidersResult* result = future.result();

  // Header.
  ShowTextBox("Sign in %s", email);

  // Fetch providers from the server if we need to.
  const bool refetch =
      future.status() == firebase::kFutureStatusInvalid || result != nullptr;
  if (refetch) {
    auth->FetchProvidersForEmail(email);
  }

  // Show a waiting icon if we're waiting for the asynchronous call to
  // complete.
  if (future.status() != firebase::kFutureStatusComplete) {
    ShowImage("waiting icon");
    return "";
  }

  // Show error code if the call failed.
  if (future.error() != firebase::auth::kAuthErrorNone) {
    ShowTextBox("Error fetching providers: %s", future.error_message());
  }

  // Show a button for each provider available to this email.
  // Return the provider for the button that's pressed.
  for (size_t i = 0; i < result->providers.size(); ++i) {
    const bool selected = ShowTextButton(result->providers[i].c_str());
    if (selected) return result->providers[i].c_str();
  }
  return "";
}
/// [Providers]

/// [Sign In]
// Try to ensure that we get logged in.
// This function is called every frame.
bool SignIn(firebase::auth::Auth* auth) {
  // Grab the result of the latest sign-in attempt.
  firebase::Future<firebase::auth::User*> future =
      auth->SignInAnonymouslyLastResult();

  // If we're in a state where we can try to sign in, do so.
  if (future.status() == firebase::kFutureStatusInvalid ||
      (future.status() == firebase::kFutureStatusComplete &&
       future.error() != firebase::auth::kAuthErrorNone)) {
    auth->SignInAnonymously();
  }

  // We're signed in if the most recent result was successful.
  return future.status() == firebase::kFutureStatusComplete &&
         future.error() == firebase::auth::kAuthErrorNone;
}
/// [Sign In]

/// [Password Reset]
const char* ImageNameForStatus(const firebase::FutureBase& future) {
  assert(future.status() != firebase::kFutureStatusInvalid);
  return future.status() == firebase::kFutureStatusPending
             ? "waiting icon"
             : future.error() == firebase::auth::kAuthErrorNone ? "checkmark"
                                                                : "x mark";
}

// This function is called once per frame.
void ResetPasswordScreen(firebase::auth::Auth* auth) {
  // Gather email address.
  // ShowInputBox() returns a value when `enter` is pressed.
  const std::string email = ShowInputBox("Enter e-mail");
  if (email != "") {
    auth->SendPasswordResetEmail(email.c_str());
  }

  // Show checkmark, X-mark, or waiting icon beside the
  // email input box, to indicate if email has been sent.
  firebase::Future<void> send_future = auth->SendPasswordResetEmailLastResult();
  ShowImage(ImageNameForStatus(send_future));

  // Display error message if the e-mail could not be sent.
  if (send_future.status() == firebase::kFutureStatusComplete &&
      send_future.error() != firebase::auth::kAuthErrorNone) {
    ShowTextBox(send_future.error_message());
  }
}
/// [Password Reset]

/// [Phone Verification]
class PhoneVerifier : public firebase::auth::PhoneAuthProvider::Listener {
 public:
  PhoneVerifier(const char* phone_number,
                firebase::auth::PhoneAuthProvider* phone_auth_provider)
    : display_message_("Sending SMS with verification code"),
      display_verification_code_input_box_(false),
      display_resend_sms_button_(false),
      phone_auth_provider_(phone_auth_provider),
      phone_number_(phone_number) {
    SendSms();
  }

  ~PhoneVerifier() override {}

  void OnVerificationCompleted(firebase::auth::Credential credential) override {
    // Grab `mutex_` for the scope of `lock`. Callbacks can be called on other
    // threads, so this mutex ensures data access is atomic.
    MutexLock lock(mutex_);
    credential_ = credential;
  }

  void OnVerificationFailed(const std::string& error) override {
    MutexLock lock(mutex_);
    display_message_ = "Verification failed with error: " + error;
  }

  void OnCodeSent(const std::string& verification_id,
                  const firebase::auth::PhoneAuthProvider::ForceResendingToken&
                      force_resending_token) override {
    MutexLock lock(mutex_);
    verification_id_ = verification_id;
    force_resending_token_ = force_resending_token;

    display_verification_code_input_box_ = true;
    display_message_ = "Waiting for SMS";
  }

  void OnCodeAutoRetrievalTimeOut(const std::string& verification_id) override {
    MutexLock lock(mutex_);
    display_resend_sms_button_ = true;
  }

  // Draw the verification GUI on screen and process input events.
  void Draw() {
    MutexLock lock(mutex_);

    // Draw an informative message describing what's currently happening.
    ShowTextBox(display_message_.c_str());

    // Once the time out expires, display a button to resend the SMS.
    // If the button is pressed, call VerifyPhoneNumber again using the
    // force_resending_token_.
    if (display_resend_sms_button_ && !verification_id_.empty()) {
      const bool resend_sms = ShowTextButton("Resend SMS");
      if (resend_sms) {
        SendSms();
      }
    }

    // Once the SMS has been sent, allow the user to enter the SMS
    // verification code into a text box. When the user has completed
    // entering it, call GetCredential() to complete the flow.
    if (display_verification_code_input_box_) {
      const std::string verification_code = ShowInputBox("Verification code");
      if (!verification_code.empty()) {
        credential_ = phone_auth_provider_->GetCredential(
            verification_id_.c_str(), verification_code.c_str());
      }
    }
  }

  // The phone number verification flow is complete when this returns
  // non-NULL.
  firebase::auth::Credential* credential() {
    MutexLock lock(mutex_);
    return credential_.is_valid() ? &credential_ : nullptr;
  }

 private:
  void SendSms() {
    static const uint32_t kAutoVerifyTimeOut = 2000;
    MutexLock lock(mutex_);
    phone_auth_provider_->VerifyPhoneNumber(
        phone_number_.c_str(), kAutoVerifyTimeOut, &force_resending_token_,
        this);
    display_resend_sms_button_ = false;
  }

  // GUI-related variables.
  std::string display_message_;
  bool display_verification_code_input_box_;
  bool display_resend_sms_button_;

  // Phone flow related variables.
  firebase::auth::PhoneAuthProvider* phone_auth_provider_;
  std::string phone_number_;
  std::string verification_id_;
  firebase::auth::PhoneAuthProvider::ForceResendingToken force_resending_token_;
  firebase::auth::Credential credential_;

  // Callbacks can be called on other threads, so guard them with a mutex.
  Mutex mutex_;
};
/// [Phone Verification]
