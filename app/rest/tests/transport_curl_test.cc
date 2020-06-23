/*
 * Copyright 2017 Google LLC
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

// This is a large test that starts a local http server and tests transport_curl
// with actual http connection.

#include "app/rest/transport_curl.h"

#include <cstdio>
#include <string>

#include "app/rest/request.h"
#include "app/rest/response.h"
#include "net/http2/server/lib/public/httpserver2.h"
#include "net/util/ports.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "absl/strings/str_format.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "util/task/status.h"

namespace firebase {
namespace rest {

const char* kServerVersion = "HTTP server for test";

void UriHandler(HTTPServerRequest* request) {
  if (request->http_method() == "GET") {
    request->output()->WriteString("test");
    request->Reply();
    LOG(INFO) << "Sent response for GET";
  } else if (request->http_method() == "POST" &&
             request->input_headers()->HeaderIs("Content-Type",
                                                "application/json")) {
    request->output()->WriteString(request->input()->ToString());
    request->Reply();
    LOG(INFO) << "Sent response for POST";
  } else {
    FAIL();
  }
}

const absl::Duration kTimeoutSeconds = absl::Seconds(10);

class TestResponse : public Response {
 public:
  void MarkCompleted() override {
    absl::MutexLock lock(&mutex_);
    Response::MarkCompleted();
  }

  void Wait() {
    absl::MutexLock lock(&mutex_);
    mutex_.AwaitWithTimeout(
        absl::Condition(
            [](void* userdata) -> bool {
              auto* response = static_cast<TestResponse*>(userdata);
              return response->header_completed() && response->body_completed();
            },
            this),
        kTimeoutSeconds);
  }

 private:
  absl::Mutex mutex_;
};

class TransportCurlTest : public testing::Test {
 protected:
  static void SetUpTestSuite() {
    InitTransportCurl();
    // Start a local http server for testing the http request.
    // Pick up a port.
    std::string error;  // PickUnusedPort actually asks for google3 string.
    TransportCurlTest::port_ = net_util::PickUnusedPort(&error);
    CHECK_GE(TransportCurlTest::port_, 0) << error;
    LOG(INFO) << "Auto selected port " << port_ << " for test http server";
    // Create a new server.
    std::unique_ptr<net_http2::HTTPServer2::EventModeOptions> options(
        new net_http2::HTTPServer2::EventModeOptions());
    options->SetVersion(kServerVersion);
    options->SetDataVersion("data_1.0");
    options->SetServerType("server");
    options->AddPort(TransportCurlTest::port_);
    options->SetWindowSizesAndLatency(0, 0, true);
    auto creation_status = net_http2::HTTPServer2::CreateEventDrivenModeServer(
        nullptr /* event manager */, std::move(options));
    CHECK_OK(creation_status.status());
    // Register URI handler and start serving.
    TransportCurlTest::server_ = creation_status.value().release();
    ABSL_DIE_IF_NULL(TransportCurlTest::server_)
        ->RegisterHandler("*", NewPermanentCallback(&UriHandler));
    CHECK_OK(TransportCurlTest::server_->StartAcceptingRequests());
    LOG(INFO) << "Local HTTP server is ready to accept request";
  }
  static void TearDownTestSuite() {
    TransportCurlTest::server_->TerminateServer();
    delete TransportCurlTest::server_;
    TransportCurlTest::server_ = nullptr;
    CleanupTransportCurl();
  }
  static int32 port_;
  static net_http2::HTTPServer2* server_;
};

int32 TransportCurlTest::port_;
net_http2::HTTPServer2* TransportCurlTest::server_;

TEST_F(TransportCurlTest, TestGlobalInitAndCleanup) {
  InitTransportCurl();
  CleanupTransportCurl();
}

TEST_F(TransportCurlTest, TestCreation) { TransportCurl curl; }

TEST_F(TransportCurlTest, TestHttpGet) {
  Request request;
  request.set_verbose(true);
  TestResponse response;
  EXPECT_EQ(0, response.status());
  EXPECT_FALSE(response.header_completed());
  EXPECT_FALSE(response.body_completed());
  EXPECT_EQ(nullptr, response.GetHeader("Server"));
  EXPECT_STREQ("", response.GetBody());

  const std::string& url =
      absl::StrFormat("http://localhost:%d", TransportCurlTest::port_);
  request.set_url(url.c_str());
  TransportCurl curl;
  curl.Perform(request, &response);
  response.Wait();
  EXPECT_EQ(200, response.status());
  EXPECT_TRUE(response.header_completed());
  EXPECT_TRUE(response.body_completed());
  EXPECT_STREQ(kServerVersion, response.GetHeader("Server"));
  EXPECT_STREQ("test", response.GetBody());
}

TEST_F(TransportCurlTest, TestHttpPost) {
  Request request;
  request.set_verbose(true);
  TestResponse response;
  EXPECT_EQ(0, response.status());
  EXPECT_FALSE(response.header_completed());
  EXPECT_FALSE(response.body_completed());
  EXPECT_EQ(nullptr, response.GetHeader("Server"));
  EXPECT_STREQ("", response.GetBody());

  const std::string& url =
      absl::StrFormat("http://localhost:%d", TransportCurlTest::port_);
  request.set_url(url.c_str());
  request.set_method("POST");
  request.add_header("Content-Type", "application/json");
  request.set_post_fields("{'a':'a','b':'b'}");
  TransportCurl curl;
  curl.Perform(request, &response);
  response.Wait();
  EXPECT_EQ(200, response.status());
  EXPECT_TRUE(response.header_completed());
  EXPECT_TRUE(response.body_completed());
  EXPECT_STREQ(kServerVersion, response.GetHeader("Server"));
  EXPECT_STREQ("{'a':'a','b':'b'}", response.GetBody());
}

}  // namespace rest
}  // namespace firebase
