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

#ifndef FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_IDENTITY_PROVIDER_CREDENTIAL_H_
#define FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_IDENTITY_PROVIDER_CREDENTIAL_H_

#include <memory>

#include "auth/src/desktop/auth_credential.h"
#include "auth/src/desktop/rpcs/verify_assertion_request.h"

namespace firebase {
namespace auth {

// An interface for credentials that represent OAuth Identity Providers that can
// be authenticated through VerifyAssertion API.
//
// Most types of credentials are IdP credentials which go through
// VerifyAssertion API. Notable exceptions are email and phone; client code can
// create (Email|Phone)AuthCredential and pass it to SignInWithCredential, which
// is effectively another way of calling SignInWithEmailAndPassword/etc (this is
// how it works in other language SDKs, and desktop should have the same
// behavior for consistency). Now, all credentials that actually go through
// SignInWithCredential are transformed into a VerifyAssertionRequest, so it
// makes sense to have this in the base class, but it can't be AuthCredential,
// because this method wouldn't make sense for email/phone auth, and I'd avoid
// OperationNotSupportedException. The solution is to have an intermediate
// abstract class in the hierarchy.
class IdentityProviderCredential : public AuthCredential {
 public:
  virtual std::unique_ptr<VerifyAssertionRequest> CreateVerifyAssertionRequest(
      const char* api_key) const = 0;
};

}  // namespace auth
}  // namespace firebase

#endif  // FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_IDENTITY_PROVIDER_CREDENTIAL_H_
