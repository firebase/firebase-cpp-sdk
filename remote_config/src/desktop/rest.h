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

#ifndef FIREBASE_REMOTE_CONFIG_CLIENT_CPP_SRC_DESKTOP_REST_H_
#define FIREBASE_REMOTE_CONFIG_CLIENT_CPP_SRC_DESKTOP_REST_H_

#include <cstdint>

#include "firebase/app.h"
#include "app/src/semaphore.h"
#ifndef REST_STUB_IMPL  // These pull in unnecessary deps for the stub
#include "app/rest/request_binary_gzip.h"
#include "app/rest/response_binary.h"
#endif  // REST_STUB_IMPL
#include "remote_config/src/desktop/config_data.h"

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

#ifdef DEBUG_SERVER
const char* const kServerURL = "https://jmt17.google.com/config";
#else
const char* const kServerURL = "https://cloudconfig.googleapis.com/config";
#endif

const char* const kHTTPMethodPost = "POST";
const char* const kContentTypeHeaderName = "Content-Type";
const char* const kContentTypeValue = "application/x-protobuffer";

// Set this key with value `1` if settings[kConfigSettingDeveloperMode] == `1`
const char* const kDeveloperModeKey = "_rcn_developer";

const int kHTTPStatusOk = 200;

class RemoteConfigREST {
 public:
#ifdef FIREBASE_TESTING
  friend class RemoteConfigRESTTest;
  FRIEND_TEST(RemoteConfigRESTTest, SetupProto);
  FRIEND_TEST(RemoteConfigRESTTest, SetupRESTRequest);
  FRIEND_TEST(RemoteConfigRESTTest, Fetch);
  FRIEND_TEST(RemoteConfigRESTTest, ParseRestResponseProtoFailure);
  FRIEND_TEST(RemoteConfigRESTTest, ParseRestResponseSuccess);
#endif  // FIREBASE_TESTING

  RemoteConfigREST(const firebase::AppOptions& app_options,
                   const LayeredConfigs& configs,
                   uint64_t cache_expiration_in_seconds);

  ~RemoteConfigREST();

  // 1. Attempt to Fetch Instance Id and token.  App is required to get an
  //    instance of InstanceIdDesktopImpl.
  // 2. Setup REST request;
  // 3. Make REST request;
  // 4. Parse REST response.
  void Fetch(const App& app);

  // After Fetch() will return updated fetched holder. Otherwise will return not
  // updated fetched holder.
  const NamespacedConfigData& fetched() const { return configs_.fetched; }

  // After Fetch() will return updated metadata. Otherwise will return not
  // updated metadata.
  const RemoteConfigMetadata& metadata() const { return configs_.metadata; }

 private:
  // Attempt to get Instance Id and token from app synchronously.  This will
  // block the current thread and wait until the futures are complete.
  void TryGetInstanceIdAndToken(const App& app);

  // Setup all values to make REST request. Call `SetupProtoRequest` to setup
  // post fields.
  void SetupRestRequest();

  // Setup protobuf ConfigFetchRequest object for REST request post fields. Call
  // `GetPackageData` to setup PackageData for ConfigFetchRequest.
  ConfigFetchRequest GetFetchRequestData();

  // Setup PackageData for ConfigFetchRequest.
  void GetPackageData(PackageData* package_data);

  // Parse REST response. Check response status and body.
  void ParseRestResponse();

  // Parse REST response body `proto_str` to ConfigFetchResponse protobuf
  // object. Update `configs_` variable based on ConfigFetchResponse.
  void ParseProtoResponse(const std::string& proto_str);

  // Return timestamp in milliseconds.
  uint64_t MillisecondsSinceEpoch();

  // Update metadata after successful fetching.
  void FetchSuccess(LastFetchStatus status);

  // Update metadata after failed fetching.
  void FetchFailure(FetchFailureReason reason);

  // app fields:
  std::string app_package_name_;
  std::string app_gmp_project_id_;

  LayeredConfigs configs_;

  // cache expiration
  uint64_t cache_expiration_in_seconds_;

  // Instance Id data
  std::string app_instance_id_;
  std::string app_instance_id_token_;

  // The semaphore to block the thread and wait for.
  Semaphore fetch_future_sem_;

#ifndef REST_STUB_IMPL
  // HTTP request/response
  firebase::rest::RequestBinaryGzip rest_request_;
  firebase::rest::ResponseBinary rest_response_;
#endif  // REST_STUB_IMPL
};

}  // namespace internal
}  // namespace remote_config
}  // namespace firebase

#endif  // FIREBASE_REMOTE_CONFIG_CLIENT_CPP_SRC_DESKTOP_REST_H_
