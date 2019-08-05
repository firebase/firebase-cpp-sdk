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

#include "auth/src/desktop/auth_util.h"

#include "app/rest/transport_builder.h"
#include "app/src/variant_util.h"
#include "auth/src/desktop/auth_desktop.h"
#include "auth/src/desktop/credential_impl.h"
#include "auth/src/desktop/identity_provider_credential.h"
#include "auth/src/desktop/rpcs/get_account_info_response.h"

namespace firebase {
namespace auth {

const char* GetApiKey(const AuthData& auth_data) {
  FIREBASE_ASSERT_RETURN("", auth_data.auth_impl);
  return static_cast<const AuthImpl*>(auth_data.auth_impl)->api_key.c_str();
}

void CompletePromise(Promise<User*>* const promise,
                     const SignInResult& sign_in_result) {
  FIREBASE_ASSERT_RETURN_VOID(promise);
  promise->CompleteWithResult(sign_in_result.user);
}

void CompletePromise(Promise<SignInResult>* const promise,
                     const SignInResult& sign_in_result) {
  FIREBASE_ASSERT_RETURN_VOID(promise);
  promise->CompleteWithResult(sign_in_result);
}

void CompletePromise(Promise<void>* const promise,
                     const SignInResult& /*unused*/) {
  FIREBASE_ASSERT_RETURN_VOID(promise);
  promise->Complete();
}

}  // namespace auth
}  // namespace firebase
