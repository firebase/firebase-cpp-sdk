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

#ifndef FIREBASE_INSTALLATIONS_SRC_DESKTOP_REST_INSTALLATIONS_REQUEST_H_
#define FIREBASE_INSTALLATIONS_SRC_DESKTOP_REST_INSTALLATIONS_REQUEST_H_

#include "app/rest/request_json.h"
#include "installations/installations_request_generated.h"
#include "installations/installations_request_resource.h"

namespace firebase {
namespace installations {
namespace internal {

class InstallationsRequest
    : public firebase::rest::RequestJson<fbs::Request, fbs::RequestT> {
 public:
  InstallationsRequest() : InstallationsRequest(installations_request_resource_data) {}
  explicit InstallationsRequest(const char* schema);

  explicit InstallationsRequest(const unsigned char* schema)
      : InstallationsRequest(reinterpret_cast<const char*>(schema)) {}

  void SetFId(std::string fid) {
    application_data_->fid = std::move(fid);
  }

  void SetAuthVersion(std::string auth_version) {
    application_data_->authVersion = std::move(auth_version);
  }

  void SetAppId(std::string app_id) {
    application_data_->appId = std::move(app_id);
  }

  void SetSdkVersion(std::string sdk_version) {
    application_data_->sdkVersion = std::move(sdk_version);
  }

  void UpdatePost() { UpdatePostFields(); }
};
}  // namespace internal
}  // namespace installations
}  // namespace firebase

#endif /* FIREBASE_INSTALLATIONS_SRC_DESKTOP_REST_INSTALLATIONS_REQUEST_H_ */
