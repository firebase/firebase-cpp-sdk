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
#include <string>

#include "app/rest/request_json.h"
#include "app/rest/response_json.h"
#include "app/src/include/firebase/internal/mutex.h"
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

namespace firebase::remote_config::internal {

// SDK version numbers reported in REST fetch requests.
extern const int SDK_MAJOR_VERSION;
extern const int SDK_MINOR_VERSION;
extern const int SDK_PATCH_VERSION;

inline constexpr const char* kServerURL =
    "https://firebaseremoteconfig.googleapis.com/v1/projects";
inline constexpr const char* kHTTPMethodPost = "POST";
inline constexpr const char* kContentTypeHeaderName = "Content-Type";
inline constexpr const char* kAcceptHeaderName = "Accept";
inline constexpr const char* kJSONContentTypeValue = "application/json";

inline constexpr int kHTTPStatusOk = 200;
inline constexpr int kHTTPStatusTooManyRequests = 429;

inline constexpr const char* kHTTPFetchKeyString = ":fetch?key=";
inline constexpr const char* kNameSpaceString = "namespaces";

class RemoteConfigREST {
 public:
#ifdef FIREBASE_TESTING
  friend class RemoteConfigRESTTest;
  FRIEND_TEST(RemoteConfigRESTTest, Setup);
  FRIEND_TEST(RemoteConfigRESTTest, SetupRESTRequest);
  FRIEND_TEST(RemoteConfigRESTTest, SetupRESTRequestWithCustomSignals);
  FRIEND_TEST(RemoteConfigRESTTest, Fetch);
  FRIEND_TEST(RemoteConfigRESTTest, ParseRestResponseProtoFailure);
  FRIEND_TEST(RemoteConfigRESTTest, ParseRestResponseSuccess);
  FRIEND_TEST(RemoteConfigRESTTest, FetchThrottled429SetsThrottledEndTime);
  FRIEND_TEST(RemoteConfigRESTTest, ParseRestResponseNoChangePreservesEntries);
#endif  // FIREBASE_TESTING

  RemoteConfigREST(const firebase::AppOptions& app_options,
                   LayeredConfigs& configs, std::string namespaces);

  ~RemoteConfigREST();

  // 1. Attempt to Fetch Installation and Auth Token.
  // 2. Setup REST request;
  // 3. Make REST request;
  // 4. Parse REST response.
  void Fetch(const App& app, uint64_t fetch_timeout_in_milliseconds);

  // After Fetch() will return updated fetched holder. Otherwise will return not
  // updated fetched holder.
  [[nodiscard]] const NamespacedConfigData& fetched() const {
    return configs_.fetched;
  }

  // After Fetch() will return updated metadata. Otherwise will return not
  // updated metadata.
  [[nodiscard]] const RemoteConfigMetadata& metadata() const {
    return configs_.metadata;
  }

 private:
  // Obtain a stable installation ID and auth token for the app.
  void TryGetInstallationsAndToken(const App& app);

  // Populate REST request headers, URL, and JSON payload.
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

  LayeredConfigs& configs_;

  // Instance Id data
  std::string app_instance_id_;
  std::string app_instance_id_token_;

  mutable Mutex mutex_;
  RemoteConfigRequest rc_request_;
  RemoteConfigResponse rc_response_;
  Semaphore fetch_future_sem_;
};

}  // namespace firebase::remote_config::internal

#endif  // FIREBASE_REMOTE_CONFIG_SRC_DESKTOP_REST_H_
