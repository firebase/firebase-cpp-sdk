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

#ifndef FIREBASE_AUTH_SRC_DESKTOP_VALIDATE_CREDENTIAL_H_
#define FIREBASE_AUTH_SRC_DESKTOP_VALIDATE_CREDENTIAL_H_

#include <cstring>
#include <string>

#include "auth/src/common/promise.h"
#include "auth/src/desktop/auth_constants.h"
#include "auth/src/desktop/auth_providers/email_auth_credential.h"
#include "auth/src/desktop/credential_impl.h"
#include "auth/src/desktop/credential_util.h"

namespace firebase {
namespace auth {

template <typename FutureResultT>
inline bool ValidateEmail(Promise<FutureResultT>* promise, const char* email) {
  FIREBASE_ASSERT_RETURN(false, promise);
  if (!email || strlen(email) == 0) {
    FailPromise(promise, kAuthErrorMissingEmail);
    return false;
  }
  return true;
}

template <typename FutureResultT>
inline bool ValidatePassword(Promise<FutureResultT>* const promise,
                             const char* const password) {
  FIREBASE_ASSERT_RETURN(false, promise);
  if (!password || strlen(password) == 0) {
    FailPromise(promise, kAuthErrorMissingPassword);
    return false;
  }
  return true;
}

template <typename FutureResultT>
inline bool ValidateEmailAndPassword(Promise<FutureResultT>* const promise,
                                     const char* const email,
                                     const char* const password) {
  return ValidateEmail(promise, email) && ValidatePassword(promise, password);
}

template <typename FutureResultT>
inline bool ValidateCredential(Promise<FutureResultT>* const promise,
                               const std::string& provider,
                               const void* const raw_credential) {
  FIREBASE_ASSERT_RETURN(false, promise);
  if (provider == kPhoneAuthProviderId) {
    promise->Fail(kAuthErrorApiNotAvailable,
                  "Phone Auth is not supported on desktop");
    return false;
  }
  const auto* credential_impl =
      static_cast<const CredentialImpl*>(raw_credential);
  if (!credential_impl || !credential_impl->auth_credential) {
    FailPromise(promise, kAuthErrorInvalidCredential);
    return false;
  }
  if (provider == kEmailPasswordAuthProviderId &&
      credential_impl->auth_credential->GetType() ==
          AuthCredential::CredentialType::kEmail) {
    const EmailAuthCredential* email_credential =
        GetEmailCredential(raw_credential);
    if (!email_credential) {
      FailPromise(promise, kAuthErrorInvalidCredential);
      return false;
    }
    return ValidateEmailAndPassword(promise,
                                    email_credential->GetEmail().c_str(),
                                    email_credential->GetPassword().c_str());
  }
  if (!credential_impl->auth_credential->IsValid()) {
    FailPromise(promise, kAuthErrorInvalidCredential);
    return false;
  }
  return true;
}

}  // namespace auth
}  // namespace firebase
#endif  // FIREBASE_AUTH_SRC_DESKTOP_VALIDATE_CREDENTIAL_H_
