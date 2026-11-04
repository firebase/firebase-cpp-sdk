/*
 * Copyright 2016 Google LLC
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

#include "auth/src/common.h"

#include <string>

#include "app/src/util.h"

namespace firebase {
namespace auth {

namespace {
const char kCredentialFutureIdentifier[17] = "Auth-Credentials";
}  // namespace

const char* const EmailAuthProvider::kProviderId = "password";
const char* const FacebookAuthProvider::kProviderId = "facebook.com";
const char* const GameCenterAuthProvider::kProviderId = "gc.apple.com";
const char* const GitHubAuthProvider::kProviderId = "github.com";
const char* const GoogleAuthProvider::kProviderId = "google.com";
const char* const MicrosoftAuthProvider::kProviderId = "microsoft.com";
const char* const PhoneAuthProvider::kProviderId = "phone";
const char* const PlayGamesAuthProvider::kProviderId = "playgames.google.com";
const char* const TwitterAuthProvider::kProviderId = "twitter.com";
const char* const YahooAuthProvider::kProviderId = "yahoo.com";

ReferenceCountedFutureImpl* GetCredentialFutureImpl() {
  StaticFutureData* future_data = StaticFutureData::GetFutureDataForModule(
      &kCredentialFutureIdentifier, kNumCredentialFunctions);
  return future_data ? future_data->api() : nullptr;
}

void CleanupCredentialFutureImpl() {
  StaticFutureData::CleanupFutureDataForModule(&kCredentialFutureIdentifier);
}

void ClearUserInfos(AuthData* auth_data) {
  if (!auth_data) return;
  for (auto* info : auth_data->user_infos) delete info;
  auth_data->user_infos.clear();
}

PhoneAuthOptions::PhoneAuthOptions()
    : force_resending_token(nullptr),
      timeout_milliseconds(0),
      ui_parent(nullptr) {}

void PhoneAuthProvider::Listener::OnCodeSent(
    const std::string& /*verification_id*/,
    const ForceResendingToken& /*force_resending_token*/) {}

void PhoneAuthProvider::Listener::OnCodeAutoRetrievalTimeOut(
    const std::string& /*verification_id*/) {}

User::User() : auth_data_(nullptr) {}

bool User::operator==(const User& user) const {
  const bool lhs_valid = is_valid();
  const bool rhs_valid = user.is_valid();
  if (lhs_valid != rhs_valid) return false;
  if (!lhs_valid) return true;
  return auth_data_ == user.auth_data_ && uid() == user.uid();
}

bool User::operator!=(const User& user) const { return !(*this == user); }

}  // namespace auth
}  // namespace firebase
