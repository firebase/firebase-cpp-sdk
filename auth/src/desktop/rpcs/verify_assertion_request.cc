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

#include "auth/src/desktop/rpcs/verify_assertion_request.h"

#include <string>

#include "app/src/assert.h"
#include "app/src/include/firebase/app.h"

namespace firebase {
namespace auth {

namespace {
void AppendParam(std::string* body, const char* key, const char* val,
                 const char* err = nullptr) {
  if (val && *val != '\0') {
    *body += std::string(key) + AuthRequest::UrlEncode(val);
  } else if (err) {
    LogError(err);
  }
}
}  // namespace

VerifyAssertionRequest::VerifyAssertionRequest(::firebase::App& app,
                                               const char* const api_key,
                                               const char* const provider_id)
    : AuthRequest(app, request_resource_data, true, "verifyAssertion?key=",
                  api_key) {
  FIREBASE_ASSERT_RETURN_VOID(api_key);
  application_data_->requestUri = "http://localhost";
  AppendParam(&application_data_->postBody, "providerId=", provider_id,
              "No provider id given");
  application_data_->returnSecureToken = true;
}

std::unique_ptr<VerifyAssertionRequest> VerifyAssertionRequest::FromIdToken(
    ::firebase::App& app, const char* const api_key,
    const char* const provider_id, const char* const id_token) {
  return FromIdToken(app, api_key, provider_id, id_token, nullptr);
}

std::unique_ptr<VerifyAssertionRequest> VerifyAssertionRequest::FromIdToken(
    ::firebase::App& app, const char* const api_key,
    const char* const provider_id, const char* const id_token,
    const char* nonce) {
  auto request = std::unique_ptr<VerifyAssertionRequest>(
      new VerifyAssertionRequest{app, api_key, provider_id});  // NOLINT
  AppendParam(&request->application_data_->postBody, "&id_token=", id_token,
              "No id token given");
  AppendParam(&request->application_data_->postBody, "&nonce=", nonce);
  request->UpdatePostFields();
  return request;
}

std::unique_ptr<VerifyAssertionRequest> VerifyAssertionRequest::FromAccessToken(
    ::firebase::App& app, const char* const api_key,
    const char* const provider_id, const char* const access_token) {
  return FromAccessToken(app, api_key, provider_id, access_token, nullptr);
}

std::unique_ptr<VerifyAssertionRequest> VerifyAssertionRequest::FromAccessToken(
    ::firebase::App& app, const char* const api_key,
    const char* const provider_id, const char* const access_token,
    const char* nonce) {
  auto request = std::unique_ptr<VerifyAssertionRequest>(
      new VerifyAssertionRequest{app, api_key, provider_id});  // NOLINT
  AppendParam(&request->application_data_->postBody,
              "&access_token=", access_token, "No access token given");
  AppendParam(&request->application_data_->postBody, "&nonce=", nonce);
  request->UpdatePostFields();
  return request;
}

std::unique_ptr<VerifyAssertionRequest>
VerifyAssertionRequest::FromAccessTokenAndOAuthSecret(
    ::firebase::App& app, const char* const api_key,
    const char* const provider_id, const char* const access_token,
    const char* const oauth_secret) {
  auto request = std::unique_ptr<VerifyAssertionRequest>(
      new VerifyAssertionRequest{app, api_key, provider_id});  // NOLINT
  AppendParam(&request->application_data_->postBody,
              "&access_token=", access_token, "No access token given");
  AppendParam(&request->application_data_->postBody,
              "&oauth_token_secret=", oauth_secret, "No OAuth secret given");
  request->UpdatePostFields();
  return request;
}

std::unique_ptr<VerifyAssertionRequest> VerifyAssertionRequest::FromAuthCode(
    ::firebase::App& app, const char* const api_key,
    const char* const provider_id, const char* const auth_code) {
  auto request = std::unique_ptr<VerifyAssertionRequest>(
      new VerifyAssertionRequest{app, api_key, provider_id});  // NOLINT
  AppendParam(&request->application_data_->postBody, "&code=", auth_code,
              "No server auth code given");
  request->UpdatePostFields();
  return request;
}

}  // namespace auth
}  // namespace firebase
