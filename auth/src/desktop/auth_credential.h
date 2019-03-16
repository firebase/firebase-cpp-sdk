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

#ifndef FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_AUTH_CREDENTIAL_H_
#define FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_AUTH_CREDENTIAL_H_

#include <string>

namespace firebase {
namespace auth {

// Represents a credential that the Firebase Authentication server can use to
// authenticate a user.
class AuthCredential {
 public:
  virtual ~AuthCredential();

  // Returns the unique string identifier of the provider for the credential.
  virtual std::string GetProvider() const = 0;

 protected:
  // This is an abstract class. Concrete instances should be created via factory
  // method getCredential() in each authentication provider class e.g.
  // FacebookAuthProvider::getCredential().
  AuthCredential() {}
};

}  // namespace auth
}  // namespace firebase

#endif  // FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_AUTH_CREDENTIAL_H_
