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

#ifndef FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_CREDENTIAL_UTIL_H_
#define FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_CREDENTIAL_UTIL_H_

#include <memory>
#include <string>
#include "app/rest/request.h"
#include "auth/src/common.h"
#include "auth/src/data.h"
#include "auth/src/desktop/auth_providers/email_auth_credential.h"
#include "auth/src/desktop/rpcs/verify_assertion_request.h"

namespace firebase {
namespace auth {

// Note: these functions have to deal with void* pointers because impl_ is
// private in Credential. Thus the Auth/User methods that have friend access to
// Credential internals have to pass credential.impl_ to these helpers.

// Creates a VerifyAssertionRequest, assuming that the given void pointer refers
// to a CredentialImpl*.
//
// It's fully the responsibility of the caller to provide the pointer to the
// correct object. The assumption is that it points to CredentialImpl which
// contains a pointer to a IdentityProviderCredential.
std::unique_ptr<VerifyAssertionRequest> CreateVerifyAssertionRequest(
    const AuthData& auth_data, const void* raw_credential_impl);

// Creates either a VerifyPasswordRequest (if it's email credential) or
// a VerifyAssertionRequest (if it's one of OAuth providers), assuming that the
// given void pointer refers to a CredentialImpl*.
std::unique_ptr<rest::Request> CreateRequestFromCredential(
    AuthData* const auth_data, const std::string& provider,
    const void* raw_credential);

// Extracts a pointer to EmailAuthCredential* from the given void pointer (which
// must actually be CredentialImpl*).
//
// It's fully the responsibility of the caller to provide the pointer to the
// correct object. The assumption is that it points to CredentialImpl which
// contains a pointer to a EmailAuthCredential.
const EmailAuthCredential* GetEmailCredential(const void* raw_credential_impl);

}  // namespace auth
}  // namespace firebase

#endif  // FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_CREDENTIAL_UTIL_H_
