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

#ifndef FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_AUTH_PROVIDERS_EMAIL_AUTH_CREDENTIAL_H_
#define FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_AUTH_PROVIDERS_EMAIL_AUTH_CREDENTIAL_H_

#include "auth/src/desktop/auth_constants.h"
#include "auth/src/desktop/auth_credential.h"

namespace firebase {
namespace auth {

class EmailAuthProvider;

class EmailAuthCredential : public AuthCredential {
 public:
  std::string GetProvider() const override {
    return kEmailPasswordAuthProviderId;
  }

  std::string GetEmail() const { return email_; }
  std::string GetPassword() const { return password_; }

 private:
  EmailAuthCredential(const std::string& email, const std::string& password)
      : email_(email), password_(password) {}

  const std::string email_;
  const std::string password_;

  friend class EmailAuthProvider;
};

}  // namespace auth
}  // namespace firebase

#endif  // FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_AUTH_PROVIDERS_EMAIL_AUTH_CREDENTIAL_H_
