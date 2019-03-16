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

#ifndef FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_VALIDATE_CREDENTIAL_H_
#define FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_VALIDATE_CREDENTIAL_H_

#include <string>
#include "auth/src/desktop/auth_constants.h"
#include "auth/src/desktop/auth_providers/email_auth_credential.h"
#include "auth/src/desktop/credential_util.h"
#include "auth/src/desktop/promise.h"

namespace firebase {
namespace auth {

// Checks that the given credential is valid to be passed to the backend.
// Void pointer is assumed to refer to a valid CredentialImpl, which will be
// downcasted based on the provider string.
// If the credential is found to be invalid, the function will fail the given
// promise and return false. Otherwise, it won't touch the promise, and will
// return true.
template <typename FutureResultT>
bool ValidateCredential(Promise<FutureResultT>* promise,
                        const std::string& provider,
                        const void* raw_credential);

// Performs sanity checks for the given email.
// If the email is found to be invalid, the function will fail the given
// promise and return false. Otherwise, it won't touch the promise, and will
// return true.
template <typename FutureResultT>
bool ValidateEmail(Promise<FutureResultT>* promise, const char* email);

// Performs sanity checks for the given email and password.
// If the credentials are found to be invalid, the function will fail the given
// promise and return false. Otherwise, it won't touch the promise, and will
// return true.
template <typename FutureResultT>
bool ValidateEmailAndPassword(Promise<FutureResultT>* promise,
                              const char* email, const char* password);

// Impl

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

  if (provider == kEmailPasswordAuthProviderId) {
    const EmailAuthCredential* email_credential =
        GetEmailCredential(raw_credential);
    return ValidateEmailAndPassword(promise,
                                    email_credential->GetEmail().c_str(),
                                    email_credential->GetPassword().c_str());
  } else if (provider == kPhoneAuthProdiverId) {
    // TODO(varconst): for now phone auth is not supported on desktop.
    promise->Fail(kAuthErrorApiNotAvailable,
                  "Phone Auth is not supported on desktop");
    return false;
  }
  return true;
}

}  // namespace auth
}  // namespace firebase
#endif  // FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_VALIDATE_CREDENTIAL_H_
