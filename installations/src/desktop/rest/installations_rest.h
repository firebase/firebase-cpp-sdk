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

#ifndef FIREBASE_INSTALLATIONS_SRC_DESKTOP_REST_INSTALLATIONS_REST_H_
#define FIREBASE_INSTALLATIONS_SRC_DESKTOP_REST_INSTALLATIONS_REST_H_

#include <cstdint>

#include "app/rest/request_json.h"
#include "app/rest/response_json.h"
#include "app/src/semaphore.h"
#include "firebase/app.h"
#include "installations/installations_request_generated.h"
#include "installations/installations_request_resource.h"
#include "installations/installations_response_generated.h"
#include "installations/installations_response_resource.h"
#include "installations/src/desktop/rest/installations_request.h"
#include "installations/src/desktop/rest/installations_response.h"

namespace firebase {
namespace installations {
namespace internal {


const char* const kServerURL =
    "https://firebaseinstallations.googleapis.com/v1/projects";
const char* const kHTTPMethodPost = "POST";
const char* const kContentTypeHeaderName = "content-type";
const char* const kAcceptHeaderName = "Accept";
const char* const kContentTypeValue = "application/x-protobuffer";
const char* const kJSONContentTypeValue = "application/json";
const char* const kContentEncodingName = "Content-Encoding";
const char* const kGzipContentEncoding = "gzip";


const int kHTTPStatusOk = 200;

const char* const kEtagHeader = "ETag";
const char* const kIfNoneMatchHeader = "If-None-Match";
const char* const kXGoogleGfeCanRetry = "X-Google-GFE-Can-Retry";


const char* const kXGoogleApiKeyName = "x-goog-api-key";

const char* const kInstallationsName = "installations";
const char* const kGenerateAuthToken = "authTokens:generate";
const char* const kAuthVersion = "FIS_v2";
const char* const kInstallationsSDKVersion = "t:0.9";

class InstallationsREST {
 public:

  InstallationsREST(const firebase::AppOptions& app_options);

  ~InstallationsREST();

  void RegisterInstallations(const App& app);

  std::string GetFid() { return installations_id_; }

 private:
  // Setup all values to make REST request. Call `SetupProtoRequest` to setup
  // post fields.
  void SetupRestRequest(const App& app);

  // Parse REST response. Check response status and body.
  void ParseRestResponse();

  // Return timestamp in milliseconds.
  uint64_t MillisecondsSinceEpoch();

  // app fields:
  std::string app_package_name_;
  std::string app_gmp_project_id_;
  std::string app_project_id_;
  std::string api_key_;
  std::string project_number_;

  std::string installations_id_;

  InstallationsRequest fis_request_;
  InstallationsResponse fis_response_;
};

}  // namespace internal
}  // namespace installations
}  // namespace firebase

#endif /* FIREBASE_INSTALLATIONS_SRC_DESKTOP_REST_INSTALLATIONS_REST_H_ */
