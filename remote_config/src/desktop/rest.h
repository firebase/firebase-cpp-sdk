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

#ifndef FIREBASE_REMOTE_CONFIG_SRC_DESKTOP_REST_H_
#define FIREBASE_REMOTE_CONFIG_SRC_DESKTOP_REST_H_

#include <cstdint>

#include "app/rest/request_json.h"
#include "app/rest/response_json.h"
#include "app/src/semaphore.h"
#include "firebase/app.h"
#include "remote_config/request_generated.h"
#include "remote_config/request_resource.h"
#include "remote_config/response_generated.h"
#include "remote_config/response_resource.h"
#include "remote_config/src/desktop/config_data.h"
#include "remote_config/src/desktop/remote_config_request.h"
#include "remote_config/src/desktop/remote_config_response.h"

#ifdef FIREBASE_TESTING
#include "gtest/gtest.h"
#endif  // FIREBASE_TESTING

namespace firebase {
namespace remote_config {
namespace internal {

// These are used in tests to build a regular protobuf for comparison.
extern const int SDK_MAJOR_VERSION;
extern const int SDK_MINOR_VERSION;
extern const int SDK_PATCH_VERSION;

// These are structures used to build a request.
struct ConfigFetchRequest;
struct PackageData;

const char* const kServerURL =
    "https://firebaseremoteconfig.googleapis.com/v1/projects";
const char* const kHTTPMethodPost = "POST";
const char* const kContentTypeHeaderName = "Content-Type";
const char* const kAcceptHeaderName = "Accept";
const char* const kContentTypeValue = "application/x-protobuffer";
const char* const kJSONContentTypeValue = "application/json";

// Set this key with value `1` if settings[kConfigSettingDeveloperMode] == `1`
const char* const kDeveloperModeKey = "_rcn_developer";

const int kHTTPStatusOk = 200;

// const char* const kApiKeyHeader = "X-Goog-Api-Key";
const char* const kEtagHeader = "ETag";
const char* const kIfNoneMatchHeader = "If-None-Match";
const char* const kXGoogleGfeCanRetry = "X-Google-GFE-Can-Retry";
const char* const kInstallationsAuthTokenHeader =
    "X-Goog-Firebase-Installations-Auth";
const char* const kHTTPFetchKeyString = ":fetch?key=";
const char* const kNameSpaceString = "namespaces";

class RemoteConfigREST {
 public:
#ifdef FIREBASE_TESTING
  friend class RemoteConfigRESTTest;
  FRIEND_TEST(RemoteConfigRESTTest, Setup);
  FRIEND_TEST(RemoteConfigRESTTest, SetupRESTRequest);
  FRIEND_TEST(RemoteConfigRESTTest, Fetch);
  FRIEND_TEST(RemoteConfigRESTTest, ParseRestResponseProtoFailure);
  FRIEND_TEST(RemoteConfigRESTTest, ParseRestResponseSuccess);
#endif  // FIREBASE_TESTING

  RemoteConfigREST(const firebase::AppOptions& app_options,
                   const LayeredConfigs& configs, const std::string namespaces);

  ~RemoteConfigREST();

  // 1. Attempt to Fetch Installation and Auth Token.
  // 2. Setup REST request;
  // 3. Make REST request;
  // 4. Parse REST response.
  void Fetch(const App& app, uint64_t fetch_timeout_in_milliseconds);

  // After Fetch() will return updated fetched holder. Otherwise will return not
  // updated fetched holder.
  const NamespacedConfigData& fetched() const { return configs_.fetched; }

  // After Fetch() will return updated metadata. Otherwise will return not
  // updated metadata.
  const RemoteConfigMetadata& metadata() const { return configs_.metadata; }

 private:
  // Attempt to get Installations and Auth Token from app synchronously.  This
  // will block the current thread and wait until the futures are complete.
  void TryGetInstallationsAndToken(const App& app);

  // Setup all values to make REST request. Call `SetupProtoRequest` to setup
  // post fields.
  void SetupRestRequest(const App& app, uint64_t fetch_timeout_in_milliseconds);

  // Parse REST response. Check response status and body.
  void ParseRestResponse();

  // Return timestamp in milliseconds.
  uint64_t MillisecondsSinceEpoch();

  // Update metadata after successful fetching.
  void FetchSuccess(LastFetchStatus status);

  // Update metadata after failed fetching.
  void FetchFailure(FetchFailureReason reason);

  // app fields:
  std::string app_package_name_;
  std::string app_gmp_project_id_;
  std::string app_project_id_;
  std::string api_key_;
  std::string namespaces_;

  LayeredConfigs configs_;

  // Instance Id data
  std::string app_instance_id_;
  std::string app_instance_id_token_;

  // The semaphore to block the thread and wait for.
  Semaphore fetch_future_sem_;

  RemoteConfigRequest rc_request_;
  RemoteConfigResponse rc_response_;
};

}  // namespace internal
}  // namespace remote_config
}  // namespace firebase

#endif  // FIREBASE_REMOTE_CONFIG_SRC_DESKTOP_REST_H_
