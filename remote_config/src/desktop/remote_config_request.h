/*
 * Copyright 2020 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef FIREBASE_REMOTE_CONFIG_SRC_DESKTOP_REMOTE_CONFIG_REQUEST_H_
#define FIREBASE_REMOTE_CONFIG_SRC_DESKTOP_REMOTE_CONFIG_REQUEST_H_

#include "app/rest/request_json.h"
#include "remote_config/request_generated.h"
#include "remote_config/request_resource.h"

#ifdef FIREBASE_TESTING
#include "gtest/gtest.h"
#endif  // FIREBASE_TESTING

namespace firebase {
namespace remote_config {
namespace internal {

class RemoteConfigRequest
    : public firebase::rest::RequestJson<fbs::Request, fbs::RequestT> {
#ifdef FIREBASE_TESTING
  friend class RemoteConfigRESTTest;
  FRIEND_TEST(RemoteConfigRESTTest, SetupRESTRequest);
#endif  // FIREBASE_TESTING
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

  void UpdatePost() { UpdatePostFields(); }
};

}  // namespace internal
}  // namespace remote_config
}  // namespace firebase

#endif  // FIREBASE_REMOTE_CONFIG_SRC_DESKTOP_REMOTE_CONFIG_REQUEST_H_
