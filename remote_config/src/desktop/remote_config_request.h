#ifndef FIREBASE_REMOTE_CONFIG_SRC_DESKTOP_REMOTE_CONFIG_REQUEST_H_
#define FIREBASE_REMOTE_CONFIG_SRC_DESKTOP_REMOTE_CONFIG_REQUEST_H_
#include <map>
#include <string>
#include <utility>
#include "app/rest/request_json.h"
#include "remote_config/request_generated.h"
#include "remote_config/request_resource.h"
#ifdef FIREBASE_TESTING
#include "gtest/gtest.h"
#endif
namespace firebase {
namespace remote_config {
namespace internal {
class RemoteConfigRequest
    : public firebase::rest::RequestJson<fbs::Request, fbs::RequestT> {
#ifdef FIREBASE_TESTING
  friend class RemoteConfigRESTTest;
  FRIEND_TEST(RemoteConfigRESTTest, SetupRESTRequest);
#endif
 public:
  RemoteConfigRequest() : RemoteConfigRequest(request_resource_data) {}
  explicit RemoteConfigRequest(const char* schema);
  explicit RemoteConfigRequest(const unsigned char* schema)
      : RemoteConfigRequest(reinterpret_cast<const char*>(schema)) {}
  void SetAppId(std::string app_id) {
    application_data_->appId = std::move(app_id);
  }
  void SetAppInstanceId(std::string installations_id) {
    application_data_->appInstanceId = std::move(installations_id);
  }
  void SetAppInstanceIdToken(std::string token) {
    application_data_->appInstanceIdToken = std::move(token);
  }
  void SetCountryCode(std::string country_code) {
    application_data_->countryCode = std::move(country_code);
  }
  void SetLanguageCode(std::string language_code) {
    application_data_->languageCode = std::move(language_code);
  }
  void SetPlatformVersion(std::string platform_version) {
    application_data_->platformVersion = std::move(platform_version);
  }
  void SetTimeZone(std::string time_zone) {
    application_data_->timeZone = std::move(time_zone);
  }
  void SetAppVersion(std::string app_version) {
    application_data_->appVersion = std::move(app_version);
  }
  void SetPackageName(std::string package_name) {
    application_data_->packageName = std::move(package_name);
  }
  void SetSdkVersion(std::string sdk_version) {
    application_data_->sdkVersion = std::move(sdk_version);
  }
  void SetAnalyticsUserProperties(std::string analytics_user_properties) {
    application_data_->analyticsUserProperties =
        std::move(analytics_user_properties);
  }
  void SetCustomSignals(std::map<std::string, Variant> custom_signals) {
    custom_signals_ = std::move(custom_signals);
  }
  const std::map<std::string, Variant>& custom_signals() const {
    return custom_signals_;
  }
  void UpdatePost() { UpdatePostFields(); }
 protected:
  void UpdatePostFields() override;
 private:
  std::map<std::string, Variant> custom_signals_;
};
}  // namespace internal
}  // namespace remote_config
}  // namespace firebase
#endif
