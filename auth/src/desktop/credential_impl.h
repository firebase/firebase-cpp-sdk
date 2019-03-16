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

#ifndef FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_CREDENTIAL_IMPL_H_
#define FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_CREDENTIAL_IMPL_H_

#include <memory>

#include "auth/src/desktop/auth_credential.h"

namespace firebase {
namespace auth {

class Credential;

struct CredentialImpl {
  explicit CredentialImpl(AuthCredential* set_auth_credential);
  std::shared_ptr<AuthCredential> auth_credential;
};

}  // namespace auth
}  // namespace firebase

#endif  // FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_CREDENTIAL_IMPL_H_
