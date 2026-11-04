#ifndef FIREBASE_REMOTE_CONFIG_SRC_DESKTOP_REST_H_
#define FIREBASE_REMOTE_CONFIG_SRC_DESKTOP_REST_H_
#include <cstdint>
#include <string>
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
#endif
namespace firebase::remote_config::internal {
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
#endif
  RemoteConfigREST(const firebase::AppOptions& app_options,
                   LayeredConfigs& configs, std::string namespaces);
  ~RemoteConfigREST();
  void Fetch(const App& app, uint64_t fetch_timeout_in_milliseconds);
  [[nodiscard]] const NamespacedConfigData& fetched() const {
    return configs_.fetched;
  }
  [[nodiscard]] const RemoteConfigMetadata& metadata() const {
    return configs_.metadata;
  }
 private:
  void TryGetInstallationsAndToken(const App& app);
  void SetupRestRequest(const App& app, uint64_t fetch_timeout_in_milliseconds);
  void SetupRestRequest(const App& app, uint64_t fetch_timeout_in_milliseconds,
                        RemoteConfigRequest* request);
  void ParseRestResponse();
  void ParseRestResponse(const RemoteConfigResponse& response);
  uint64_t MillisecondsSinceEpoch();
  void FetchSuccess(LastFetchStatus status);
  void FetchFailure(FetchFailureReason reason);
  std::string app_package_name_;
  std::string app_gmp_project_id_;
  std::string app_project_id_;
  std::string api_key_;
  std::string namespaces_;
  LayeredConfigs& configs_;
  std::string app_instance_id_;
  std::string app_instance_id_token_;
  RemoteConfigRequest rc_request_;
  RemoteConfigResponse rc_response_;
  Semaphore fetch_future_sem_;
};
}  // namespace firebase::remote_config::internal
#endif
