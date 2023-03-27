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

#include "app/src/util.h"

namespace firebase {
namespace auth {

const char* kUserNotInitializedErrorMessage =
    "Operation attempted on an invalid User object.";
const char* kPhoneAuthNotSupportedErrorMessage =
    "Phone Auth is not supported on this platform.";
const char* kAuthInvalidParameterErrorMessage =
    "A parameter pass to the auth method is null or invalid.";
extern const char* kInvalidCredentialErrorMessage =
    "The provided credential does not match the required type.";
extern const char* kErrorEmptyEmailPasswordErrorMessage =
    "Empty email or password are not allowed.";

// static member variables
const uint32_t PhoneAuthProvider::kMaxTimeoutMs = 3000;

namespace {
static const char kCredentialFutureIdentifier[17] = "Auth-Credentials";
}

const char* const EmailAuthProvider::kProviderId = "password";
const char* const FacebookAuthProvider::kProviderId = "facebook.com";
const char* const GameCenterAuthProvider::kProviderId = "gc.apple.com";
const char* const GitHubAuthProvider::kProviderId = "github.com";
const char* const GoogleAuthProvider::kProviderId = "google.com";
const char* const MicrosoftAuthProvider::kProviderId = "microsoft.com";
const char* const PhoneAuthProvider::kProviderId = "phone";
const char* const PlayGamesAuthProvider::kProviderId = "playgames.google.com";
const char* const TwitterAuthProvider::kProviderId = "twitter.com";
const char* const YahooAuthProvider::kProviderId = "yahoo.com";

ReferenceCountedFutureImpl* GetCredentialFutureImpl() {
  StaticFutureData* future_data = StaticFutureData::GetFutureDataForModule(
      &kCredentialFutureIdentifier, kNumCredentialFunctions);
  if (future_data == nullptr) return nullptr;

  return future_data->api();
}

void CompleteFuture(int error, const char* error_msg,
                    SafeFutureHandle<void> handle, FutureData* future_data) {
  if (future_data->future_impl.ValidFuture(handle)) {
    future_data->future_impl.Complete(handle, error, error_msg);
  }
}

void CompleteFuture(int error, const char* error_msg,
                    SafeFutureHandle<std::string> handle,
                    FutureData* future_data, const std::string& result) {
  if (future_data->future_impl.ValidFuture(handle)) {
    future_data->future_impl.CompleteWithResult(handle, error, error_msg,
                                                result);
  }
}

void CompleteFuture(int error, const char* error_msg,
                    SafeFutureHandle<User*> handle, FutureData* future_data,
                    User* user) {
  if (future_data->future_impl.ValidFuture(handle)) {
    future_data->future_impl.CompleteWithResult(handle, error, error_msg, user);
  }
}

void CompleteFuture(int error, const char* error_msg,
                    SafeFutureHandle<SignInResult> handle,
                    FutureData* future_data, SignInResult sign_in_result) {
  if (future_data->future_impl.ValidFuture(handle)) {
    future_data->future_impl.CompleteWithResult(handle, error, error_msg,
                                                sign_in_result);
  }
}

// For calls that aren't asynchronous, we can create and complete at the
// same time.
Future<void> CreateAndCompleteFuture(int fn_idx, int error,
                                     const char* error_msg,
                                     FutureData* future_data) {
  SafeFutureHandle<void> handle = CreateFuture<void>(fn_idx, future_data);
  CompleteFuture(error, error_msg, handle, future_data);
  return MakeFuture(&future_data->future_impl, handle);
}

Future<std::string> CreateAndCompleteFuture(int fn_idx, int error,
                                            const char* error_msg,
                                            FutureData* future_data,
                                            const std::string& result) {
  SafeFutureHandle<std::string> handle =
      CreateFuture<std::string>(fn_idx, future_data);
  CompleteFuture(error, error_msg, handle, future_data, result);
  return MakeFuture(&future_data->future_impl, handle);
}

void CleanupCredentialFutureImpl() {
  StaticFutureData::CleanupFutureDataForModule(&kCredentialFutureIdentifier);
}

void ClearUserInfos(AuthData* auth_data) {
  for (size_t i = 0; i < auth_data->user_infos.size(); ++i) {
    delete auth_data->user_infos[i];
    auth_data->user_infos[i] = nullptr;
  }
  auth_data->user_infos.clear();
}

void PhoneAuthProvider::Listener::OnCodeSent(
    const std::string& /*verification_id*/,
    const ForceResendingToken& /*force_resending_token*/) {}

void PhoneAuthProvider::Listener::OnCodeAutoRetrievalTimeOut(
    const std::string& /*verification_id*/) {}

}  // namespace auth
}  // namespace firebase
