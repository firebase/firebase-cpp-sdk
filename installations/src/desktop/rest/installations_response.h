//  Copyright (c) 2022 Google LLC
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

#ifndef FIREBASE_INSTALLATIONS_SRC_DESKTOP_REST_INSTALLATIONS_RESPONSE_H_
#define FIREBASE_INSTALLATIONS_SRC_DESKTOP_REST_INSTALLATIONS_RESPONSE_H_

#include "app/rest/response_json.h"
#include "installations/installations_response_generated.h"
#include "installations/installations_response_resource.h"

namespace firebase {
namespace installations {
namespace internal {

class InstallationsResponse
    : public firebase::rest::ResponseJson<fbs::Installations, fbs::InstallationsT> {
 public:
  InstallationsResponse() : InstallationsResponse(installations_response_resource_data) {}
  explicit InstallationsResponse(const char* schema);

  explicit InstallationsResponse(const unsigned char* schema)
      : InstallationsResponse(reinterpret_cast<const char*>(schema)) {}

  std::string GetFid();

};
}  // namespace internal
}  // namespace installations
}  // namespace firebase

#endif /* FIREBASE_INSTALLATIONS_SRC_DESKTOP_REST_INSTALLATIONS_RESPONSE_H_ */
