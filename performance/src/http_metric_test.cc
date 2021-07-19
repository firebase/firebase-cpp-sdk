// Copyright 2021 Google LLC

#if defined(FIREBASE_ANDROID_FOR_DESKTOP)
#define __ANDROID__
#include <jni.h>

#include "testing/run_all_tests.h"
#endif  // defined(FIREBASE_ANDROID_FOR_DESKTOP)

#include <memory>

#include "app/src/include/firebase/app.h"
#include "app/tests/include/firebase/app_for_testing.h"
#include "performance/src/include/firebase/performance.h"
#include "performance/src/include/firebase/performance/http_metric.h"
#include "performance/src/include/firebase/performance/trace.h"
#include "performance/src/performance_common.h"

#ifdef __ANDROID__
#include "app/src/util_android.h"
#endif  // __ANDROID__

#if defined(FIREBASE_ANDROID_FOR_DESKTOP)
#undef __ANDROID__
#endif  // defined(FIREBASE_ANDROID_FOR_DESKTOP)

#include "absl/memory/memory.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "testing/config.h"
#include "testing/reporter.h"

namespace firebase {
namespace performance {

class HttpMetricTest : public ::testing::Test {
 protected:
  void SetUp() override {
    firebase::testing::cppsdk::ConfigSet("{}");
    reporter_.reset();

    firebase_app_.reset(testing::CreateApp());
    AddExpectationAndroid("FirebasePerformance.getInstance", {});
    performance::Initialize(*firebase_app_);
  }

  void TearDown() override {
    firebase::testing::cppsdk::ConfigReset();
    Terminate();
    firebase_app_.reset();
    EXPECT_THAT(reporter_.getFakeReports(),
                ::testing::Eq(reporter_.getExpectations()));
  }

  void AddExpectationApple(const char* fake,
                           std::initializer_list<std::string> args) {
    reporter_.addExpectation(fake, "", firebase::testing::cppsdk::kIos, args);
  }

  void AddExpectationAndroid(const char* fake,
                             std::initializer_list<std::string> args) {
    reporter_.addExpectation(fake, "", firebase::testing::cppsdk::kAndroid,
                             args);
  }

  std::unique_ptr<App> firebase_app_;
  firebase::testing::cppsdk::Reporter reporter_;
};

TEST_F(HttpMetricTest, TestCreateGetRequest) {
  AddExpectationApple("-[FIRHTTPMetric initWithUrl:HTTPMethod:]",
                      {"https://google.com", "GET"});
  AddExpectationApple("-[FIRHTTPMetric start]", {});
  AddExpectationApple("-[FIRHTTPMetric stop]", {});

  AddExpectationAndroid("new HttpMetric", {"https://google.com", "GET"});
  AddExpectationAndroid("HttpMetric.start", {});
  AddExpectationAndroid("HttpMetric.stop", {});

  HttpMetric metric("https://google.com", kHttpMethodGet);
}

TEST_F(HttpMetricTest, TestCreatePutRequest) {
  AddExpectationApple("-[FIRHTTPMetric initWithUrl:HTTPMethod:]",
                      {"https://google.com", "PUT"});
  AddExpectationApple("-[FIRHTTPMetric start]", {});
  AddExpectationApple("-[FIRHTTPMetric stop]", {});

  AddExpectationAndroid("new HttpMetric", {"https://google.com", "PUT"});
  AddExpectationAndroid("HttpMetric.start", {});
  AddExpectationAndroid("HttpMetric.stop", {});

  HttpMetric metric("https://google.com", kHttpMethodPut);
}

TEST_F(HttpMetricTest, TestCreatePostRequest) {
  AddExpectationApple("-[FIRHTTPMetric initWithUrl:HTTPMethod:]",
                      {"https://google.com", "POST"});
  AddExpectationApple("-[FIRHTTPMetric start]", {});
  AddExpectationApple("-[FIRHTTPMetric stop]", {});

  AddExpectationAndroid("new HttpMetric", {"https://google.com", "POST"});
  AddExpectationAndroid("HttpMetric.start", {});
  AddExpectationAndroid("HttpMetric.stop", {});

  HttpMetric metric("https://google.com", kHttpMethodPost);
}

TEST_F(HttpMetricTest, TestCreateDeleteRequest) {
  AddExpectationApple("-[FIRHTTPMetric initWithUrl:HTTPMethod:]",
                      {"https://google.com", "DELETE"});
  AddExpectationApple("-[FIRHTTPMetric start]", {});
  AddExpectationApple("-[FIRHTTPMetric stop]", {});

  AddExpectationAndroid("new HttpMetric", {"https://google.com", "DELETE"});
  AddExpectationAndroid("HttpMetric.start", {});
  AddExpectationAndroid("HttpMetric.stop", {});

  HttpMetric metric("https://google.com", kHttpMethodDelete);
}

TEST_F(HttpMetricTest, TestCreateHeadRequest) {
  AddExpectationApple("-[FIRHTTPMetric initWithUrl:HTTPMethod:]",
                      {"https://google.com", "HEAD"});
  AddExpectationApple("-[FIRHTTPMetric start]", {});
  AddExpectationApple("-[FIRHTTPMetric stop]", {});

  AddExpectationAndroid("new HttpMetric", {"https://google.com", "HEAD"});
  AddExpectationAndroid("HttpMetric.start", {});
  AddExpectationAndroid("HttpMetric.stop", {});

  HttpMetric metric("https://google.com", kHttpMethodHead);
}

TEST_F(HttpMetricTest, TestCreatePatchRequest) {
  AddExpectationApple("-[FIRHTTPMetric initWithUrl:HTTPMethod:]",
                      {"https://google.com", "PATCH"});
  AddExpectationApple("-[FIRHTTPMetric start]", {});
  AddExpectationApple("-[FIRHTTPMetric stop]", {});

  AddExpectationAndroid("new HttpMetric", {"https://google.com", "PATCH"});
  AddExpectationAndroid("HttpMetric.start", {});
  AddExpectationAndroid("HttpMetric.stop", {});

  HttpMetric metric("https://google.com", kHttpMethodPatch);
}

TEST_F(HttpMetricTest, TestCreateOptionsRequest) {
  AddExpectationApple("-[FIRHTTPMetric initWithUrl:HTTPMethod:]",
                      {"https://google.com", "OPTIONS"});
  AddExpectationApple("-[FIRHTTPMetric start]", {});
  AddExpectationApple("-[FIRHTTPMetric stop]", {});

  AddExpectationAndroid("new HttpMetric", {"https://google.com", "OPTIONS"});
  AddExpectationAndroid("HttpMetric.start", {});
  AddExpectationAndroid("HttpMetric.stop", {});

  HttpMetric metric("https://google.com", kHttpMethodOptions);
}

TEST_F(HttpMetricTest, TestCreateTraceRequest) {
  AddExpectationApple("-[FIRHTTPMetric initWithUrl:HTTPMethod:]",
                      {"https://google.com", "TRACE"});
  AddExpectationApple("-[FIRHTTPMetric start]", {});
  AddExpectationApple("-[FIRHTTPMetric stop]", {});

  AddExpectationAndroid("new HttpMetric", {"https://google.com", "TRACE"});
  AddExpectationAndroid("HttpMetric.start", {});
  AddExpectationAndroid("HttpMetric.stop", {});

  HttpMetric metric("https://google.com", kHttpMethodTrace);
}

TEST_F(HttpMetricTest, TestCreateConnectRequest) {
  AddExpectationApple("-[FIRHTTPMetric initWithUrl:HTTPMethod:]",
                      {"https://google.com", "CONNECT"});
  AddExpectationApple("-[FIRHTTPMetric start]", {});
  AddExpectationApple("-[FIRHTTPMetric stop]", {});

  AddExpectationAndroid("new HttpMetric", {"https://google.com", "CONNECT"});
  AddExpectationAndroid("HttpMetric.start", {});
  AddExpectationAndroid("HttpMetric.stop", {});

  HttpMetric metric("https://google.com", kHttpMethodConnect);
}

TEST_F(HttpMetricTest, TestCreateDelayedGetRequest) {
  AddExpectationApple("-[FIRHTTPMetric initWithUrl:HTTPMethod:]",
                      {"https://google.com", "GET"});
  AddExpectationApple("-[FIRHTTPMetric start]", {});
  AddExpectationApple("-[FIRHTTPMetric stop]", {});

  AddExpectationAndroid("new HttpMetric", {"https://google.com", "GET"});
  AddExpectationAndroid("HttpMetric.start", {});
  AddExpectationAndroid("HttpMetric.stop", {});

  HttpMetric metric;
  metric.Start("https://google.com", kHttpMethodGet);
}

TEST_F(HttpMetricTest, TestCreateHttpMetricCppObject) {
  HttpMetric metric;

  // No expectations, it shouldn't call into the native implementation.
}

TEST_F(HttpMetricTest, TestCreateButNotStart) {
  HttpMetric metric;
  metric.Create("https://google.com", kHttpMethodGet);

  AddExpectationApple("-[FIRHTTPMetric initWithUrl:HTTPMethod:]",
                      {"https://google.com", "GET"});
  AddExpectationAndroid("new HttpMetric", {"https://google.com", "GET"});
}

TEST_F(HttpMetricTest, TestStartAfterCreate) {
  HttpMetric metric;
  metric.Create("https://google.com", kHttpMethodGet);
  metric.StartCreatedHttpMetric();

  AddExpectationApple("-[FIRHTTPMetric initWithUrl:HTTPMethod:]",
                      {"https://google.com", "GET"});
  AddExpectationApple("-[FIRHTTPMetric start]", {});
  // Stop isn't called as expected.

  AddExpectationAndroid("new HttpMetric", {"https://google.com", "GET"});
  AddExpectationAndroid("HttpMetric.start", {});
  // Stop isn't called as expected.
}

TEST_F(HttpMetricTest, TestCreateGetRequestNullUrl) {
  // No android or iOS expectation as it shouldn't even call through to the
  // native layers.

  EXPECT_NO_THROW(HttpMetric metric(nullptr, kHttpMethodGet));
}

TEST_F(HttpMetricTest, TestIsStarted) {
  AddExpectationApple("-[FIRHTTPMetric initWithUrl:HTTPMethod:]",
                      {"https://google.com", "GET"});
  AddExpectationApple("-[FIRHTTPMetric start]", {});

  AddExpectationAndroid("new HttpMetric", {"https://google.com", "GET"});
  AddExpectationAndroid("HttpMetric.start", {});

  HttpMetric metric("https://google.com", kHttpMethodGet);
  EXPECT_TRUE(metric.is_started());

  AddExpectationApple("-[FIRHTTPMetric stop]", {});
  AddExpectationAndroid("HttpMetric.stop", {});

  metric.Stop();
  EXPECT_FALSE(metric.is_started());
}

TEST_F(HttpMetricTest, TestSetAttribute) {
  HttpMetric metric("https://google.com", kHttpMethodGet);
  reporter_.reset();
  AddExpectationApple("-[FIRHTTPMetric setValue:forAttribute:]",
                      {"my_value", "my_attribute"});
  AddExpectationApple("-[FIRHTTPMetric stop]", {});

  AddExpectationAndroid("HttpMetric.putAttribute",
                        {"my_attribute", "my_value"});
  AddExpectationAndroid("HttpMetric.stop", {});

  metric.SetAttribute("my_attribute", "my_value");
}

TEST_F(HttpMetricTest, TestSetAttributeStoppedHttpMetric) {
  HttpMetric metric("https://google.com", kHttpMethodGet);
  metric.Stop();
  reporter_.reset();

  EXPECT_NO_THROW(metric.SetAttribute("my_attribute", "my_value"));
}

TEST_F(HttpMetricTest, TestSetAttributeNullAttributeName) {
  HttpMetric metric("https://google.com", kHttpMethodGet);
  metric.Stop();
  reporter_.reset();

  EXPECT_NO_THROW(metric.SetAttribute(nullptr, "my_value"));
  EXPECT_EQ(metric.GetAttribute(nullptr), "");
}

TEST_F(HttpMetricTest, TestGetAttribute) {
  HttpMetric metric("https://google.com", kHttpMethodGet);
  metric.SetAttribute("my_attribute", "my_value");
  reporter_.reset();

  AddExpectationApple("-[FIRHTTPMetric valueForAttribute:]", {"my_attribute"});
  AddExpectationApple("-[FIRHTTPMetric stop]", {});
  AddExpectationAndroid("HttpMetric.getAttribute", {"my_attribute"});
  AddExpectationAndroid("HttpMetric.stop", {});

  metric.GetAttribute("my_attribute");
}

TEST_F(HttpMetricTest, TestGetAttributeNull) {
  HttpMetric metric("https://google.com", kHttpMethodGet);
  metric.Stop();
  reporter_.reset();

  // Also ensures that this doesn't crash the process.
  EXPECT_EQ("", metric.GetAttribute(nullptr));
}

TEST_F(HttpMetricTest, TestGetAttributeStoppedHttpMetric) {
  HttpMetric metric("https://google.com", kHttpMethodGet);
  metric.Stop();
  reporter_.reset();

  // Also ensures that this doesn't crash the process.
  EXPECT_EQ("", metric.GetAttribute("my_attribute"));
}

TEST_F(HttpMetricTest, TestRemoveAttribute) {
  HttpMetric metric("https://google.com", kHttpMethodGet);
  reporter_.reset();
  AddExpectationApple("-[FIRHTTPMetric removeAttribute:]", {"my_attribute"});
  AddExpectationApple("-[FIRHTTPMetric stop]", {});

  AddExpectationAndroid("HttpMetric.removeAttribute", {"my_attribute"});
  AddExpectationAndroid("HttpMetric.stop", {});

  metric.SetAttribute("my_attribute", nullptr);
}

TEST_F(HttpMetricTest, TestRemoveAttributeStoppedHttpMetric) {
  HttpMetric metric("https://google.com", kHttpMethodGet);
  metric.Stop();
  reporter_.reset();

  EXPECT_NO_THROW(metric.SetAttribute("my_attribute", nullptr));
}

TEST_F(HttpMetricTest, TestSetHttpResponseCode) {
  HttpMetric metric("https://google.com", kHttpMethodGet);
  reporter_.reset();
  AddExpectationApple("-[FIRHTTPMetric setResponseCode:]", {"404"});
  AddExpectationApple("-[FIRHTTPMetric stop]", {});

  AddExpectationAndroid("HttpMetric.setHttpResponseCode", {"404"});
  AddExpectationAndroid("HttpMetric.stop", {});

  metric.set_http_response_code(404);
}

TEST_F(HttpMetricTest, TestSetHttpResponseCodeStoppedHttpMetric) {
  HttpMetric metric("https://google.com", kHttpMethodGet);
  metric.Stop();
  reporter_.reset();

  EXPECT_NO_THROW(metric.set_http_response_code(404));
}

TEST_F(HttpMetricTest, TestSetRequestPayloadSize) {
  HttpMetric metric("https://google.com", kHttpMethodGet);
  reporter_.reset();
  AddExpectationApple("-[FIRHTTPMetric setRequestPayloadSize:]", {"2000"});
  AddExpectationApple("-[FIRHTTPMetric stop]", {});

  AddExpectationAndroid("HttpMetric.setRequestPayloadSize", {"2000"});
  AddExpectationAndroid("HttpMetric.stop", {});

  metric.set_request_payload_size(2000);
}

TEST_F(HttpMetricTest, TestSetRequestPayloadSizeStoppedHttpMetric) {
  HttpMetric metric("https://google.com", kHttpMethodGet);
  metric.Stop();
  reporter_.reset();

  EXPECT_NO_THROW(metric.set_request_payload_size(2000));
}

TEST_F(HttpMetricTest, TestSetResponsePayloadSize) {
  HttpMetric metric("https://google.com", kHttpMethodGet);
  reporter_.reset();
  AddExpectationApple("-[FIRHTTPMetric setResponsePayloadSize:]", {"2000"});
  AddExpectationApple("-[FIRHTTPMetric stop]", {});

  AddExpectationAndroid("HttpMetric.setResponsePayloadSize", {"2000"});
  AddExpectationAndroid("HttpMetric.stop", {});

  metric.set_response_payload_size(2000);
}

TEST_F(HttpMetricTest, TestSetResponsePayloadSizeStoppedHttpMetric) {
  HttpMetric metric("https://google.com", kHttpMethodGet);
  metric.Stop();
  reporter_.reset();

  EXPECT_NO_THROW(metric.set_response_payload_size(2000));
}

TEST_F(HttpMetricTest, TestSetResponseContentType) {
  HttpMetric metric("https://google.com", kHttpMethodGet);
  reporter_.reset();
  AddExpectationApple("-[FIRHTTPMetric setResponseContentType:]",
                      {"application/json"});
  AddExpectationApple("-[FIRHTTPMetric stop]", {});

  AddExpectationAndroid("HttpMetric.setResponseContentType",
                        {"application/json"});
  AddExpectationAndroid("HttpMetric.stop", {});

  metric.set_response_content_type("application/json");
}

TEST_F(HttpMetricTest, TestSetResponseContentTypeNull) {
  HttpMetric metric("https://google.com", kHttpMethodGet);
  reporter_.reset();

  AddExpectationApple("-[FIRHTTPMetric stop]", {});
  AddExpectationAndroid("HttpMetric.stop", {});

  // This is a no-op.
  EXPECT_NO_THROW(metric.set_response_content_type(nullptr));
}

TEST_F(HttpMetricTest, TestSetResponseContentTypeStoppedHttpMetric) {
  HttpMetric metric("https://google.com", kHttpMethodGet);
  metric.Stop();
  reporter_.reset();

  EXPECT_NO_THROW(metric.set_response_content_type("application/json"));
}

}  // namespace performance
}  // namespace firebase
