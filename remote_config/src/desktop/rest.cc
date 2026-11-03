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
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include "app/rest/transport_builder.h"
#include "app/rest/transport_curl.h"
#include "app/rest/transport_interface.h"
#include "app/rest/util.h"
#include "app/src/app_common.h"
#include "app/src/base64.h"
#include "app/src/locale.h"
#include "app/src/log.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/uuid.h"
#include "firebase/app.h"
#include "remote_config/src/common.h"
#include "remote_config/src/desktop/config_data.h"

namespace firebase {
namespace remote_config {
namespace internal {

// The Java library's FirebaseRemoteConfig.java references this file to keep
// this value in sync.
const int SDK_MAJOR_VERSION = 1;
const int SDK_MINOR_VERSION = 3;
const int SDK_PATCH_VERSION = 0;

RemoteConfigREST::RemoteConfigREST(const firebase::AppOptions& app_options,
                                   LayeredConfigs& configs,
                                   std::string namespaces)
    : app_package_name_(
          app_options.package_name() ? app_options.package_name() : ""),
      app_gmp_project_id_(app_options.app_id() ? app_options.app_id() : ""),
      app_project_id_(app_options.project_id() ? app_options.project_id() : ""),
      api_key_(app_options.api_key() ? app_options.api_key() : ""),
      namespaces_(std::move(namespaces)),
      configs_(configs) {
  rest::util::Initialize();
  firebase::rest::InitTransportCurl();
}

RemoteConfigREST::~RemoteConfigREST() {
  firebase::rest::CleanupTransportCurl();
  rest::util::Terminate();
}

void RemoteConfigREST::Fetch(const App& app,
                             uint64_t fetch_timeout_in_milliseconds) {
  rc_request_ = RemoteConfigRequest();
  rc_response_ = RemoteConfigResponse();

  TryGetInstallationsAndToken(app);

  SetupRestRequest(app, fetch_timeout_in_milliseconds);
  firebase::rest::CreateTransport()->Perform(rc_request_, &rc_response_);
  ParseRestResponse();
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
  if (!app_instance_id_.empty() && !app_instance_id_token_.empty()) {
    return;
  }
  static Mutex* s_id_mutex = new Mutex();
  static auto* s_cached_ids =
      new std::map<std::string, std::pair<std::string, std::string>>();

  std::string cache_key =
      std::string(app.name() ? app.name() : "") + ":" + app_gmp_project_id_;
  MutexLock lock(*s_id_mutex);
  auto it = s_cached_ids->find(cache_key);
  if (it != s_cached_ids->end()) {
    app_instance_id_ = it->second.first;
    app_instance_id_token_ = it->second.second;
    return;
  }
  app_instance_id_ = GenerateFakeId();
  app_instance_id_token_ = GenerateFakeId();
  (*s_cached_ids)[cache_key] =
      std::make_pair(app_instance_id_, app_instance_id_token_);
}

void RemoteConfigREST::SetupRestRequest(
    const App& app, uint64_t fetch_timeout_in_milliseconds) {
  (void)app;
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
  rc_request_.options().timeout_ms = fetch_timeout_in_milliseconds;

  rc_request_.SetAppId(app_gmp_project_id_);
  rc_request_.SetAppInstanceId(app_instance_id_);
  rc_request_.SetAppInstanceIdToken(app_instance_id_token_);

  rc_request_.SetPlatformVersion("2");
  std::string locale = firebase::internal::GetLocale();
  if (!locale.empty()) {
    rc_request_.SetLanguageCode(locale);
  }
  rc_request_.SetTimeZone(firebase::internal::GetTimezone());
  rc_request_.SetPackageName(app_package_name_);
  rc_request_.SetSdkVersion(std::to_string(
      SDK_MAJOR_VERSION * 10000 + SDK_MINOR_VERSION * 100 + SDK_PATCH_VERSION));
  rc_request_.SetCustomSignals(configs_.metadata.custom_signals());

  rc_request_.UpdatePost();
}

void RemoteConfigREST::ParseRestResponse() {
  if (rc_response_.status() == kHTTPStatusTooManyRequests) {
    FetchFailure(kFetchFailureReasonThrottled);
    LogError("fetching failure: throttled (http code 429)");
    return;
  }

  if (rc_response_.status() != kHTTPStatusOk) {
    FetchFailure(kFetchFailureReasonError);
    LogError("fetching failure: http code %d", rc_response_.status());
    return;
  }

  const char* body = rc_response_.GetBody();
  if (!body || std::strlen(body) == 0 || rc_response_.sdk_error_code() != 0) {
    FetchFailure(kFetchFailureReasonError);
    LogError("fetching failure: empty or invalid response body (http code %d)",
             rc_response_.status());
    return;
  }

  const Variant& entries = rc_response_.GetEntries();

  NamespaceKeyValueMap config_map(configs_.fetched.config());
  LogDebug("Parsing config response...");
  if (rc_response_.StatusMatch("NO_CHANGE")) {
    LogDebug("No change");
    if (configs_.fetched.timestamp() == 0) {
      configs_.fetched =
          NamespacedConfigData(config_map, MillisecondsSinceEpoch());
    }
  } else if (rc_response_.StatusMatch("UPDATE")) {
    config_map[namespaces_].clear();
    if (entries.is_map()) {
      for (const auto& keyvalue : entries.map()) {
        if (!keyvalue.first.is_string() || !keyvalue.second.is_string()) {
          continue;
        }
        std::string key =
            keyvalue.first.is_mutable_string()
                ? keyvalue.first.mutable_string()
                : std::string(keyvalue.first.string_value()
                                  ? keyvalue.first.string_value()
                                  : "");
        std::string val =
            keyvalue.second.is_mutable_string()
                ? keyvalue.second.mutable_string()
                : std::string(keyvalue.second.string_value()
                                  ? keyvalue.second.string_value()
                                  : "");
        config_map[namespaces_][key] = val;
        LogDebug("Update: ns=%s kv=(%s, %s)", namespaces_.c_str(), key.c_str(),
                 val.c_str());
      }
    }
    configs_.fetched =
        NamespacedConfigData(config_map, MillisecondsSinceEpoch());
  } else if (rc_response_.StatusMatch("NO_TEMPLATE")) {
    LogDebug("NotAuthorized: ns=%s", namespaces_.c_str());
    config_map.erase(namespaces_);
    configs_.fetched =
        NamespacedConfigData(config_map, MillisecondsSinceEpoch());
  } else if (rc_response_.StatusMatch("EMPTY_CONFIG")) {
    LogDebug("EmptyConfig: ns=%s", namespaces_.c_str());
    config_map[namespaces_].clear();
    configs_.fetched =
        NamespacedConfigData(config_map, MillisecondsSinceEpoch());
  } else {
    FetchFailure(kFetchFailureReasonError);
    LogError("fetching failure: unrecognized response state");
    return;
  }

  FetchSuccess(kLastFetchStatusSuccess);
}

void RemoteConfigREST::FetchSuccess(LastFetchStatus status) {
  ConfigInfo info(configs_.metadata.info());
  info.last_fetch_status = status;
  info.last_fetch_failure_reason = kFetchFailureReasonInvalid;
  info.throttled_end_time = 0;
  info.fetch_time = MillisecondsSinceEpoch();
  configs_.metadata.set_info(info);
}

void RemoteConfigREST::FetchFailure(FetchFailureReason reason) {
  ConfigInfo info(configs_.metadata.info());
  uint64_t now = MillisecondsSinceEpoch();
  info.last_fetch_failure_reason = reason;
  if (reason == kFetchFailureReasonThrottled) {
    info.throttled_end_time =
        now + kDefaultCacheExpiration *
                  ::firebase::internal::kMillisecondsPerSecond;
  }
  info.last_fetch_status = kLastFetchStatusFailure;
  info.fetch_time = now;
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
