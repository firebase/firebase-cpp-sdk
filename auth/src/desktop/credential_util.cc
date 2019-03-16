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

#include "auth/src/desktop/credential_util.h"

#include "app/src/assert.h"
#include "auth/src/desktop/auth_util.h"
#include "auth/src/desktop/credential_impl.h"
#include "auth/src/desktop/identity_provider_credential.h"
#include "auth/src/desktop/rpcs/verify_assertion_request.h"
#include "auth/src/desktop/rpcs/verify_password_request.h"

namespace firebase {
namespace auth {

std::unique_ptr<VerifyAssertionRequest> CreateVerifyAssertionRequest(
    const AuthData& auth_data, const void* const raw_credential_impl) {
  const auto* credential_impl =
      static_cast<const CredentialImpl*>(raw_credential_impl);
  const auto* idp_credential = static_cast<const IdentityProviderCredential*>(
      credential_impl->auth_credential.get());
  return idp_credential->CreateVerifyAssertionRequest(GetApiKey(auth_data));
}

std::unique_ptr<rest::Request> CreateRequestFromCredential(
    AuthData* const auth_data, const std::string& provider,
    const void* const raw_credential) {
  FIREBASE_ASSERT_RETURN(nullptr, provider != kPhoneAuthProdiverId);

  if (provider == kEmailPasswordAuthProviderId) {
    const EmailAuthCredential* email_credential =
        GetEmailCredential(raw_credential);
    if (!email_credential) {
      return nullptr;
    }
    return std::unique_ptr<rest::Request>(  // NOLINT
        new VerifyPasswordRequest(GetApiKey(*auth_data),
                                  email_credential->GetEmail().c_str(),
                                  email_credential->GetPassword().c_str()));
  }

  return CreateVerifyAssertionRequest(*auth_data, raw_credential);
}

const EmailAuthCredential* GetEmailCredential(
    const void* const raw_credential_impl) {
  FIREBASE_ASSERT_RETURN(nullptr, raw_credential_impl);

  const auto* credential_impl =
      static_cast<const CredentialImpl*>(raw_credential_impl);
  return static_cast<const EmailAuthCredential*>(
      credential_impl->auth_credential.get());
}

}  // namespace auth
}  // namespace firebase
