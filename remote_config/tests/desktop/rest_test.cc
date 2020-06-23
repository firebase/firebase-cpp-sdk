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
#include "app/src/include/firebase/app.h"
#include "app/tests/include/firebase/app_for_testing.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "remote_config/src/desktop/rest_nanopb_encode.h"
#include "testing/config.h"
#include "net/proto2/public/text_format.h"
#include "zlib/zlibwrapper.h"
#include "wireless/android/config/proto/config.proto.h"

namespace firebase {
namespace remote_config {
namespace internal {

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
    SetupProtoResponse();
  }

  void TearDown() override { delete app_; }

  void SetupContent() {
    std::map<std::string, std::string> empty_map;
    NamespacedConfigData fetched(
        NamespaceKeyValueMap({
            {"star_wars:droid",
             {{"name", "BB-8"},
              {"height", "0.67 meters"},
              {"mass", "18 kilograms"}}},
            {"star_wars:starship",
             {{"name", "Millennium Falcon"},
              {"length", "34.52–34.75 meters"},
              {"maximum_atmosphere_speed", "1,050 km/h"}}},
            {"star_wars:films", empty_map},
            {"star_wars:creatures",
             {{"name", "Wampa"},
              {"height", "3 meters"},
              {"mass", "150 kilograms"}}},
            {"star_wars:locations",
             {{"name", "Coruscant"},
              {"rotation_period", "24 standard hours"},
              {"orbital_period", "365 standard days"}}},
        }),
        MillisecondsSinceEpoch() - 7 * 3600 * 1000);  // 7 hours ago.
    NamespacedConfigData active(
        NamespaceKeyValueMap({{"star_wars:droid",
                               {{"name", "R2-D2"},
                                {"height", "1.09 meters"},
                                {"mass", "32 kilograms"}}},
                              {"star_wars:starship",
                               {{"name", "Imperial I-class Star Destroyer"},
                                {"length", "1,600 meters"},
                                {"maximum_atmosphere_speed", "975 km/h"}}}}),
        MillisecondsSinceEpoch() - 10 * 3600 * 1000);  // 10 hours ago.
    // Can be empty for testing.
    NamespacedConfigData defaults(NamespaceKeyValueMap(), 0);

    RemoteConfigMetadata metadata;
    metadata.set_info(ConfigInfo(
        {MillisecondsSinceEpoch() - 7 * 3600 * 1000 /* 7 hours ago */,
         kLastFetchStatusSuccess, kFetchFailureReasonInvalid, 0}));
    metadata.set_digest_by_namespace(
        MetaDigestMap({{"star_wars:droid", "DROID_DIGEST"},
                       {"star_wars:starship", "STARSHIP_DIGEST"},
                       {"star_wars:films", "FILMS_DIGEST"},
                       {"star_wars:creatures", "CREATURES_DIGEST"},
                       {"star_wars:locations", "LOCATIONS_DIGEST"}}));
    metadata.AddSetting(kConfigSettingDeveloperMode, "1");

    configs_ = LayeredConfigs(fetched, active, defaults, metadata);
  }

  void SetupProtoResponse() {
    std::string text =
        "app_config {"
        "  app_name: \"com.google.samples.quickstart.config\""

        // UPDATE, add new namespace.
        "    namespace_config {"
        "      namespace: \"star_wars:vehicle\""
        "      digest: \"VEHICLE_NEW_DIGEST\""
        "      status: UPDATE"
        "      entry {key: \"name\" value: \"All Terrain Armored Transport\"}"
        "      entry {key: \"passengers\" value: \"40 troops\"}"
        "      entry {key: \"cargo_capacity\" value: \"3,500 metric tons\"}"
        "    }"

        // UPDATE, update existed namespace.
        "    namespace_config {"
        "      namespace: \"star_wars:starship\""
        "      digest: \"STARSHIP_NEW_DIGEST\""
        "      status: UPDATE"
        "      entry {key: \"name\" value: \"Imperial I-class Star Destroyer\"}"
        "      entry {key: \"length\" value: \"1,600 meters\"}"
        "      entry {key: \"maximum_atmosphere_speed\" value: \"975 km/h\"}"
        "    }"

        // NO_TEMPLATE for existed namespace. Remove digest and namespace.
        "    namespace_config {"
        "      namespace: \"star_wars:films\" status: NO_TEMPLATE"
        "    }"

        // NO_TEMPLATE for NOT existed namespace. Will be ignored.
        "    namespace_config {"
        "      namespace: \"star_wars:spinoff_films\" status: NO_TEMPLATE"
        "    }"

        // NO_CHANGE for existed namespace. Only digest will be updated.
        "    namespace_config {"
        "      namespace: \"star_wars:droid\""
        "      digest: \"DROID_NEW_DIGEST\""
        "      status: NO_CHANGE"
        "    }"

        // EMPTY_CONFIG for existed namespace. Clear namespace and update
        // digest.
        "    namespace_config {"
        "      namespace: \"star_wars:creatures\""
        "      digest: \"CREATURES_NEW_DIGEST\""
        "      status: EMPTY_CONFIG"
        "    }"

        // EMPTY_CONFIG for NOT existed namespace. Create empty namespace and
        // add new digest to map.
        "    namespace_config {"
        "      namespace: \"star_wars:duels\""
        "      digest: \"DUELS_NEW_DIGEST\""
        "      status: EMPTY_CONFIG"
        "    }"

        // NOT_AUTHORIZED for existed namespace. Remove namespace and digest.
        "    namespace_config {"
        "      namespace: \"star_wars:locations\""
        "      status: NOT_AUTHORIZED"
        "    }"

        // NOT_AUTHORIZED for NOT existed namespace. Will be ignored.
        "    namespace_config {"
        "      namespace: \"star_wars:video_games\""
        "      status: NOT_AUTHORIZED"
        "    }"

        "}";

    EXPECT_TRUE(proto2::TextFormat::ParseFromString(text, &proto_response_));
  }

  // This was moved from the code that used to build proto requests when
  // protosbufs were used directly. It can live here because the tests can
  // still depend on protobufs and gives us a way to validate nanopbs are
  // encoded the same way as the original protos.
  android::config::ConfigFetchRequest GetProtoFetchRequestData(
      const RemoteConfigREST& rest) {
    android::config::ConfigFetchRequest proto_request;
    proto_request.set_client_version(2);
    proto_request.set_device_type(5);
    proto_request.set_device_subtype(10);

    android::config::PackageData* package_data =
        proto_request.add_package_data();
    package_data->set_package_name(rest.app_package_name_);
    package_data->set_gmp_project_id(rest.app_gmp_project_id_);

    for (const auto& keyvalue : rest.configs_.metadata.digest_by_namespace()) {
      android::config::NamedValue* named_value =
          package_data->add_namespace_digest();
      named_value->set_name(keyvalue.first);
      named_value->set_value(keyvalue.second);
    }

    // Check if developer mode enable
    if (rest.configs_.metadata.GetSetting(kConfigSettingDeveloperMode) == "1") {
      android::config::NamedValue* named_value =
          package_data->add_custom_variable();
      named_value->set_name(kDeveloperModeKey);
      named_value->set_value("1");
    }

    // Need iid for next two fields
    // package_data->set_app_instance_id("fake instance id");
    // package_data->set_app_instance_id_token("fake instance id token");

    package_data->set_requested_cache_expiration_seconds(
        static_cast<int32_t>(rest.cache_expiration_in_seconds_));

    if (rest.configs_.fetched.timestamp() == 0) {
      package_data->set_fetched_config_age_seconds(-1);
    } else {
      package_data->set_fetched_config_age_seconds(static_cast<int32_t>(
          (MillisecondsSinceEpoch() - rest.configs_.fetched.timestamp()) /
          1000));
    }

    package_data->set_sdk_version(SDK_MAJOR_VERSION * 10000 +
                                  SDK_MINOR_VERSION * 100 + SDK_PATCH_VERSION);

    if (rest.configs_.active.timestamp() == 0) {
      package_data->set_active_config_age_seconds(-1);
    } else {
      package_data->set_active_config_age_seconds(static_cast<int32_t>(
          (MillisecondsSinceEpoch() - rest.configs_.active.timestamp()) /
          1000));
    }
    return proto_request;
  }

  // Check all values in case when fetch failed.
  void ExpectFetchFailure(const RemoteConfigREST& rest, int code) {
    EXPECT_EQ(rest.rest_response_.status(), code);
    EXPECT_TRUE(rest.rest_response_.header_completed());
    EXPECT_TRUE(rest.rest_response_.body_completed());

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

  std::string GzipCompress(const std::string& input) {
    ZLib zlib;
    zlib.SetGzipHeaderMode();
    uLongf result_size = ZLib::MinCompressbufSize(input.length());
    std::unique_ptr<char[]> result(new char[result_size]);
    int err = zlib.Compress(
        reinterpret_cast<unsigned char*>(result.get()), &result_size,
        reinterpret_cast<const unsigned char*>(input.data()), input.length());
    EXPECT_EQ(err, Z_OK);
    return std::string(result.get(), result_size);
  }

  std::string GzipDecompress(const std::string& input) {
    ZLib zlib;
    zlib.SetGzipHeaderMode();
    uLongf result_length = zlib.GzipUncompressedLength(
        reinterpret_cast<const unsigned char*>(input.data()), input.length());
    std::unique_ptr<char[]> result(new char[result_length]);
    int err = zlib.Uncompress(
        reinterpret_cast<unsigned char*>(result.get()), &result_length,
        reinterpret_cast<const unsigned char*>(input.data()), input.length());
    EXPECT_EQ(err, Z_OK);
    return std::string(result.get(), result_length);
  }

  firebase::App* app_ = nullptr;

  LayeredConfigs configs_;

  rest::Response rest_response_;
  android::config::ConfigFetchResponse proto_response_;
};

// Check correctness protobuf object setup for REST request.
TEST_F(RemoteConfigRESTTest, SetupProto) {
  RemoteConfigREST rest(app_->options(), configs_, 3600);
  ConfigFetchRequest request_data = rest.GetFetchRequestData();

  EXPECT_EQ(request_data.client_version, 2);
  // Not handling repeated package_data since spec says there's only 1.

  PackageData& package_data = request_data.package_data;
  EXPECT_EQ(package_data.package_name, app_->options().package_name());
  EXPECT_EQ(package_data.gmp_project_id, app_->options().app_id());

  // Check digests
  std::map<std::string, std::string> digests;
  for (const auto& item : package_data.namespace_digest) {
    digests[item.first] = item.second;
  }
  EXPECT_THAT(digests, ::testing::Eq(std::map<std::string, std::string>(
                           {{"star_wars:droid", "DROID_DIGEST"},
                            {"star_wars:starship", "STARSHIP_DIGEST"},
                            {"star_wars:films", "FILMS_DIGEST"},
                            {"star_wars:creatures", "CREATURES_DIGEST"},
                            {"star_wars:locations", "LOCATIONS_DIGEST"}})));

  // Check developers settings
  std::map<std::string, std::string> settings;
  for (const auto& item : package_data.custom_variable) {
    settings[item.first] = item.second;
  }
  EXPECT_THAT(settings, ::testing::Eq(std::map<std::string, std::string>(
                            {{"_rcn_developer", "1"}})));

  // The same value as in RemoteConfigRest constructor.
  EXPECT_EQ(package_data.requested_cache_expiration_seconds, 3600);

  // Fetched age should be in range [7hours, 7hours + eps],
  // where eps - some small value in seconds.
  EXPECT_GE(package_data.fetched_config_age_seconds, 7 * 3600);
  EXPECT_LE(package_data.fetched_config_age_seconds, 7 * 3600 + 10);

  // Active age should be in range [10hours, 10hours + eps],
  // where eps - some small value in seconds.
  EXPECT_GE(package_data.active_config_age_seconds, 10 * 3600);
  EXPECT_LE(package_data.active_config_age_seconds, 10 * 3600 + 10);
}

// Check correctness REST request setup.
TEST_F(RemoteConfigRESTTest, SetupRESTRequest) {
  RemoteConfigREST rest(app_->options(), configs_, 3600);
  rest.SetupRestRequest();

  firebase::rest::RequestOptions request_options = rest.rest_request_.options();
  EXPECT_EQ(request_options.url, kServerURL);
  EXPECT_EQ(request_options.method, kHTTPMethodPost);
  std::string post_fields;
  EXPECT_TRUE(rest.rest_request_.ReadBodyIntoString(&post_fields));

  ConfigFetchRequest fetch_data = rest.GetFetchRequestData();
  std::string encoded_str = EncodeFetchRequest(fetch_data);

  EXPECT_EQ(GzipDecompress(post_fields), encoded_str);
  EXPECT_NE(request_options.header.find("Content-Type"),
            request_options.header.end());
  EXPECT_EQ(request_options.header["Content-Type"],
            "application/x-protobuffer");
  EXPECT_NE(request_options.header.find("x-goog-api-client"),
            request_options.header.end());
  EXPECT_THAT(request_options.header["x-goog-api-client"],
              ::testing::HasSubstr("fire-cpp/"));

  // Setup a proto directly with the request data.
  android::config::ConfigFetchRequest proto_data =
      GetProtoFetchRequestData(rest);
  std::string proto_str = proto_data.SerializeAsString();
  EXPECT_EQ(proto_str, encoded_str);
  // If a proto encode doesn't match, the strings aren't easily printable, so
  // the following makes it easier to examine the discrepancies.
  if (encoded_str != proto_str) {
    printf("--------- Encoded Proto ------------\n");
    android::config::ConfigFetchRequest proto_parse;
    proto_parse.ParseFromString(encoded_str);
    printf("%s\n", proto_parse.DebugString().c_str());
    printf("-------- Reference Proto -----------\n");
    printf("%s\n", proto_data.DebugString().c_str());
    printf("------------------------------------\n");

    int max_len = (encoded_str.length() > proto_str.length())
                      ? encoded_str.length()
                      : proto_str.length();
    printf("encoded size: %d   reference size: %d\n",
           static_cast<int>(encoded_str.length()),
           static_cast<int>(proto_str.length()));
    for (int i = 0; i < max_len; i++) {
      char oldc = (i < proto_str.length()) ? proto_str.c_str()[i] : 0;
      char newc = (i < encoded_str.length()) ? encoded_str.c_str()[i] : 0;
      printf("%02X (%03d) '%c'    %02X (%03d) '%c'\n",
             newc, newc, newc,
             oldc, oldc, oldc);
    }
  }
}

// Can't pass binary body response to testing::cppsdk::ConfigSet. Can configure
// only response with not gzip body.
//
// Test passing http request to mock transport and get http
// response with error or with empty body.
//
// We have 2 different cases:
//
// 1) response code is 200. Response body is empty, because can't gunzip not
// gzip body.
//
// 2) response code is 400. Will not try gunzip body, but it's still failure,
// because response code is not 200.
TEST_F(RemoteConfigRESTTest, Fetch) {
  int codes[] = {200, 400};
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

    RemoteConfigREST rest(app_->options(), configs_, 3600);
    rest.Fetch(*app_);

    ExpectFetchFailure(rest, code);
  }
}

TEST_F(RemoteConfigRESTTest, ParseRestResponseProtoFailure) {
  std::string header = "HTTP/1.1 200 Ok";
  std::string body = GzipCompress("some fake body, NOT proto");

  RemoteConfigREST rest(app_->options(), configs_, 3600);
  rest.rest_response_.ProcessHeader(header.data(), header.length());
  rest.rest_response_.ProcessBody(body.data(), body.length());
  rest.rest_response_.MarkCompleted();
  EXPECT_EQ(rest.rest_response_.status(), 200);

  rest.ParseRestResponse();

  ExpectFetchFailure(rest, 200);
}

TEST_F(RemoteConfigRESTTest, ParseRestResponseSuccess) {
  std::string header = "HTTP/1.1 200 Ok";
  std::string body = GzipCompress(proto_response_.SerializeAsString());

  RemoteConfigREST rest(app_->options(), configs_, 3600);
  rest.rest_response_.ProcessHeader(header.data(), header.length());
  rest.rest_response_.ProcessBody(body.data(), body.length());
  rest.rest_response_.MarkCompleted();
  EXPECT_EQ(rest.rest_response_.status(), 200);

  rest.ParseRestResponse();

  std::map<std::string, std::string> empty_map;
  EXPECT_THAT(rest.fetched().config(),
              ::testing::ContainerEq(NamespaceKeyValueMap({
                  {"star_wars:vehicle",
                   {{"name", "All Terrain Armored Transport"},
                    {"passengers", "40 troops"},
                    {"cargo_capacity", "3,500 metric tons"}}},
                  {"star_wars:droid",
                   {{"name", "BB-8"},
                    {"height", "0.67 meters"},
                    {"mass", "18 kilograms"}}},
                  {"star_wars:starship",
                   {{"name", "Imperial I-class Star Destroyer"},
                    {"length", "1,600 meters"},
                    {"maximum_atmosphere_speed", "975 km/h"}}},
                  {"star_wars:creatures", empty_map},
                  {"star_wars:duels", empty_map},
              })));

  EXPECT_THAT(rest.metadata().digest_by_namespace(),
              ::testing::ContainerEq(MetaDigestMap(
                  {{"star_wars:vehicle", "VEHICLE_NEW_DIGEST"},
                   {"star_wars:starship", "STARSHIP_NEW_DIGEST"},
                   {"star_wars:droid", "DROID_NEW_DIGEST"},
                   {"star_wars:creatures", "CREATURES_NEW_DIGEST"},
                   {"star_wars:duels", "DUELS_NEW_DIGEST"}})));

  ConfigInfo info = rest.metadata().info();
  EXPECT_EQ(info.last_fetch_status, kLastFetchStatusSuccess);
  EXPECT_LE(info.fetch_time, MillisecondsSinceEpoch());
  EXPECT_GE(info.fetch_time, MillisecondsSinceEpoch() - 10000);
}

}  // namespace internal
}  // namespace remote_config
}  // namespace firebase
