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

#include <cassert>

#include "app/src/assert.h"
#include "auth/src/desktop/auth_providers/oauth_auth_credential.h"
#include "auth/src/desktop/credential_impl.h"
#include "auth/src/include/firebase/auth/credential.h"

namespace firebase {
namespace auth {

// static
Credential OAuthProvider::GetCredential(const char* const provider_id,
                                        const char* const id_token,
                                        const char* const access_token) {
  return GetCredential(provider_id, id_token, /*raw_nonce=*/"", access_token);
}

// static
Credential OAuthProvider::GetCredential(const char* provider_id,
                                        const char* id_token,
                                        const char* raw_nonce,
                                        const char* access_token) {
  FIREBASE_ASSERT_RETURN(Credential(), provider_id != nullptr);
  const char* const safe_id_token = id_token ? id_token : "";
  const char* const safe_raw_nonce = raw_nonce ? raw_nonce : "";
  const char* const safe_access_token = access_token ? access_token : "";
  if (provider_id[0] == '\0' ||
      (safe_id_token[0] == '\0' && safe_access_token[0] == '\0')) {
    return Credential();
  }
  return Credential{new CredentialImpl{new OAuthCredential{
      provider_id, safe_id_token, safe_raw_nonce, safe_access_token}}};
}

}  // namespace auth
}  // namespace firebase
