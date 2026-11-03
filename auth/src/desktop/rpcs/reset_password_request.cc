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

#include "auth/src/desktop/rpcs/reset_password_request.h"

#include "app/src/assert.h"
#include "app/src/include/firebase/app.h"
#include "app/src/log.h"

namespace firebase {
namespace auth {

ResetPasswordRequest::ResetPasswordRequest(::firebase::App& app,
                                           const char* api_key,
                                           const char* oob_code,
                                           const char* new_password)
    : AuthRequest(app, request_resource_data, true, "resetPassword?key=",
                  api_key) {
  FIREBASE_ASSERT_RETURN_VOID(api_key);

  if (oob_code && *oob_code != '\0') {
    application_data_->oobCode = oob_code;
  } else {
    LogError("No oob code given.");
  }
  if (new_password && *new_password != '\0') {
    application_data_->newPassword = new_password;
  } else {
    LogError("No new password given.");
  }
  UpdatePostFields();
}

}  // namespace auth
}  // namespace firebase
