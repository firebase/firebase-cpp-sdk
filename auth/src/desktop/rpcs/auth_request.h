/*
 * Copyright 2018 Google LLC
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

#ifndef FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_RPCS_AUTH_REQUEST_H_
#define FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_RPCS_AUTH_REQUEST_H_

#include "app/rest/request_json.h"
#include "auth/request_generated.h"
#include "auth/request_resource.h"

namespace firebase {
namespace auth {

// Key name for header when sending language code data.
extern const char* kHeaderFirebaseLocale;

class AuthRequest
    : public firebase::rest::RequestJson<fbs::Request, fbs::RequestT> {
 public:
  explicit AuthRequest(const char* schema);

  explicit AuthRequest(const unsigned char* schema)
      : AuthRequest(reinterpret_cast<const char*>(schema)) {}
};

}  // namespace auth
}  // namespace firebase

#endif  // FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_RPCS_AUTH_REQUEST_H_
