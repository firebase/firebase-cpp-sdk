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

#include "app/src/assert.h"
#include "auth/src/desktop/auth_credential.h"
#include "auth/src/desktop/credential_impl.h"
#include "auth/src/include/firebase/auth/credential.h"

namespace firebase {
namespace auth {

Credential::~Credential() { delete static_cast<CredentialImpl*>(impl_); }

Credential::Credential(const Credential& rhs) : impl_(nullptr) {
  if (rhs.impl_) {
    impl_ = new CredentialImpl(*(static_cast<CredentialImpl*>(rhs.impl_)));
  }
  error_code_ = rhs.error_code_;
  error_message_ = rhs.error_message_;
}

Credential& Credential::operator=(const Credential& rhs) {
  delete static_cast<CredentialImpl*>(impl_);
  impl_ = nullptr;

  if (rhs.impl_) {
    impl_ = new CredentialImpl(*(static_cast<CredentialImpl*>(rhs.impl_)));
  }

  error_code_ = rhs.error_code_;
  error_message_ = rhs.error_message_;

  return *this;
}

std::string Credential::provider() const {
  FIREBASE_ASSERT_MESSAGE(is_valid(), "Credential doesn't have an valid impl");

  if (impl_ == nullptr) {
    return std::string();
  } else {
    return static_cast<CredentialImpl*>(impl_)->auth_credential->GetProvider();
  }
}

bool Credential::is_valid() const { return impl_ != nullptr; }

}  // namespace auth
}  // namespace firebase
