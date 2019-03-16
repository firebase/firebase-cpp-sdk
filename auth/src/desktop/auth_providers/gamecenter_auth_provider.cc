/*
 * Copyright 2019 Google LLC
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

#include "app/src/assert.h"
#include "app/src/include/firebase/future.h"
#include "auth/src/common.h"
#include "auth/src/desktop/credential_impl.h"
#include "auth/src/include/firebase/auth/credential.h"

bool is_gamecenter_available_on_desktop = false;

namespace firebase {
namespace auth {

// static
Future<Credential> GameCenterAuthProvider::GetCredential() {
  auto future_api = GetCredentialFutureImpl();
  const auto handle = future_api->SafeAlloc<Credential>(
      kCredentialFn_GameCenterGetCredential);

  future_api->Complete(handle, kAuthErrorInvalidCredential,
                       "GameCenter is not supported on Android.");

  FIREBASE_ASSERT_RETURN(MakeFuture(future_api, handle),
                         is_gamecenter_available_on_desktop);

  return MakeFuture(future_api, handle);
}

// static
Future<Credential> GameCenterAuthProvider::GetCredentialLastResult() {
  auto future_api = GetCredentialFutureImpl();
  auto last_result =
      future_api->LastResult(kCredentialFn_GameCenterGetCredential);
  return static_cast<const Future<Credential>&>(last_result);
}

// static
bool GameCenterAuthProvider::IsPlayerAuthenticated() {
  FIREBASE_ASSERT_RETURN(false, is_gamecenter_available_on_desktop);
  return false;
}

}  // namespace auth
}  // namespace firebase
