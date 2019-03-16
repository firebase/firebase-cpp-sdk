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

#include <stdint.h>
#include "app/src/assert.h"
#include "auth/src/data.h"
#include "auth/src/include/firebase/auth.h"
#include "auth/src/include/firebase/auth/credential.h"

namespace firebase {
namespace auth {

// Right now we use the stub implementation. Need to replace this with logic
// that works for desktop.

static const char* kMockVerificationId = "mock verification id";

PhoneAuthProvider::ForceResendingToken::ForceResendingToken()
    : data_(nullptr) {}

PhoneAuthProvider::ForceResendingToken::~ForceResendingToken() {}

PhoneAuthProvider::ForceResendingToken::ForceResendingToken(
    const ForceResendingToken& rhs)
    : data_(nullptr) {}

PhoneAuthProvider::ForceResendingToken& PhoneAuthProvider::ForceResendingToken::
operator=(const ForceResendingToken& rhs) {
  return *this;
}

bool PhoneAuthProvider::ForceResendingToken::operator==(
    const ForceResendingToken& rhs) const {
  return true;
}

bool PhoneAuthProvider::ForceResendingToken::operator!=(
    const ForceResendingToken& rhs) const {
  return true;
}

PhoneAuthProvider::Listener::Listener() : data_(nullptr) {}
PhoneAuthProvider::Listener::~Listener() {}

PhoneAuthProvider::PhoneAuthProvider() : data_(nullptr) {}
PhoneAuthProvider::~PhoneAuthProvider() {}

void PhoneAuthProvider::VerifyPhoneNumber(
    const char* /*phone_number*/, uint32_t /*auto_verify_time_out_ms*/,
    const ForceResendingToken* force_resending_token, Listener* listener) {
  FIREBASE_ASSERT_RETURN_VOID(listener != nullptr);

  // Mock the tokens by sending a new one whenever it's unspecified.
  ForceResendingToken token;
  if (force_resending_token != nullptr) {
    token = *force_resending_token;
  }

  listener->OnCodeAutoRetrievalTimeOut(kMockVerificationId);
  listener->OnCodeSent(kMockVerificationId, token);
}

Credential PhoneAuthProvider::GetCredential(const char* verification_id,
                                            const char* verification_code) {
  FIREBASE_ASSERT_MESSAGE_RETURN(Credential(nullptr), false,
                                 "Phone Auth is not supported on desktop");

  return Credential(nullptr);
}

// static
PhoneAuthProvider& PhoneAuthProvider::GetInstance(Auth* auth) {
  return auth->auth_data_->phone_auth_provider;
}

}  // namespace auth
}  // namespace firebase
