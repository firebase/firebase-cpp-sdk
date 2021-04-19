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

#include <map>
#include <string>

#include "app/rest/transport_builder.h"
#include "app/rest/transport_interface.h"
#include "app/rest/transport_mock.h"
#include "app/src/app_common.h"
#include "app/src/include/firebase/app.h"
#include "app/src/locale.h"
#include "app/tests/include/firebase/app_for_testing.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "testing/config.h"

namespace firebase {
namespace remote_config {
namespace internal {

const char* const kTestNamespaces = "firebasetest";

class RemoteConfigRESTTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Use TransportMock for testing instead of TransportCurl
    rest::SetTransportBuilder([]() -> std::unique_ptr<rest::Transport> {
      return std::unique_ptr<rest::Transport>(new rest::TransportMock);
    });

    firebase::AppOptions options = testing::MockAppOptions();
    options.set_package_name("com.google.samples.quickstart.config");
    options.set_app_id("1:290292664153:android:eddef00f8bd18e11");

    app_ = testing::CreateApp(options);

    SetupContent();
  }

  void TearDown() override { delete app_; }

  void SetupContent() {
    std::map<std::string, std::string> empty_map;
    NamespacedConfigData fetched(
        NamespaceKeyValueMap({
            {kTestNamespaces,
             {{"TestBoolean", "false"},
              {"TestData", "12345"},
              {"TestLong", "543"},
              {"TestDouble", "12.88"},
              {"TestString", "This is a 7 hour old string"}}},
        }),
        MillisecondsSinceEpoch() - 7 * 3600 * 1000);  // 7 hours ago.
    NamespacedConfigData active(
        NamespaceKeyValueMap({
            {kTestNamespaces,
             {{"TestBoolean", "false"},
              {"TestData", "3221"},
              {"TestLong", "876"},
              {"TestDouble", "34.55"},
              {"TestString", "This is a 10 hour old string"}}},
        }),
        MillisecondsSinceEpoch() - 10 * 3600 * 1000);  // 10 hours ago.
    // Can be empty for testing.
    NamespacedConfigData defaults(NamespaceKeyValueMap(), 0);

    RemoteConfigMetadata metadata;
    metadata.set_info(ConfigInfo(
        {MillisecondsSinceEpoch() - 7 * 3600 * 1000 /* 7 hours ago */,
         kLastFetchStatusSuccess, kFetchFailureReasonInvalid, 0}));

    configs_ = LayeredConfigs(fetched, active, defaults, metadata);
  }

  //  Check all values in case when fetch failed.
  void ExpectFetchFailure(const RemoteConfigREST& rest, int code) {
    EXPECT_EQ(rest.rc_response_.status(), code);
    EXPECT_TRUE(rest.rc_response_.header_completed());
    EXPECT_TRUE(rest.rc_response_.body_completed());

    EXPECT_EQ(rest.fetched().config(), configs_.fetched.config());
    EXPECT_EQ(rest.metadata().digest_by_namespace(),
              configs_.metadata.digest_by_namespace());

    ConfigInfo info = rest.metadata().info();
    EXPECT_EQ(info.last_fetch_status, kLastFetchStatusFailure);
    EXPECT_LE(info.fetch_time, MillisecondsSinceEpoch());
    EXPECT_GE(info.fetch_time, MillisecondsSinceEpoch() - 10000);
    EXPECT_EQ(info.last_fetch_failure_reason, kFetchFailureReasonError);
    EXPECT_LE(info.throttled_end_time, MillisecondsSinceEpoch());
    EXPECT_GE(info.throttled_end_time, MillisecondsSinceEpoch() - 10000);
  }

  uint64_t MillisecondsSinceEpoch() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
  }

  firebase::App* app_ = nullptr;

  LayeredConfigs configs_;

  rest::Response rest_response_;

  std::string response_body_ = R"({
    "entries": {
      "TestBoolean": "true",
      "TestData": "4321",
      "TestDouble": "625.63",
      "TestLong": "119",
      "TestString": "This is a string"
    },
    "appName": "com.google.android.remote_config.testapp",
    "state": "UPDATE"
  })";
};

// Check correctness object setup for REST request.
TEST_F(RemoteConfigRESTTest, Setup) {
  RemoteConfigREST rest(app_->options(), configs_, kTestNamespaces);

  EXPECT_EQ(rest.app_package_name_, app_->options().package_name());
  EXPECT_EQ(rest.app_gmp_project_id_, app_->options().app_id());
  EXPECT_EQ(rest.app_project_id_, app_->options().project_id());
  EXPECT_EQ(rest.api_key_, app_->options().api_key());
  EXPECT_EQ(rest.namespaces_, kTestNamespaces);
}

// Check correctness REST request setup.
TEST_F(RemoteConfigRESTTest, SetupRESTRequest) {
  RemoteConfigREST rest(app_->options(), configs_, kTestNamespaces);
  rest.SetupRestRequest(*app_, kDefaultTimeoutInMilliseconds);

  firebase::rest::RequestOptions request_options = rest.rc_request_.options();

  std::string server_url(kServerURL);
  server_url.append("/");
  server_url.append(rest.app_project_id_);
  server_url.append("/");
  server_url.append(kNameSpaceString);
  server_url.append("/");
  server_url.append(rest.namespaces_);
  server_url.append(kHTTPFetchKeyString);
  server_url.append(rest.api_key_);

  EXPECT_EQ(request_options.url, server_url);
  EXPECT_EQ(request_options.method, kHTTPMethodPost);

  EXPECT_NE(request_options.header.find(kContentTypeHeaderName),
            request_options.header.end());
  EXPECT_EQ(request_options.header[kContentTypeHeaderName],
            kJSONContentTypeValue);
  EXPECT_NE(request_options.header.find(kAcceptHeaderName),
            request_options.header.end());
  EXPECT_EQ(request_options.header[kAcceptHeaderName], kJSONContentTypeValue);
  EXPECT_NE(request_options.header.find(app_common::kApiClientHeader),
            request_options.header.end());
  EXPECT_EQ(request_options.header[app_common::kApiClientHeader],
            App::GetUserAgent());

  EXPECT_EQ(rest.rc_request_.application_data_->appId,
            app_->options().app_id());
  EXPECT_EQ(rest.rc_request_.application_data_->packageName,
            app_->options().package_name());
  EXPECT_EQ(rest.rc_request_.application_data_->platformVersion, "2");

  EXPECT_EQ(rest.rc_request_.application_data_->timeZone,
            firebase::internal::GetTimezone());

  std::string locale = firebase::internal::GetLocale();
  EXPECT_EQ(rest.rc_request_.application_data_->languageCode, locale);

  if (locale.length() > 0) {
    EXPECT_EQ(rest.rc_request_.application_data_->countryCode,
              locale.substr(0, 2));
  }

  // TODO(cynthiajiang) verify installations id and token.
}

// Verify the rest request with mock project will return code 404
TEST_F(RemoteConfigRESTTest, Fetch) {
  int codes[] = {404};
  for (int code : codes) {
    char config[1000];
    snprintf(config, sizeof(config),
             "{"
             "  config:["
             "    {fake:'%s',"
             "     httpresponse: {"
             "       header: ['HTTP/1.1 %d Ok','Server:mock server 101'],"
             "       body: ['some body, not proto, not gzip',]"
             "     }"
             "    }"
             "  ]"
             "}",
             kServerURL, code);
    firebase::testing::cppsdk::ConfigSet(config);

    RemoteConfigREST rest(app_->options(), configs_, kTestNamespaces);
    rest.Fetch(*app_, 3600);

    ExpectFetchFailure(rest, code);
  }
}

TEST_F(RemoteConfigRESTTest, ParseRestResponseProtoFailure) {
  std::string header = "HTTP/1.1 200 Ok";
  std::string body = "";

  RemoteConfigREST rest(app_->options(), configs_, kTestNamespaces);
  rest.rc_response_.ProcessHeader(header.data(), header.length());
  rest.rc_response_.ProcessBody(body.data(), body.length());
  rest.rc_response_.MarkCompleted();
  EXPECT_EQ(rest.rc_response_.status(), 200);

  rest.ParseRestResponse();

  ExpectFetchFailure(rest, 200);
}

TEST_F(RemoteConfigRESTTest, ParseRestResponseSuccess) {
  std::string header = "HTTP/1.1 200 Ok";

  RemoteConfigREST rest(app_->options(), configs_, kTestNamespaces);
  rest.rc_response_.ProcessHeader(header.data(), header.length());
  rest.rc_response_.ProcessBody(response_body_.data(), response_body_.length());
  rest.rc_response_.MarkCompleted();
  EXPECT_EQ(rest.rc_response_.status(), 200);

  rest.ParseRestResponse();

  std::map<std::string, std::string> empty_map;
  EXPECT_THAT(rest.fetched().config(),
              ::testing::ContainerEq(NamespaceKeyValueMap({
                  {kTestNamespaces,
                   {{"TestBoolean", "true"},
                    {"TestData", "4321"},
                    {"TestDouble", "625.63"},
                    {"TestLong", "119"},
                    {"TestString", "This is a string"}}},
              })));

  ConfigInfo info = rest.metadata().info();
  EXPECT_EQ(info.last_fetch_status, kLastFetchStatusSuccess);
  EXPECT_LE(info.fetch_time, MillisecondsSinceEpoch());
  EXPECT_GE(info.fetch_time, MillisecondsSinceEpoch() - 10000);
}

}  // namespace internal
}  // namespace remote_config
}  // namespace firebase
