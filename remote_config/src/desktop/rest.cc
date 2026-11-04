#include "remote_config/src/desktop/rest.h"
#include <chrono>
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
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/locale.h"
#include "app/src/log.h"
#include "app/src/uuid.h"
#include "firebase/app.h"
#include "flatbuffers/stl_emulation.h"
#include "remote_config/src/common.h"
#include "remote_config/src/desktop/config_data.h"
namespace firebase::remote_config::internal {
namespace {
std::string UrlEncodeComponent(const std::string& input) {
  static constexpr char kHex[] = "0123456789ABCDEF";
  std::string out;
  out.reserve(input.size());
  for (unsigned char c : input) {
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_' ||
        c == '~') {
      out.push_back(static_cast<char>(c));
    } else {
      out.push_back('%');
      out.push_back(kHex[(c >> 4) & 0x0F]);
      out.push_back(kHex[c & 0x0F]);
    }
  }
  return out;
}
std::string SafeCString(const char* str) { return str ? str : ""; }
}  // namespace
const int SDK_MAJOR_VERSION = 1;
const int SDK_MINOR_VERSION = 3;
const int SDK_PATCH_VERSION = 0;
RemoteConfigREST::RemoteConfigREST(const firebase::AppOptions& app_options,
                                   LayeredConfigs& configs,
                                   std::string namespaces)
    : app_package_name_(SafeCString(app_options.package_name())),
      app_gmp_project_id_(SafeCString(app_options.app_id())),
      app_project_id_(SafeCString(app_options.project_id())),
      api_key_(SafeCString(app_options.api_key())),
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
                             uint64_t fetch_timeout_in_milliseconds) {
  RemoteConfigRequest request;
  RemoteConfigResponse response;
  TryGetInstallationsAndToken(app);
  SetupRestRequest(app, fetch_timeout_in_milliseconds, &request);
  firebase::rest::CreateTransport()->Perform(request, &response);
  ParseRestResponse(response);
  rc_request_ = std::move(request);
  rc_response_ = std::move(response);
}
static std::string GenerateFakeId() {
  firebase::internal::Uuid uuid;
  uuid.Generate();
  uint8_t buffer[8];
  buffer[0] = ((uuid.data[0] ^ uuid.data[8]) & 0x0f) | 0x70;
  for (int i = 1; i < 8; ++i) buffer[i] = uuid.data[i] ^ uuid.data[i + 8];
  std::string input(reinterpret_cast<char*>(buffer), sizeof(buffer));
  std::string output;
  return firebase::internal::Base64EncodeUrlSafe(input, &output) ? output : "";
}
void RemoteConfigREST::TryGetInstallationsAndToken(const App& app) {
  if (!app_instance_id_.empty() && !app_instance_id_token_.empty()) return;
  static auto& s_id_mutex = *new Mutex();
  static auto& s_cached_ids =
      *new std::map<std::string, std::pair<std::string, std::string>>();
  std::string cache_key =
      std::string(app.name() ? app.name() : "") + ":" + app_gmp_project_id_;
  MutexLock lock(s_id_mutex);
  if (auto it = s_cached_ids.find(cache_key); it != s_cached_ids.end()) {
    app_instance_id_ = it->second.first;
    app_instance_id_token_ = it->second.second;
    return;
  }
  app_instance_id_ = GenerateFakeId();
  app_instance_id_token_ = GenerateFakeId();
  s_cached_ids[cache_key] = {app_instance_id_, app_instance_id_token_};
}
void RemoteConfigREST::SetupRestRequest(
    const App& app, uint64_t fetch_timeout_in_milliseconds) {
  SetupRestRequest(app, fetch_timeout_in_milliseconds, &rc_request_);
}
void RemoteConfigREST::SetupRestRequest(const App& app,
                                        uint64_t fetch_timeout_in_milliseconds,
                                        RemoteConfigRequest* request) {
  (void)app;
  if (!request) return;
  std::string server_url = std::string(kServerURL) + "/" +
                           UrlEncodeComponent(app_project_id_) + "/" +
                           kNameSpaceString + "/" +
                           UrlEncodeComponent(namespaces_) +
                           kHTTPFetchKeyString + UrlEncodeComponent(api_key_);
  request->set_url(server_url.c_str());
  request->set_method(kHTTPMethodPost);
  request->add_header(kContentTypeHeaderName, kJSONContentTypeValue);
  request->add_header(kAcceptHeaderName, kJSONContentTypeValue);
  request->options().timeout_ms = fetch_timeout_in_milliseconds;
  request->SetAppId(app_gmp_project_id_);
  request->SetAppInstanceId(app_instance_id_);
  request->SetAppInstanceIdToken(app_instance_id_token_);
  request->SetPlatformVersion("2");
  if (std::string locale = firebase::internal::GetLocale(); !locale.empty()) {
    request->SetLanguageCode(locale);
  }
  request->SetTimeZone(firebase::internal::GetTimezone());
  request->SetPackageName(app_package_name_);
  request->SetSdkVersion(std::to_string(
      SDK_MAJOR_VERSION * 10000 + SDK_MINOR_VERSION * 100 + SDK_PATCH_VERSION));
  request->SetCustomSignals(configs_.metadata.custom_signals());
  request->UpdatePost();
}
void RemoteConfigREST::ParseRestResponse() { ParseRestResponse(rc_response_); }
void RemoteConfigREST::ParseRestResponse(const RemoteConfigResponse& response) {
  if (response.status() == kHTTPStatusTooManyRequests) {
    FetchFailure(kFetchFailureReasonThrottled);
    LogError("fetching failure: throttled (http code 429)");
    return;
  }
  if (response.status() != kHTTPStatusOk) {
    FetchFailure(kFetchFailureReasonError);
    LogError("fetching failure: http code %d", response.status());
    return;
  }
  const char* body = response.GetBody();
  if (!body || std::strlen(body) == 0 || response.sdk_error_code() != 0) {
    FetchFailure(kFetchFailureReasonError);
    LogError("fetching failure: empty or invalid response body (http code %d)",
             response.status());
    return;
  }
  const Variant& entries = response.GetEntries();
  NamespaceKeyValueMap config_map(configs_.fetched.config());
  if (response.StatusMatch("NO_CHANGE")) {
    if (configs_.fetched.timestamp() == 0) {
      configs_.fetched =
          NamespacedConfigData(config_map, MillisecondsSinceEpoch());
    }
  } else if (response.StatusMatch("UPDATE")) {
    config_map[namespaces_].clear();
    if (entries.is_map()) {
      for (const auto& [key_var, val_var] : entries.map()) {
        if (!key_var.is_string() || !val_var.is_string()) continue;
        std::string key = key_var.is_mutable_string()
                              ? key_var.mutable_string()
                              : SafeCString(key_var.string_value());
        std::string val = val_var.is_mutable_string()
                              ? val_var.mutable_string()
                              : SafeCString(val_var.string_value());
        config_map[namespaces_][key] = val;
      }
    }
    configs_.fetched =
        NamespacedConfigData(config_map, MillisecondsSinceEpoch());
  } else if (response.StatusMatch("NO_TEMPLATE")) {
    config_map.erase(namespaces_);
    configs_.fetched =
        NamespacedConfigData(config_map, MillisecondsSinceEpoch());
  } else if (response.StatusMatch("EMPTY_CONFIG")) {
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
    info.throttled_end_time = now + kDefaultCacheExpiration;
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
}  // namespace firebase::remote_config::internal
