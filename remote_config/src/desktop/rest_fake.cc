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

#include <chrono>  // NOLINT
#include <cstdint>
#include <memory>

#include "firebase/app.h"
#include "remote_config/src/desktop/config_data.h"
#include "remote_config/src/desktop/rest.h"

namespace firebase {
namespace remote_config {
namespace internal {

// Stub REST implementation.
// The purpose of this class is to hold content and not actually do anything
// with it when the normal API calls happen.
RemoteConfigREST::RemoteConfigREST(const firebase::AppOptions& app_options,
                                   const LayeredConfigs& configs,
                                   const std::string namespaces)
    : app_package_name_(app_options.app_id()),
      app_gmp_project_id_(app_options.project_id()),
      app_project_id_(app_options.project_id()),
      api_key_(app_options.api_key()),
      namespaces_(std::move(namespaces)),
      configs_(configs),
      fetch_future_sem_(0) {
  configs_.fetched = NamespacedConfigData(
      NamespaceKeyValueMap({{"namespace", {{"key", "value"}}}}), 1000000);

  configs_.metadata.set_info(
      ConfigInfo{0, kLastFetchStatusSuccess, kFetchFailureReasonError, 0});
  configs_.metadata.set_digest_by_namespace(
      MetaDigestMap({{"namespace", "digest"}}));
}

RemoteConfigREST::~RemoteConfigREST() {}

void RemoteConfigREST::Fetch(const App& app,
                             uint64_t fetch_timeout_in_milliseconds) {}

void RemoteConfigREST::SetupRestRequest(
    const App& app, uint64_t fetch_timeout_in_milliseconds) {}

void RemoteConfigREST::ParseRestResponse() {}

void RemoteConfigREST::FetchSuccess(LastFetchStatus status) {}

void RemoteConfigREST::FetchFailure(FetchFailureReason reason) {}

uint64_t RemoteConfigREST::MillisecondsSinceEpoch() { return 0; }

}  // namespace internal
}  // namespace remote_config
}  // namespace firebase
