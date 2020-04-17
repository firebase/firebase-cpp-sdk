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
#include "app/src/log.h"
#include "remote_config/src/desktop/config_data.h"
#include "remote_config/src/desktop/rest_nanopb_decode.h"
#include "remote_config/src/desktop/rest_nanopb_encode.h"

// This macro is only used to shorten the enum names, for readability.
#define CONFIG_NAMESPACESTATUS(VALUE) \
  desktop_config_AppNamespaceConfigTable_NamespaceStatus_##VALUE

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
                                   uint64_t cache_expiration_in_seconds)
    : app_package_name_(app_options.package_name()),
      app_gmp_project_id_(app_options.app_id()),
      configs_(configs),
      cache_expiration_in_seconds_(cache_expiration_in_seconds),
      fetch_future_sem_(0) {
  rest::util::Initialize();
  firebase::rest::InitTransportCurl();
}

RemoteConfigREST::~RemoteConfigREST() {
  firebase::rest::CleanupTransportCurl();
  rest::util::Terminate();
}

void RemoteConfigREST::Fetch(const App& app) {
  TryGetInstanceIdAndToken(app);
  SetupRestRequest();
  firebase::rest::CreateTransport()->Perform(rest_request_, &rest_response_);
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

void RemoteConfigREST::TryGetInstanceIdAndToken(const App& app) {
  // Convert the app reference stored in RemoteConfigInternal
  // pointer for InstanceIdDesktopImpl.
  App* non_const_app = const_cast<App*>(&app);
  auto* iid_impl =
      firebase::instance_id::internal::InstanceIdDesktopImpl::GetInstance(
          non_const_app);
  if (iid_impl == nullptr) {
    // Instance ID not supported.
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

void RemoteConfigREST::SetupRestRequest() {
  ConfigFetchRequest config_fetch_request = GetFetchRequestData();

#ifdef DEBUG_SERVER
  rest_request_.set_verbose(true);
#endif  // DEBUG_SERVER
  rest_request_.set_url(kServerURL);
  rest_request_.set_method(kHTTPMethodPost);
  rest_request_.add_header(kContentTypeHeaderName, kContentTypeValue);
  rest_request_.add_header(app_common::kApiClientHeader, App::GetUserAgent());

  std::string proto_str = EncodeFetchRequest(config_fetch_request);
  rest_request_.set_post_fields(proto_str.data(), proto_str.length());
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
  if (rest_response_.status() != kHTTPStatusOk) {
    FetchFailure(kFetchFailureReasonError);
    LogError("fetching failure: http code %d", rest_response_.status());
    LogDebug("Response body:\n%s", rest_response_.rest::Response::GetBody());
    return;
  }

  rest_response_.set_use_gunzip(true);
  const char* data;
  size_t size;
  rest_response_.GetBody(&data, &size);
  std::string body(data, size);
  if (body == "") {
    FetchFailure(kFetchFailureReasonError);
    LogError("fetching failure: empty body");
    return;
  }
  ParseProtoResponse(body);
}

void RemoteConfigREST::ParseProtoResponse(const std::string& proto_str) {
  ConfigFetchResponse response;
  if (!DecodeResponse(&response, proto_str)) {
    FetchFailure(kFetchFailureReasonError);
    LogError("protobuf parsing error");
    return;
  }

  MetaDigestMap meta_digest;
  // Start with a copy of the fetched config state.
  NamespaceKeyValueMap config_map(configs_.fetched.config());

  LogDebug("Parsing config response...");
  for (const auto& app_config : response.configs) {
    LogDebug("Found response config checking app name %s vs %s",
             app_package_name_.c_str(), app_config.app_name.c_str());
    // Check the same app name.
    if (app_package_name_.compare(app_config.app_name) != 0) continue;

    LogDebug("Parsing config for app...");
    for (const auto& config : app_config.ns_configs) {
      switch (config.status) {
        case CONFIG_NAMESPACESTATUS(NO_CHANGE):
          meta_digest[config.config_namespace] = config.digest;
          LogDebug("No change: ns=%s digest=%s",
                   config.config_namespace.c_str(), config.digest.c_str());
          break;
        case CONFIG_NAMESPACESTATUS(UPDATE):
          meta_digest[config.config_namespace] = config.digest;
          config_map[config.config_namespace].clear();
          for (const auto& keyvalue : config.key_values) {
            config_map[config.config_namespace][keyvalue.key] = keyvalue.value;
            LogDebug("Update: ns=%s kv=(%s, %s)",
                     config.config_namespace.c_str(), keyvalue.key.c_str(),
                     keyvalue.value.c_str());
          }
          break;
        case CONFIG_NAMESPACESTATUS(NO_TEMPLATE):
        case CONFIG_NAMESPACESTATUS(NOT_AUTHORIZED):
          LogDebug("NotAuthorized: ns=%s", config.config_namespace.c_str());
          meta_digest.erase(config.config_namespace);
          config_map.erase(config.config_namespace);
          break;
        case CONFIG_NAMESPACESTATUS(EMPTY_CONFIG):
          LogDebug("EmptyConfig: ns=%s", config.config_namespace.c_str());
          meta_digest[config.config_namespace] = config.digest;
          config_map[config.config_namespace].clear();
          break;
      }
    }
  }

  configs_.metadata.set_digest_by_namespace(meta_digest);
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
