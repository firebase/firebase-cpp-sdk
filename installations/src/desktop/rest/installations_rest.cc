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

#include "installations/src/desktop/rest/installations_rest.h"

#include "app/rest/transport_builder.h"
#include "app/rest/transport_curl.h"
#include "app/rest/transport_interface.h"
#include "app/rest/util.h"

namespace firebase {
namespace installations {
namespace internal {

InstallationsREST::InstallationsREST(const firebase::AppOptions& app_options)
    : app_package_name_(app_options.package_name()),
      app_gmp_project_id_(app_options.app_id()),
      app_project_id_(app_options.project_id()),
      api_key_(app_options.api_key()) {
  rest::util::Initialize();
  firebase::rest::InitTransportCurl();
}

InstallationsREST::~InstallationsREST() {
  firebase::rest::CleanupTransportCurl();
  rest::util::Terminate();
}

void InstallationsREST::RegisterInstallations(const App& app) {

  SetupRestRequest(app);
  std::string request_str = fis_request_.ToString();
  LogDebug(request_str.c_str());
  firebase::rest::CreateTransport()->Perform(fis_request_, &fis_response_);
  ParseRestResponse();
}

void InstallationsREST::SetupRestRequest(const App& app) {
  std::string server_url(kServerURL);
  server_url.append("/");
  server_url.append("605833183374");
  server_url.append("/");
  server_url.append(kInstallationsName);
  server_url.append("/");

  fis_request_.set_url(server_url.c_str());
  fis_request_.set_method(kHTTPMethodPost);
  fis_request_.add_header(kContentTypeHeaderName, kJSONContentTypeValue);
  fis_request_.add_header(kAcceptHeaderName, kJSONContentTypeValue);
  fis_request_.add_header(kContentEncodingName, kGzipContentEncoding);
  fis_request_.add_header(kXGoogleApiKeyName, api_key_.c_str());
  //fis_request_.options().timeout_ms = fetch_timeout_in_milliseconds;

  fis_request_.SetFId("sdfweofdvnad");
  fis_request_.SetAppId(app_gmp_project_id_);
  fis_request_.SetAuthVersion(kAuthVersion);
  fis_request_.SetSdkVersion(kInstallationsSDKVersion);
  

  fis_request_.UpdatePost();
}

void InstallationsREST::ParseRestResponse() {
  if (fis_response_.status() != kHTTPStatusOk) {
    
    LogError("Installations Request failure: http code %d", fis_response_.status());
    return;
  }

  if (strlen(fis_response_.GetBody()) == 0) {
    
    LogError("Installations Request failure: http code %d", fis_response_.status());
    return;
  }

  installations_id_ = fis_response_.GetFid();

  
}

uint64_t InstallationsREST::MillisecondsSinceEpoch() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

}  // namespace internal
}  // namespace installations
}  // namespace firebase