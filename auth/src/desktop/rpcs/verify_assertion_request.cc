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

#include "app/src/assert.h"

namespace firebase {
namespace auth {

VerifyAssertionRequest::VerifyAssertionRequest(const char* const api_key,
                                               const char* const provider_id)
    : AuthRequest(request_resource_data) {
  FIREBASE_ASSERT_RETURN_VOID(api_key);

  const char api_host[] =
      "https://www.googleapis.com/identitytoolkit/v3/relyingparty/"
      "verifyAssertion?key=";
  std::string url;
  url.reserve(strlen(api_host) + strlen(api_key));
  url.append(api_host);
  url.append(api_key);
  set_url(url.c_str());
  application_data_->requestUri = url;

  if (provider_id) {
    post_body_ = std::string{"providerId="} + provider_id;
  } else {
    LogError("No provider id given");
  }
  application_data_->returnSecureToken = true;
}

std::unique_ptr<VerifyAssertionRequest> VerifyAssertionRequest::FromIdToken(
    const char* const api_key, const char* const provider_id,
    const char* const id_token) {
  return FromIdToken(api_key, provider_id, id_token, /*nonce=*/nullptr);
}

std::unique_ptr<VerifyAssertionRequest> VerifyAssertionRequest::FromIdToken(
    const char* const api_key, const char* const provider_id,
    const char* const id_token, const char* nonce) {
  auto request = std::unique_ptr<VerifyAssertionRequest>(  // NOLINT
      new VerifyAssertionRequest{api_key, provider_id});

  if (id_token) {
    request->post_body_ += std::string{"&id_token="} + id_token;
  } else {
    LogError("No id token given");
  }

  if (nonce) {
    request->post_body_ += std::string{"&nonce="} + nonce;
  }

  request->application_data_->postBody = request->post_body_;
  request->UpdatePostFields();
  return request;
}

std::unique_ptr<VerifyAssertionRequest> VerifyAssertionRequest::FromAccessToken(
    const char* const api_key, const char* const provider_id,
    const char* const access_token) {
  return FromAccessToken(api_key, provider_id, access_token,
                                /*nonce=*/nullptr);
}

std::unique_ptr<VerifyAssertionRequest> VerifyAssertionRequest::FromAccessToken(
    const char* const api_key, const char* const provider_id,
    const char* const access_token, const char* nonce) {
  auto request = std::unique_ptr<VerifyAssertionRequest>(  // NOLINT
      new VerifyAssertionRequest{api_key, provider_id});

  if (access_token) {
    request->post_body_ += std::string{"&access_token="} + access_token;
  } else {
    LogError("No access token given");
  }

  if (nonce) {
    request->post_body_ += std::string{"&nonce="} + nonce;
  }

  request->application_data_->postBody = request->post_body_;
  request->UpdatePostFields();
  return request;
}

std::unique_ptr<VerifyAssertionRequest>
VerifyAssertionRequest::FromAccessTokenAndOAuthSecret(
    const char* const api_key, const char* const provider_id,
    const char* const access_token, const char* const oauth_secret) {
  auto request = std::unique_ptr<VerifyAssertionRequest>(  // NOLINT
      new VerifyAssertionRequest{api_key, provider_id});

  if (access_token) {
    request->post_body_ += std::string{"&access_token="} + access_token;
  } else {
    LogError("No access token given");
  }
  if (oauth_secret) {
    request->post_body_ += std::string{"&oauth_token_secret="} + oauth_secret;
  } else {
    LogError("No OAuth secret given");
  }

  request->application_data_->postBody = request->post_body_;
  request->UpdatePostFields();
  return request;
}

static std::unique_ptr<VerifyAssertionRequest> FromAuthCode(
    const char* api_key, const char* provider_id, const char* auth_code);

std::unique_ptr<VerifyAssertionRequest> VerifyAssertionRequest::FromAuthCode(
    const char* const api_key, const char* const provider_id,
    const char* const auth_code) {
  auto request = std::unique_ptr<VerifyAssertionRequest>(  // NOLINT
      new VerifyAssertionRequest{api_key, provider_id});

  if (auth_code) {
    request->post_body_ += std::string{"&code="} + auth_code;
  } else {
    LogError("No server auth code given");
  }

  request->application_data_->postBody = request->post_body_;
  request->UpdatePostFields();
  return request;
}

}  // namespace auth
}  // namespace firebase
