// Copyright 2022 Google LLC
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

#ifndef FIREBASE_APP_CHECK_SRC_DESKTOP_TOKEN_RESPONSE_H_
#define FIREBASE_APP_CHECK_SRC_DESKTOP_TOKEN_RESPONSE_H_

#include <string>

#include "app/rest/response_json.h"
#include "app_check/token_response_generated.h"
#include "app_check/token_response_resource.h"
#include "firebase/app.h"

namespace firebase {
namespace app_check {
namespace internal {

class TokenResponse : public firebase::rest::ResponseJson<fbs::TokenResponse,
                                                          fbs::TokenResponseT> {
 public:
  TokenResponse() : ResponseJson(token_response_resource_data) {}

  const std::string& token() { return application_data_->token; }
  const std::string& ttl() { return application_data_->ttl; }
};

}  // namespace internal
}  // namespace app_check
}  // namespace firebase

#endif  // FIREBASE_APP_CHECK_SRC_DESKTOP_TOKEN_RESPONSE_H_
