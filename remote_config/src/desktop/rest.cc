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

#include "remote_config/src/desktop/rest.h"

#include <chrono>  // NOLINT
#include <cstdint>
#include <memory>
#include <sstream>

#include "firebase/app.h"
#include "app/instance_id/instance_id_desktop_impl.h"
#include "app/meta/move.h"
#include "app/rest/request_binary.h"
#include "app/rest/response_binary.h"
#include "app/rest/transport_builder.h"
#include "app/rest/transport_curl.h"
#include "app/rest/transport_interface.h"
#include "app/rest/util.h"
#include "app/src/app_common.h"
#include "app/src/base64.h"
#include "app/src/log.h"
#include "app/src/uuid.h"
#include "remote_config/src/desktop/config_data.h"
#include "remote_config/src/desktop/rest_nanopb_decode.h"
#include "remote_config/src/desktop/rest_nanopb_encode.h"

namespace firebase {
namespace remote_config {
namespace internal {

// The Java library's FirebaseRemoteConfig.java references this file to keep
// this value in sync.
const int SDK_MAJOR_VERSION = 1;
const int SDK_MINOR_VERSION = 3;
const int SDK_PATCH_VERSION = 0;

const char kTokenScope[] = "*";

RemoteConfigREST::RemoteConfigREST(const firebase::AppOptions& app_options,
                                   const LayeredConfigs& configs,
                                   const std::string namespaces)
    : app_package_name_(app_options.package_name()),
      app_gmp_project_id_(app_options.app_id()),
      app_project_id_(app_options.project_id()),
      api_key_(app_options.api_key()),
      namespaces_(std::move(namespaces)),
      configs_(configs),
      fetch_future_sem_(0) {
  rest::util::Initialize();
  firebase::rest::InitTransportCurl();
}

RemoteConfigREST::~RemoteConfigREST() {
  firebase::rest::CleanupTransportCurl();
  rest::util::Terminate();
}

void RemoteConfigREST::Fetch(const App& app,
                             uint64_t cache_expiration_in_seconds) {
  cache_expiration_in_seconds_ = cache_expiration_in_seconds;
  TryGetInstallationsAndToken(app);

  SetupRestRequest(app);
  firebase::rest::CreateTransport()->Perform(rc_request_, &rc_response_);
  ParseRestResponse();
}

void WaitForFuture(const Future<std::string>& future, Semaphore* future_sem,
                   std::string* result, const char* action_name) {
  // Block and wait until Future is complete.
  future.OnCompletion(
      [](const firebase::Future<std::string>& result, void* data) {
        Semaphore* sem = static_cast<Semaphore*>(data);
        sem->Post();
      },
      future_sem);
  future_sem->Wait();

  if (future.status() == firebase::kFutureStatusComplete &&
      future.error() ==
          firebase::instance_id::internal::InstanceIdDesktopImpl::kErrorNone) {
    *result = *future.result();
  } else if (future.status() != firebase::kFutureStatusComplete) {
    // It is fine if timeout
    LogWarning("Remote Config Fetch: %s timeout", action_name);
  } else {
    // It is fine if failed
    LogWarning("Remote Config Fetch: Failed to %s. Error %d: %s", action_name,
               future.error(), future.error_message());
  }
}

static std::string GenerateFakeId() {
  firebase::internal::Uuid uuid;
  uuid.Generate();
  // Collapse into 8 bytes, forcing the top 4 bits to be 0x70.
  uint8_t buffer[8];
  buffer[0] = ((uuid.data[0] ^ uuid.data[8]) & 0x0f) | 0x70;
  buffer[1] = uuid.data[1] ^ uuid.data[9];
  buffer[2] = uuid.data[2] ^ uuid.data[10];
  buffer[3] = uuid.data[3] ^ uuid.data[11];
  buffer[4] = uuid.data[4] ^ uuid.data[12];
  buffer[5] = uuid.data[5] ^ uuid.data[13];
  buffer[6] = uuid.data[6] ^ uuid.data[14];
  buffer[7] = uuid.data[7] ^ uuid.data[15];

  std::string input(reinterpret_cast<char*>(buffer), sizeof(buffer));
  std::string output;

  if (firebase::internal::Base64EncodeUrlSafe(input, &output)) {
    return output;
  }
  return "";  // Error encoding.
}

void RemoteConfigREST::TryGetInstallationsAndToken(const App& app) {
  // Convert the app reference stored in RemoteConfigInternal
  // pointer for InstanceIdDesktopImpl.
  App* non_const_app = const_cast<App*>(&app);
  auto* iid_impl =
      firebase::instance_id::internal::InstanceIdDesktopImpl::GetInstance(
          non_const_app);
  if (iid_impl == nullptr) {
    // Instance ID not supported.
    app_instance_id_ = GenerateFakeId();
    app_instance_id_token_ = GenerateFakeId();
    return;
  }

  WaitForFuture(iid_impl->GetId(), &fetch_future_sem_, &app_instance_id_,
                "Get Instance Id");

  // Only get token if instance id is retrieved.
  if (!app_instance_id_.empty()) {
    WaitForFuture(iid_impl->GetToken(kTokenScope), &fetch_future_sem_,
                  &app_instance_id_token_, "Get Instance Id Token");
  }
}

void RemoteConfigREST::SetupRestRequest(const App& app) {
  std::string server_url(kServerURL);
  server_url.append("/");
  server_url.append(app_project_id_);
  server_url.append("/");
  server_url.append(kNameSpaceString);
  server_url.append("/");
  server_url.append(namespaces_);
  server_url.append(kHTTPFetchKeyString);
  server_url.append(api_key_);

  rc_request_.set_url(server_url.c_str());
  rc_request_.set_method(kHTTPMethodPost);
  rc_request_.add_header(kContentTypeHeaderName, kJSONContentTypeValue);
  rc_request_.add_header(kAcceptHeaderName, kJSONContentTypeValue);

  rc_request_.SetAppId(app_gmp_project_id_);
  rc_request_.SetAppInstanceId(
      app_instance_id_);  // TODO(cynthiajiang) change to installations
  rc_request_.SetAppInstanceIdToken(
      app_instance_id_token_);  // TODO(cynthiajiang) change to installations

  rc_request_.SetPlatformVersion("2");
  rc_request_.SetPackageName(app_package_name_);
  rc_request_.SetSdkVersion(std::to_string(
      SDK_MAJOR_VERSION * 10000 + SDK_MINOR_VERSION * 100 + SDK_PATCH_VERSION));

  rc_request_.UpdatePost();
}

ConfigFetchRequest RemoteConfigREST::GetFetchRequestData() {
  ConfigFetchRequest request = ConfigFetchRequest();
  GetPackageData(&request.package_data);

  request.client_version = 2;
  request.device_type = 5;  // DESKTOP
#if FIREBASE_PLATFORM_WINDOWS
  request.device_subtype = 8;  // WINDOWS
#elif FIREBASE_PLATFORM_OSX
  request.device_subtype = 9;  // OS X
#elif FIREBASE_PLATFORM_LINUX
  request.device_subtype = 10;  // LINUX
#else
#error Unknown operating system.
#endif
  return Move(request);
}

void RemoteConfigREST::GetPackageData(PackageData* package_data) {
  package_data->package_name = app_package_name_;
  package_data->gmp_project_id = app_gmp_project_id_;

  package_data->namespace_digest = configs_.metadata.digest_by_namespace();

  // Check if developer mode enable
  if (configs_.metadata.GetSetting(kConfigSettingDeveloperMode) == "1") {
    package_data->custom_variable[kDeveloperModeKey] = "1";
  }

  package_data->app_instance_id = app_instance_id_;
  package_data->app_instance_id_token = app_instance_id_token_;

  package_data->requested_cache_expiration_seconds =
      static_cast<int32_t>(cache_expiration_in_seconds_);

  if (configs_.fetched.timestamp() == 0) {
    package_data->fetched_config_age_seconds = -1;
  } else {
    package_data->fetched_config_age_seconds = static_cast<int32_t>(
        (MillisecondsSinceEpoch() - configs_.fetched.timestamp()) / 1000);
  }

  package_data->sdk_version =
      SDK_MAJOR_VERSION * 10000 + SDK_MINOR_VERSION * 100 + SDK_PATCH_VERSION;

  if (configs_.active.timestamp() == 0) {
    package_data->active_config_age_seconds = -1;
  } else {
    package_data->active_config_age_seconds = static_cast<int32_t>(
        (MillisecondsSinceEpoch() - configs_.active.timestamp()) / 1000);
  }
}

void RemoteConfigREST::ParseRestResponse() {
  if (rc_response_.status() != kHTTPStatusOk) {
    FetchFailure(kFetchFailureReasonError);
    LogError("fetching failure: http code %d", rc_response_.status());
    return;
  }

  Variant entries = rc_response_.GetEntries();

  NamespaceKeyValueMap config_map(configs_.fetched.config());
  LogDebug("Parsing config response...");
  if (rc_response_.StatusMatch("NO_CHANGE")) {
    LogDebug("No change");
  } else if (rc_response_.StatusMatch("UPDATE")) {
    config_map[namespaces_].clear();
    for (const auto& keyvalue : entries.map()) {
      config_map[namespaces_][keyvalue.first.mutable_string()] =
          keyvalue.second.mutable_string();
      LogDebug("Update: ns=%s kv=(%s, %s)", namespaces_.c_str(),
               keyvalue.first.mutable_string().c_str(),
               keyvalue.second.mutable_string().c_str());
    }
  } else if (rc_response_.StatusMatch("NO_TEMPLATE")) {
    LogDebug("NotAuthorized: ns=%s", namespaces_.c_str());
    config_map.erase(namespaces_);
  } else if (rc_response_.StatusMatch("EMPTY_CONFIG")) {
    LogDebug("EmptyConfig: ns=%s", namespaces_.c_str());
    config_map[namespaces_].clear();
  }

  configs_.fetched = NamespacedConfigData(config_map, MillisecondsSinceEpoch());
  FetchSuccess(kLastFetchStatusSuccess);
}

void RemoteConfigREST::FetchSuccess(LastFetchStatus status) {
  ConfigInfo info(configs_.metadata.info());
  info.last_fetch_status = status;
  info.fetch_time = MillisecondsSinceEpoch();
  configs_.metadata.set_info(info);
}

void RemoteConfigREST::FetchFailure(FetchFailureReason reason) {
  ConfigInfo info(configs_.metadata.info());
  info.last_fetch_failure_reason = reason;
  info.throttled_end_time = MillisecondsSinceEpoch();
  info.last_fetch_status = kLastFetchStatusFailure;
  info.fetch_time = MillisecondsSinceEpoch();
  configs_.metadata.set_info(info);
}

uint64_t RemoteConfigREST::MillisecondsSinceEpoch() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

}  // namespace internal
}  // namespace remote_config
}  // namespace firebase
