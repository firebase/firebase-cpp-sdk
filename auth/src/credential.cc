/*
 * Copyright 2023 Google LLC
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

#include "auth/src/include/firebase/auth/credential.h"

#include <utility>

#include "auth/src/common.h"

namespace firebase {
namespace auth {

#if !defined(SWIG)
Credential::Credential(Credential&& rhs) noexcept
    : impl_(rhs.impl_),
      error_code_(rhs.error_code_),
      error_message_(std::move(rhs.error_message_)) {
  rhs.impl_ = nullptr;
  rhs.error_code_ = kAuthErrorNone;
}

Credential& Credential::operator=(Credential&& rhs) noexcept {
  if (this != &rhs) {
    std::swap(impl_, rhs.impl_);
    std::swap(error_code_, rhs.error_code_);
    std::swap(error_message_, rhs.error_message_);
  }
  return *this;
}
#endif  // !defined(SWIG)

PhoneAuthCredential::PhoneAuthCredential() : Credential() {}

PhoneAuthCredential::PhoneAuthCredential(void* impl) : Credential(impl) {}

PhoneAuthCredential::PhoneAuthCredential(const PhoneAuthCredential& rhs)
    : Credential(rhs), sms_code_(rhs.sms_code_) {}

#if !defined(SWIG)
PhoneAuthCredential::PhoneAuthCredential(PhoneAuthCredential&& rhs) noexcept
    : Credential(std::move(rhs)), sms_code_(std::move(rhs.sms_code_)) {}

PhoneAuthCredential& PhoneAuthCredential::operator=(
    PhoneAuthCredential&& rhs) noexcept {
  if (this != &rhs) {
    Credential::operator=(std::move(rhs));
    sms_code_ = std::move(rhs.sms_code_);
  }
  return *this;
}
#endif  // !defined(SWIG)

}  // namespace auth
}  // namespace firebase
