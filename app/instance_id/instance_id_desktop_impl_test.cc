// Copyright 2019 Google LLC
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

#include "app/instance_id/instance_id_desktop_impl.h"

#include <map>
#include <set>
#include <string>

#include "app/rest/transport_mock.h"
#include "app/rest/util.h"
#include "app/rest/www_form_url_encoded.h"
#include "app/src/app_identifier.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/version.h"
#include "app/src/log.h"
#include "app/src/secure/user_secure_manager_fake.h"
#include "app/src/time.h"
#include "app/tests/include/firebase/app_for_testing.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "testing/config.h"
#include "third_party/jsoncpp/testing.h"

namespace firebase {
namespace instance_id {
namespace internal {
using ::testing::MatchesRegex;
using ::testing::Ne;

// Access fields and method from another class. Used because only the
// InstanceIdDesktopImplTest class has access, not any of the test case classes.
#define ACCESS_FIELD(object, name, field_type, field_name) \
  static void Set##name(object* impl, field_type value) {  \
    impl->field_name = value;                              \
  }                                                        \
  static field_type Get##name(object* impl) { return impl->field_name; }

#define ACCESS_METHOD0(object, method_return_type, method_name) \
  static method_return_type method_name(object* impl) {         \
    return impl->method_name();                                 \
  }

#define ACCESS_METHOD1(object, method_return_type, method_name, arg1_type) \
  static method_return_type method_name(object* impl, arg1_type arg1) {    \
    return impl->method_name(arg1);                                        \
  }

#define ACCESS_METHOD2(object, method_return_type, method_name, arg1_type, \
                       arg2_type)                                          \
  static method_return_type method_name(object* impl, arg1_type arg1,      \
                                        arg2_type arg2) {                  \
    return impl->method_name(arg1, arg2);                                  \
  }

#define SENDER_ID "55662211"
static const char kAppName[] = "app";
static const char kStorageDomain[] = "iid_test";
static const char kSampleProjectId[] = "sample_project_id";
static const char kSamplePackageName[] = "sample.package.name";
static const char kAppVersion[] = "5.6.7";
static const char kOsVersion[] = "freedos-1.2.3";
static const int kPlatform = 100;

static const char kInstanceId[] = "test_instance_id";
static const char kDeviceId[] = "test_device_id";
static const char kSecurityToken[] = "test_security_token";
static const uint64_t kLastCheckinTimeMs = 0x1234567890L;
static const char kDigest[] = "test_digest";

// Mock REST transport that validates request parameters.
class ValidatingTransportMock : public rest::TransportMock {
 public:
  struct ExpectedRequest {
    ExpectedRequest() {}

    ExpectedRequest(const char* body_, bool body_is_json_,
                    const std::map<std::string, std::string>& headers_)
        : body(body_), body_is_json(body_is_json_), headers(headers_) {}

    std::string body;
    bool body_is_json;
    std::map<std::string, std::string> headers;
  };

  ValidatingTransportMock() {}

  void SetExpectedRequestForUrl(const std::string& url,
                                const ExpectedRequest& expected) {
    expected_request_by_url_[url] = expected;
  }

 protected:
  void PerformInternal(
      rest::Request* request, rest::Response* response,
      flatbuffers::unique_ptr<rest::Controller>* controller_out) override {
    std::string body;
    EXPECT_TRUE(request->ReadBodyIntoString(&body));

    auto expected_it = expected_request_by_url_.find(request->options().url);
    if (expected_it != expected_request_by_url_.end()) {
      const ExpectedRequest& expected = expected_it->second;
      if (expected.body_is_json) {
        EXPECT_THAT(body, Json::testing::EqualsJson(expected.body));
      } else {
        EXPECT_EQ(body, expected.body);
      }
      EXPECT_EQ(request->options().header, expected.headers);
    }

    rest::TransportMock::PerformInternal(request, response, controller_out);
  }

 private:
  std::map<std::string, ExpectedRequest> expected_request_by_url_;
};

class InstanceIdDesktopImplTest : public ::testing::Test {
 protected:
  void SetUp() override {
    LogSetLevel(kLogLevelDebug);
    AppOptions options = testing::MockAppOptions();
    options.set_package_name(kSamplePackageName);
    options.set_project_id(kSampleProjectId);
    options.set_messaging_sender_id(SENDER_ID);
    app_ = testing::CreateApp(options, kAppName);
    impl_ = InstanceIdDesktopImpl::GetInstance(app_);
    SetUserSecureManager(
        impl_,
        MakeUnique<firebase::app::secure::UserSecureManagerFake>(
            kStorageDomain,
            firebase::internal::CreateAppIdentifierFromOptions(app_->options())
                .c_str()));
    transport_ = new ValidatingTransportMock();
    SetTransport(impl_, UniquePtr<ValidatingTransportMock>(transport_));
  }

  void TearDown() override {
    DeleteFromStorage(impl_);
    delete impl_;
    delete app_;
    transport_ = nullptr;
  }

  // Busy waits until |future| has completed.
  void WaitForFuture(const FutureBase& future) {
    ASSERT_THAT(future.status(), Ne(FutureStatus::kFutureStatusInvalid));
    while (true) {
      if (future.status() != FutureStatus::kFutureStatusPending) {
        break;
      }
    }
  }

  // Create accessors / mutators for private fields in InstanceIdDesktopImpl.
  ACCESS_FIELD(InstanceIdDesktopImpl, UserSecureManager,
               UniquePtr<firebase::app::secure::UserSecureManager>,
               user_secure_manager_);
  ACCESS_FIELD(InstanceIdDesktopImpl, InstanceId, std::string, instance_id_);
  typedef std::map<std::string, std::string> TokenMap;
  ACCESS_FIELD(InstanceIdDesktopImpl, Tokens, TokenMap, tokens_);
  ACCESS_FIELD(InstanceIdDesktopImpl, Locale, std::string, locale_);
  ACCESS_FIELD(InstanceIdDesktopImpl, Timezone, std::string, timezone_);
  ACCESS_FIELD(InstanceIdDesktopImpl, LoggingId, int, logging_id_);
  ACCESS_FIELD(InstanceIdDesktopImpl, IosDeviceModel, std::string,
               ios_device_model_);
  ACCESS_FIELD(InstanceIdDesktopImpl, IosDeviceVersion, std::string,
               ios_device_version_);
  ACCESS_FIELD(InstanceIdDesktopImpl, CheckinDataLastCheckinTimeMs, uint64_t,
               checkin_data_.last_checkin_time_ms);
  ACCESS_FIELD(InstanceIdDesktopImpl, CheckinDataSecurityToken, std::string,
               checkin_data_.security_token);
  ACCESS_FIELD(InstanceIdDesktopImpl, CheckinDataDeviceId, std::string,
               checkin_data_.device_id);
  ACCESS_FIELD(InstanceIdDesktopImpl, CheckinDataDigest, std::string,
               checkin_data_.digest);
  ACCESS_FIELD(InstanceIdDesktopImpl, AppVersion, std::string, app_version_);
  ACCESS_FIELD(InstanceIdDesktopImpl, OsVersion, std::string, os_version_);
  ACCESS_FIELD(InstanceIdDesktopImpl, Platform, int, platform_);
  ACCESS_FIELD(InstanceIdDesktopImpl, Transport, UniquePtr<rest::Transport>,
               transport_);
  // Create wrappers for private methods in InstanceIdDesktopImpl.
  ACCESS_METHOD0(InstanceIdDesktopImpl, bool, SaveToStorage);
  ACCESS_METHOD0(InstanceIdDesktopImpl, bool, LoadFromStorage);
  ACCESS_METHOD0(InstanceIdDesktopImpl, bool, DeleteFromStorage);
  ACCESS_METHOD0(InstanceIdDesktopImpl, bool, InitialOrRefreshCheckin);
  ACCESS_METHOD0(InstanceIdDesktopImpl, std::string, GenerateAppId);
  ACCESS_METHOD2(InstanceIdDesktopImpl, bool, FetchServerToken, const char*,
                 bool*);
  ACCESS_METHOD2(InstanceIdDesktopImpl, bool, DeleteServerToken, const char*,
                 bool);

  InstanceIdDesktopImpl* impl_;
  App* app_;
  ValidatingTransportMock* transport_;
};

TEST_F(InstanceIdDesktopImplTest, TestInitialization) {
  // Does everything initialize and delete properly? Checked automatically.
}

TEST_F(InstanceIdDesktopImplTest, TestSaveAndLoad) {
  SetInstanceId(impl_, kInstanceId);
  SetCheckinDataLastCheckinTimeMs(impl_, kLastCheckinTimeMs);
  SetCheckinDataDeviceId(impl_, kDeviceId);
  SetCheckinDataSecurityToken(impl_, kSecurityToken);
  SetCheckinDataDigest(impl_, kDigest);
  std::map<std::string, std::string> tokens;
  tokens["*"] = "123456789";
  tokens["fcm"] = "987654321";
  SetTokens(impl_, tokens);

  // Save to storage.
  EXPECT_TRUE(SaveToStorage(impl_));

  // Zero out the in-memory version so we need to load from storage.
  SetInstanceId(impl_, "");
  SetCheckinDataLastCheckinTimeMs(impl_, 0);
  SetCheckinDataDeviceId(impl_, "");
  SetCheckinDataSecurityToken(impl_, "");
  SetCheckinDataDigest(impl_, "");
  SetTokens(impl_, std::map<std::string, std::string>());

  // Make sure the data is zeroed out.
  EXPECT_EQ("", GetInstanceId(impl_));
  EXPECT_EQ(0, GetCheckinDataLastCheckinTimeMs(impl_));
  EXPECT_EQ("", GetCheckinDataDeviceId(impl_));
  EXPECT_EQ("", GetCheckinDataSecurityToken(impl_));
  EXPECT_EQ("", GetCheckinDataDigest(impl_));
  EXPECT_EQ(0, GetTokens(impl_).size());

  // Load the data from storage.
  EXPECT_TRUE(LoadFromStorage(impl_));

  // Ensure that the loaded data is correct.
  EXPECT_EQ(kInstanceId, GetInstanceId(impl_));
  EXPECT_EQ(kLastCheckinTimeMs, GetCheckinDataLastCheckinTimeMs(impl_));
  EXPECT_EQ(kDeviceId, GetCheckinDataDeviceId(impl_));
  EXPECT_EQ(kSecurityToken, GetCheckinDataSecurityToken(impl_));
  EXPECT_EQ(kDigest, GetCheckinDataDigest(impl_));
  EXPECT_EQ(tokens, GetTokens(impl_));

  EXPECT_TRUE(DeleteFromStorage(impl_));
  EXPECT_FALSE(LoadFromStorage(impl_))
      << "LoadFromStorage() should return false after deletion.";
}

TEST_F(InstanceIdDesktopImplTest, TestGenerateAppId) {
  const int kNumAppIds = 100;  // Generate 100 AppIDs.
  std::set<std::string> generated_app_ids;

  for (int i = 0; i < kNumAppIds; ++i) {
    std::string app_id = GenerateAppId(impl_);

    // AppIDs are always 11 bytes long.
    EXPECT_EQ(app_id.length(), 11) << "Bad length: " << app_id;

    // AppIDs always start with c, d, e, or f, since the first 4 bits are 0x7.
    EXPECT_TRUE(app_id[0] == 'c' || app_id[0] == 'd' || app_id[0] == 'e' ||
                app_id[0] == 'f')
        << "Invalid first character: " << app_id;

    // AppIDs should only consist of [A-Za-z0-9_-]
    EXPECT_THAT(app_id, MatchesRegex("^[A-Za-z0-9_-]*$"));

    // The same AppIDs should never be generated twice, so ensure no collision
    // occurred. In theory this may be slightly flaky, but in practice if it
    // actually collides with only 100 AppIDs, then we have a bigger problem.
    EXPECT_TRUE(generated_app_ids.find(app_id) == generated_app_ids.end())
        << "Got an AppID collision: " << app_id;
    generated_app_ids.insert(app_id);
  }
}

TEST_F(InstanceIdDesktopImplTest, CheckinFailure) {
  // Backend returns an error.
  firebase::testing::cppsdk::ConfigSet(
      "{"
      "  config: ["
      "    {"
      "      fake: 'https://device-provisioning.googleapis.com/checkin',"
      "      httpresponse: {"
      "        header: ['HTTP/1.1 405 Method Not Allowed'],"
      "        body: ['']"
      "      }"
      "    }"
      "  ]"
      "}");
  EXPECT_FALSE(InitialOrRefreshCheckin(impl_));

  // Backend returns a malformed response.
  firebase::testing::cppsdk::ConfigSet(
      "{"
      "  config: ["
      "    {"
      "      fake: 'https://device-provisioning.googleapis.com/checkin',"
      "      httpresponse: {"
      "        header: ['HTTP/1.1 200 OK'],"
      "        body: ['a bad response']"
      "      }"
      "    }"
      "  ]"
      "}");
  EXPECT_FALSE(InitialOrRefreshCheckin(impl_));
}

#define CHECKIN_SECURITY_TOKEN "123456789"
#define CHECKIN_DEVICE_ID "987654321"
#define CHECKIN_DIGEST "CA/fDTryF5eVxjNF8ZIJAg=="

#define CHECKIN_RESPONSE_BODY                                        \
  "        {"                                                        \
  "          \"device_data_version_info\":"                          \
  "\"DEVICE_DATA_VERSION_INFO_PLACEHOLDER\","                        \
  "          \"stats_ok\":\"1\","                                    \
  "          \"security_token\":" CHECKIN_SECURITY_TOKEN             \
  ","                                                                \
  "          \"digest\":\"" CHECKIN_DIGEST                           \
  "\","                                                              \
  "          \"time_msec\":1557948713568,"                           \
  "          \"version_info\":\"0-qhPDIT2HYXIJ42qPW9kfDzoKzPqxY\","  \
  "          \"android_id\":" CHECKIN_DEVICE_ID                      \
  ","                                                                \
  "          \"intent\":["                                           \
  "            {\"action\":\"com.google.android.gms.checkin.NOOP\"}" \
  "          ],"                                                     \
  "          \"setting\":["                                          \
  "            {\"name\":\"android_id\","                            \
  "            \"value\":\"" CHECKIN_DEVICE_ID                       \
  "\"},"                                                             \
  "            {\"name\":\"device_country\","                        \
  "            \"value\":\"us\"},"                                   \
  "            {\"name\":\"device_registration_time\","              \
  "            \"value\":\"1557946800000\"},"                        \
  "            {\"name\":\"ios_device\","                            \
  "            \"value\":\"1\"}"                                     \
  "          ]"                                                      \
  "        }"

TEST_F(InstanceIdDesktopImplTest, Checkin) {
  firebase::testing::cppsdk::ConfigSet(
      "{"
      "  config: ["
      "    {"
      "      fake: 'https://device-provisioning.googleapis.com/checkin',"
      "      httpresponse: {"
      "        header: ['HTTP/1.1 200 OK'],"
      "        body: ['" CHECKIN_RESPONSE_BODY
      "']"
      "      }"
      "    }"
      "  ]"
      "}");

#define CHECKIN_TIMEZONE "America/Los_Angeles"
#define CHECKIN_LOCALE "en_US"
#define CHECKIN_IOS_DEVICE_MODEL "iPhone 8"
#define CHECKIN_IOS_DEVICE_VERSION "8.0"
#define CHECKIN_LOGGING_ID 11223344

  std::map<std::string, std::string> headers;
  headers[rest::util::kAccept] = rest::util::kApplicationJson;
  headers[rest::util::kContentType] = rest::util::kApplicationJson;
  transport_->SetExpectedRequestForUrl(
      "https://device-provisioning.googleapis.com/checkin",
      ValidatingTransportMock::ExpectedRequest(
          "{ \"checkin\": "
          "{ \"iosbuild\": "
          "{ \"model\": \"" CHECKIN_IOS_DEVICE_MODEL "\", "
          "\"os_version\": \"" CHECKIN_IOS_DEVICE_VERSION "\" }, "
          "\"last_checkin_msec\": 0, \"type\": 2, \"user_number\": 0 }, "
          "\"digest\": \"\", \"fragment\": 0, \"id\": 0, "
          "\"locale\": \"en_US\", "
          "\"logging_id\": " FIREBASE_STRING(
              CHECKIN_LOGGING_ID) ", "
                                  "\"security_token\": 0, \"timezone\": "
                                  "\"" CHECKIN_TIMEZONE "\", "
                                  "\"user_serial_number\": 0, \"version\": 2 }",
          true, headers));
  SetLocale(impl_, CHECKIN_LOCALE);
  SetTimezone(impl_, CHECKIN_TIMEZONE);
  SetLoggingId(impl_, CHECKIN_LOGGING_ID);
  SetIosDeviceModel(impl_, CHECKIN_IOS_DEVICE_MODEL);
  SetIosDeviceVersion(impl_, CHECKIN_IOS_DEVICE_VERSION);
  EXPECT_TRUE(InitialOrRefreshCheckin(impl_));

  // Make sure the logged checkin time is within a second.
  EXPECT_LT(firebase::internal::GetTimestamp() -
                GetCheckinDataLastCheckinTimeMs(impl_),
            1000);
  // Check the cached check-in data.
  EXPECT_EQ(CHECKIN_SECURITY_TOKEN, GetCheckinDataSecurityToken(impl_));
  EXPECT_EQ(CHECKIN_DEVICE_ID, GetCheckinDataDeviceId(impl_));
  EXPECT_EQ(CHECKIN_DIGEST, GetCheckinDataDigest(impl_));

  // Try checking in again, this should do nothing as the credentials haven't
  // expired.
  firebase::testing::cppsdk::ConfigSet("{}");
  transport_->SetExpectedRequestForUrl(
      "https://device-provisioning.googleapis.com/checkin",
      ValidatingTransportMock::ExpectedRequest());
  EXPECT_TRUE(InitialOrRefreshCheckin(impl_));
  // Make sure the cached check-in data didn't change.
  EXPECT_EQ(CHECKIN_SECURITY_TOKEN, GetCheckinDataSecurityToken(impl_));
  EXPECT_EQ(CHECKIN_DEVICE_ID, GetCheckinDataDeviceId(impl_));
  EXPECT_EQ(CHECKIN_DIGEST, GetCheckinDataDigest(impl_));

#undef CHECKIN_TIMEZONE
#undef CHECKIN_LOCALE
#undef CHECKIN_IOS_DEVICE_MODEL
#undef CHECKIN_IOS_DEVICE_VERSION
#undef CHECKIN_LOGGING_ID
#undef CHECKIN_LOGGING_ID_STRING
}

#define FETCH_TOKEN "atoken"

TEST_F(InstanceIdDesktopImplTest, FetchServerToken) {
  firebase::testing::cppsdk::ConfigSet(
      "{"
      "  config: ["
      "    {"
      "      fake: 'https://device-provisioning.googleapis.com/checkin',"
      "      httpresponse: {"
      "        header: ['HTTP/1.1 200 OK'],"
      "        body: ['" CHECKIN_RESPONSE_BODY
      "']"
      "      }"
      "    },"
      "    {"
      "      fake: 'https://fcmtoken.googleapis.com/register',"
      "      httpresponse: {"
      "        header: ['HTTP/1.1 200 OK'],"
      "        body: ['token=" FETCH_TOKEN
      "']"
      "      }"
      "    }"
      "  ]"
      "}");

  // Set token fetch parameters.
  SetAppVersion(impl_, kAppVersion);
  SetOsVersion(impl_, kOsVersion);
  SetPlatform(impl_, kPlatform);
  // TODO(smiles): The following two lines should be removed when we're
  // generating the IID.
  SetInstanceId(impl_, kInstanceId);
  SaveToStorage(impl_);
  std::string expected_request;
  {
    rest::WwwFormUrlEncoded form(&expected_request);
    form.Add("sender", SENDER_ID);
    form.Add("app", kSamplePackageName);
    form.Add("app_ver", kAppVersion);
    form.Add("device", CHECKIN_DEVICE_ID);
    form.Add("X-scope", "*");
    form.Add("X-subtype", SENDER_ID);
    form.Add("X-osv", kOsVersion);
    form.Add("plat", "100");
    form.Add("app_id", kInstanceId);
  }
  std::map<std::string, std::string> headers;
  headers[rest::util::kAccept] = rest::util::kApplicationWwwFormUrlencoded;
  headers[rest::util::kContentType] = rest::util::kApplicationWwwFormUrlencoded;
  headers[rest::util::kAuthorization] =
      std::string("AidLogin ") + std::string(CHECKIN_DEVICE_ID) +
      std::string(":") + std::string(CHECKIN_SECURITY_TOKEN);
  transport_->SetExpectedRequestForUrl(
      "https://fcmtoken.googleapis.com/register",
      ValidatingTransportMock::ExpectedRequest(expected_request.c_str(), false,
                                               headers));
  bool retry;
  EXPECT_TRUE(FetchServerToken(impl_, "*", &retry));
  EXPECT_FALSE(retry);

  std::map<std::string, std::string> expected_tokens;
  expected_tokens["*"] = FETCH_TOKEN;
  EXPECT_EQ(expected_tokens, GetTokens(impl_));
}

TEST_F(InstanceIdDesktopImplTest, FetchServerTokenRegistrationError) {
  firebase::testing::cppsdk::ConfigSet(
      "{"
      "  config: ["
      "    {"
      "      fake: 'https://device-provisioning.googleapis.com/checkin',"
      "      httpresponse: {"
      "        header: ['HTTP/1.1 200 OK'],"
      "        body: ['" CHECKIN_RESPONSE_BODY
      "']"
      "      }"
      "    },"
      "    {"
      "      fake: 'https://fcmtoken.googleapis.com/register',"
      "      httpresponse: {"
      "        header: ['HTTP/1.1 200 OK'],"
      "        body: ['Error=PHONE_REGISTRATION_ERROR&token=" FETCH_TOKEN
      "sender=55662211"
      "']"
      "      }"
      "    }"
      "  ]"
      "}");

  // Set token fetch parameters.
  SetAppVersion(impl_, kAppVersion);
  SetOsVersion(impl_, kOsVersion);
  SetPlatform(impl_, kPlatform);
  // TODO(smiles): The following two lines should be removed when we're
  // generating the IID.
  SetInstanceId(impl_, kInstanceId);
  std::map<std::string, std::string> tokens;
  tokens["*"] = FETCH_TOKEN;
  SetTokens(impl_, tokens);
  SaveToStorage(impl_);

  bool retry;
  EXPECT_FALSE(FetchServerToken(impl_, "fcm", &retry));
  EXPECT_TRUE(retry);
  EXPECT_EQ(1, GetTokens(impl_).size());
}

TEST_F(InstanceIdDesktopImplTest, FetchServerTokenExpired) {
  firebase::testing::cppsdk::ConfigSet(
      "{"
      "  config: ["
      "    {"
      "      fake: 'https://device-provisioning.googleapis.com/checkin',"
      "      httpresponse: {"
      "        header: ['HTTP/1.1 200 OK'],"
      "        body: ['" CHECKIN_RESPONSE_BODY
      "']"
      "      }"
      "    },"
      "    {"
      "      fake: 'https://fcmtoken.googleapis.com/register',"
      "      httpresponse: {"
      "        header: ['HTTP/1.1 200 OK'],"
      "        body: ['Error=foo%3Abar%3Aother%20stuff%3ARST&token=" FETCH_TOKEN
      "sender=55662211"
      "']"
      "      }"
      "    }"
      "  ]"
      "}");

  // Set token fetch parameters.
  SetAppVersion(impl_, kAppVersion);
  SetOsVersion(impl_, kOsVersion);
  SetPlatform(impl_, kPlatform);
  // TODO(smiles): The following two lines should be removed when we're
  // generating the IID.
  SetInstanceId(impl_, kInstanceId);
  std::map<std::string, std::string> tokens;
  tokens["*"] = FETCH_TOKEN;
  SetTokens(impl_, tokens);
  SaveToStorage(impl_);

  bool retry;
  EXPECT_FALSE(FetchServerToken(impl_, "fcm", &retry));
  EXPECT_FALSE(retry);
  EXPECT_EQ(0, GetTokens(impl_).size());
}

#undef FETCH_TOKEN
#undef CHECKIN_SECURITY_TOKEN
#undef CHECKIN_DEVICE_ID
#undef CHECKIN_DIGEST
#undef CHECKIN_RESPONSE_BODY

TEST_F(InstanceIdDesktopImplTest, FetchServerTokenFailure) {
  // Backend returns an error.
  firebase::testing::cppsdk::ConfigSet(
      "{"
      "  config: ["
      "    {"
      "      fake: 'https://fcmtoken.googleapis.com/register',"
      "      httpresponse: {"
      "        header: ['HTTP/1.1 405 Method Not Allowed'],"
      "        body: ['']"
      "      }"
      "    }"
      "  ]"
      "}");
  bool retry;
  EXPECT_FALSE(FetchServerToken(impl_, "*", &retry));
  EXPECT_FALSE(retry);
  EXPECT_EQ(0, GetTokens(impl_).size());

  // Backend returns an invalid response.
  firebase::testing::cppsdk::ConfigSet(
      "{"
      "  config: ["
      "    {"
      "      fake: 'https://fcmtoken.googleapis.com/register',"
      "      httpresponse: {"
      "        header: ['HTTP/1.1 200 OK'],"
      "        body: ['foo=bar&wibble=wobble']"
      "      }"
      "    }"
      "  ]"
      "}");
  EXPECT_FALSE(FetchServerToken(impl_, "*", &retry));
  EXPECT_FALSE(retry);
  EXPECT_EQ(0, GetTokens(impl_).size());
}

TEST_F(InstanceIdDesktopImplTest, DeleteServerTokenNoop) {
  // Deleting a token that doesn't exist should succeed.
  EXPECT_TRUE(DeleteServerToken(impl_, nullptr, true));
  EXPECT_TRUE(DeleteServerToken(impl_, "fcm", false));
}

TEST_F(InstanceIdDesktopImplTest, DeleteServerToken) {
  const char* kResponses[] = {
      "{"
      "  config: ["
      "    {"
      "      fake: 'https://fcmtoken.googleapis.com/register',"
      "      httpresponse: {"
      "        header: ['HTTP/1.1 200 OK'],"
      "        body: ['token=" SENDER_ID
      "']"
      "      }"
      "    }"
      "  ]"
      "}",
      "{"
      "  config: ["
      "    {"
      "      fake: 'https://fcmtoken.googleapis.com/register',"
      "      httpresponse: {"
      "        header: ['HTTP/1.1 200 OK'],"
      "        body: ['deleted=" SENDER_ID
      "']"
      "      }"
      "    }"
      "  ]"
      "}",
  };
  for (size_t i = 0; i < sizeof(kResponses) / sizeof(kResponses[0]); ++i) {
    firebase::testing::cppsdk::ConfigSet(kResponses[i]);

    std::string expected_request;
    {
      rest::WwwFormUrlEncoded form(&expected_request);
      form.Add("sender", SENDER_ID);
      form.Add("app", kSamplePackageName);
      form.Add("app_ver", kAppVersion);
      form.Add("device", kDeviceId);
      form.Add("X-scope", "fcm");
      form.Add("X-subtype", SENDER_ID);
      form.Add("X-osv", kOsVersion);
      form.Add("plat", "100");
      form.Add("app_id", kInstanceId);
      form.Add("delete", "true");
    }
    std::map<std::string, std::string> headers;
    headers[rest::util::kAccept] = rest::util::kApplicationWwwFormUrlencoded;
    headers[rest::util::kContentType] =
        rest::util::kApplicationWwwFormUrlencoded;
    headers[rest::util::kAuthorization] =
        std::string("AidLogin ") + std::string(kDeviceId) + std::string(":") +
        std::string(kSecurityToken);

    transport_->SetExpectedRequestForUrl(
        "https://fcmtoken.googleapis.com/register",
        ValidatingTransportMock::ExpectedRequest(expected_request.c_str(),
                                                 false, headers));

    SetAppVersion(impl_, kAppVersion);
    SetOsVersion(impl_, kOsVersion);
    SetPlatform(impl_, kPlatform);
    SetCheckinDataDeviceId(impl_, kDeviceId);
    SetCheckinDataSecurityToken(impl_, kSecurityToken);
    SetCheckinDataLastCheckinTimeMs(impl_, firebase::internal::GetTimestamp());
    SetInstanceId(impl_, kInstanceId);
    std::map<std::string, std::string> tokens;
    tokens["*"] = "123456789";
    tokens["fcm"] = "987654321";
    SetTokens(impl_, tokens);
    SaveToStorage(impl_);

    EXPECT_TRUE(DeleteServerToken(impl_, "fcm", false)) << "Iteration " << i;

    std::map<std::string, std::string> expected_tokens;
    expected_tokens["*"] = "123456789";
    EXPECT_EQ(expected_tokens, GetTokens(impl_));

    // Clean up storage before the next iteration.
    DeleteFromStorage(impl_);
  }
}

TEST_F(InstanceIdDesktopImplTest, DeleteTokenFailed) {
  SetAppVersion(impl_, kAppVersion);
  SetOsVersion(impl_, kOsVersion);
  SetPlatform(impl_, kPlatform);
  SetCheckinDataDeviceId(impl_, kDeviceId);
  SetCheckinDataSecurityToken(impl_, kSecurityToken);
  SetCheckinDataLastCheckinTimeMs(impl_, firebase::internal::GetTimestamp());
  SetInstanceId(impl_, kInstanceId);
  std::map<std::string, std::string> tokens;
  tokens["*"] = "123456789";
  tokens["fcm"] = "987654321";
  SetTokens(impl_, tokens);
  SaveToStorage(impl_);

  // Delete a token that isn't present.
  EXPECT_TRUE(DeleteServerToken(impl_, "non-existent-token", false));
  EXPECT_EQ(tokens, GetTokens(impl_));

  // Delete a token with a server failure.
  firebase::testing::cppsdk::ConfigSet(
      "{"
      "  config: ["
      "    {"
      "      fake: 'https://fcmtoken.googleapis.com/register',"
      "      httpresponse: {"
      "        header: ['HTTP/1.1 405 Method Not Allowed'],"
      "        body: ['']"
      "      }"
      "    }"
      "  ]"
      "}");
  EXPECT_FALSE(DeleteServerToken(impl_, "fcm", false));
  EXPECT_EQ(tokens, GetTokens(impl_));

  // Delete a token with an invalid server response.
  firebase::testing::cppsdk::ConfigSet(
      "{"
      "  config: ["
      "    {"
      "      fake: 'https://fcmtoken.googleapis.com/register',"
      "      httpresponse: {"
      "        header: ['HTTP/1.1 200 OK'],"
      "        body: ['everything is just fine']"
      "      }"
      "    }"
      "  ]"
      "}");
  EXPECT_FALSE(DeleteServerToken(impl_, "fcm", false));
  EXPECT_EQ(tokens, GetTokens(impl_));
}

TEST_F(InstanceIdDesktopImplTest, DeleteAllServerTokens) {
  firebase::testing::cppsdk::ConfigSet(
      "{"
      "  config: ["
      "    {"
      "      fake: 'https://fcmtoken.googleapis.com/register',"
      "      httpresponse: {"
      "        header: ['HTTP/1.1 200 OK'],"
      "        body: ['token=" SENDER_ID
      "']"
      "      }"
      "    }"
      "  ]"
      "}");

  std::string expected_request;
  {
    rest::WwwFormUrlEncoded form(&expected_request);
    form.Add("sender", SENDER_ID);
    form.Add("app", kSamplePackageName);
    form.Add("app_ver", kAppVersion);
    form.Add("device", kDeviceId);
    form.Add("X-scope", "*");
    form.Add("X-subtype", SENDER_ID);
    form.Add("X-osv", kOsVersion);
    form.Add("plat", "100");
    form.Add("app_id", kInstanceId);
    form.Add("delete", "true");
    form.Add("iid-operation", "delete");
  }
  std::map<std::string, std::string> headers;
  headers[rest::util::kAccept] = rest::util::kApplicationWwwFormUrlencoded;
  headers[rest::util::kContentType] = rest::util::kApplicationWwwFormUrlencoded;
  headers[rest::util::kAuthorization] =
      std::string("AidLogin ") + std::string(kDeviceId) + std::string(":") +
      std::string(kSecurityToken);

  transport_->SetExpectedRequestForUrl(
      "https://fcmtoken.googleapis.com/register",
      ValidatingTransportMock::ExpectedRequest(expected_request.c_str(), false,
                                               headers));

  SetAppVersion(impl_, kAppVersion);
  SetOsVersion(impl_, kOsVersion);
  SetPlatform(impl_, kPlatform);
  SetCheckinDataDeviceId(impl_, kDeviceId);
  SetCheckinDataSecurityToken(impl_, kSecurityToken);
  SetCheckinDataLastCheckinTimeMs(impl_, firebase::internal::GetTimestamp());
  SetInstanceId(impl_, kInstanceId);
  std::map<std::string, std::string> tokens;
  tokens["*"] = "123456789";
  tokens["fcm"] = "987654321";
  SetTokens(impl_, tokens);
  SaveToStorage(impl_);

  EXPECT_TRUE(DeleteServerToken(impl_, nullptr, true));
  EXPECT_EQ(0, GetTokens(impl_).size());
}

}  // namespace internal
}  // namespace instance_id
}  // namespace firebase
