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

// Exercises TransportCurl against a real localhost HTTP server (see
// test_http_server.h): sync and async requests, Controller pause / resume /
// cancel, timeouts, concurrency and teardown ordering.

#include "app/rest/transport_curl.h"

#include <atomic>
#include <chrono>  // NOLINT
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <thread>  // NOLINT
#include <vector>

#if !defined(_WIN32)
#include <fcntl.h>
#include <sys/resource.h>
#include <sys/select.h>
#include <unistd.h>
#endif

#include "app/rest/controller_interface.h"
#include "app/rest/curl/curl_io_loop.h"
#include "app/rest/request.h"
#include "app/rest/response.h"
#include "app/rest/tests/test_http_server.h"
#include "app/rest/util.h"
#include "app/src/semaphore.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace firebase {
namespace rest {
namespace {

constexpr int kWaitTimeoutMs = 10000;

// A Response that lets the test block until the transport has marked it
// completed or failed.
class TestResponse : public Response {
 public:
  TestResponse() : done_(0), completed_calls_(0), failed_calls_(0) {}

  void MarkCompleted() override {
    Response::MarkCompleted();
    completed_calls_++;
    completion_thread_was_io_thread_ =
        curl::CurlIoLoop::IsRunning() && curl::CurlIoLoop::Get().IsOnIoThread();
    done_.Post();
  }

  void MarkFailed() override {
    Response::MarkFailed();
    failed_calls_++;
    done_.Post();
  }

  // Waits until MarkCompleted() or MarkFailed() has been called.
  bool Wait(int timeout_ms = kWaitTimeoutMs) {
    return done_.TimedWait(timeout_ms);
  }

  int completed_calls() const { return completed_calls_.load(); }
  int failed_calls() const { return failed_calls_.load(); }
  bool completion_thread_was_io_thread() const {
    return completion_thread_was_io_thread_.load();
  }

 private:
  Semaphore done_;
  std::atomic<int> completed_calls_;
  std::atomic<int> failed_calls_;
  std::atomic<bool> completion_thread_was_io_thread_{false};
};

// A Request whose upload size is unknown up front, forcing
// "Transfer-Encoding: chunked" when streamed.
class ChunkedRequest : public Request {
 public:
  explicit ChunkedRequest(const std::string& body) : body_(body), offset_(0) {
    options_.stream_post_fields = true;
  }
  size_t GetPostFieldsSize() const override { return ~static_cast<size_t>(0); }
  size_t ReadBody(char* buffer, size_t length, bool* abort) override {
    *abort = false;
    size_t remaining = body_.size() - offset_;
    size_t to_copy = remaining < length ? remaining : length;
    // Trickle the body out to make sure multiple read callbacks happen.
    if (to_copy > 7) to_copy = 7;
    std::memcpy(buffer, body_.data() + offset_, to_copy);
    offset_ += to_copy;
    return to_copy;
  }

 private:
  std::string body_;
  size_t offset_;
};

class TransportCurlTest : public ::testing::Test {
 protected:
  static void SetUpTestSuite() {
    server_ = new test::TestHttpServer();
    ASSERT_TRUE(server_->Start());
  }
  static void TearDownTestSuite() {
    delete server_;
    server_ = nullptr;
  }

  void SetUp() override { InitTransportCurl(); }
  void TearDown() override { CleanupTransportCurl(); }

  static std::string Url(const char* path) { return server_->BaseUrl() + path; }

  static test::TestHttpServer* server_;
};

test::TestHttpServer* TransportCurlTest::server_ = nullptr;

TEST_F(TransportCurlTest, GlobalInitAndCleanupAreRefCounted) {
  curl::CurlIoLoop* loop = &curl::CurlIoLoop::Get();
  InitTransportCurl();
  InitTransportCurl();
  CleanupTransportCurl();
  EXPECT_EQ(loop, &curl::CurlIoLoop::Get());
  CleanupTransportCurl();
  EXPECT_EQ(loop, &curl::CurlIoLoop::Get());
  EXPECT_TRUE(curl::CurlIoLoop::IsRunning());
}

TEST_F(TransportCurlTest, Creation) {
  TransportCurl curl;
  EXPECT_FALSE(curl.is_async());
}

TEST_F(TransportCurlTest, HttpGetSync) {
  Request request;
  request.set_verbose(true);
  TestResponse response;
  EXPECT_EQ(0, response.status());
  EXPECT_FALSE(response.header_completed());
  EXPECT_FALSE(response.body_completed());
  EXPECT_EQ(nullptr, response.GetHeader("Server"));
  EXPECT_STREQ("", response.GetBody());

  request.set_url(Url("/").c_str());
  TransportCurl curl;
  curl.Perform(request, &response);
  // Synchronous mode: everything is already there when Perform() returns.
  EXPECT_EQ(200, response.status());
  EXPECT_TRUE(response.header_completed());
  EXPECT_TRUE(response.body_completed());
  EXPECT_STREQ(test::TestHttpServer::kServerName(),
               response.GetHeader("Server"));
  EXPECT_STREQ(test::TestHttpServer::kServerName(),
               response.GetHeader("server"));
  EXPECT_STREQ("test", response.GetBody());
  EXPECT_TRUE(request.completed());
  EXPECT_EQ(1, response.completed_calls());
  EXPECT_EQ(0, response.failed_calls());
  EXPECT_TRUE(response.completion_thread_was_io_thread());
}

TEST_F(TransportCurlTest, HttpPostJsonEcho) {
  Request request;
  request.set_verbose(true);
  TestResponse response;

  request.set_url(Url("/echo").c_str());
  request.set_method("POST");
  request.add_header("Content-Type", "application/json");
  request.set_post_fields("{'a':'a','b':'b'}");
  TransportCurl curl;
  curl.Perform(request, &response);
  EXPECT_EQ(200, response.status());
  EXPECT_TRUE(response.header_completed());
  EXPECT_TRUE(response.body_completed());
  EXPECT_STREQ(test::TestHttpServer::kServerName(),
               response.GetHeader("Server"));
  EXPECT_STREQ("{'a':'a','b':'b'}", response.GetBody());

  test::RecordedRequest seen = server_->last_request();
  EXPECT_EQ("POST", seen.method);
  EXPECT_EQ("/echo", seen.path);
  EXPECT_EQ("application/json", seen.headers["content-type"]);
  EXPECT_EQ("17", seen.headers["content-length"]);
}

TEST_F(TransportCurlTest, HttpPostStreamedWithKnownSize) {
  Request request;
  request.set_url(Url("/echo").c_str());
  request.set_method("POST");
  request.set_post_fields("streamed body");
  request.options().stream_post_fields = true;
  TestResponse response;
  TransportCurl curl;
  curl.Perform(request, &response);
  EXPECT_EQ(200, response.status());
  EXPECT_STREQ("streamed body", response.GetBody());
  EXPECT_EQ("13", server_->last_request().headers["content-length"]);
}

TEST_F(TransportCurlTest, HttpPostStreamedChunked) {
  const std::string body = "0123456789abcdefghijklmnopqrstuvwxyz chunked!";
  ChunkedRequest request(body);
  request.set_url(Url("/echo").c_str());
  request.set_method("POST");
  TestResponse response;
  TransportCurl curl;
  curl.Perform(request, &response);
  EXPECT_EQ(200, response.status());
  EXPECT_EQ(body, response.GetBody());
  test::RecordedRequest seen = server_->last_request();
  EXPECT_EQ("chunked", seen.headers["transfer-encoding"]);
  EXPECT_EQ(body, seen.body);
}

TEST_F(TransportCurlTest, CustomMethodDoesNotLeakIntoNextRequest) {
  TransportCurl curl;
  {
    Request request;
    request.set_url(Url("/").c_str());
    request.set_method("PUT");
    TestResponse response;
    curl.Perform(request, &response);
    EXPECT_EQ(200, response.status());
    EXPECT_STREQ("PUT", response.GetBody());
    EXPECT_EQ("PUT", server_->last_request().method);
  }
  {
    // Same TransportCurl (same easy handle): the custom method must be gone.
    Request request;
    request.set_url(Url("/").c_str());
    TestResponse response;
    curl.Perform(request, &response);
    EXPECT_EQ(200, response.status());
    EXPECT_STREQ("test", response.GetBody());
    EXPECT_EQ("GET", server_->last_request().method);
  }
  {
    // And a POST after a GET really posts.
    Request request;
    request.set_url(Url("/").c_str());
    request.set_method("POST");
    request.set_post_fields("again");
    TestResponse response;
    curl.Perform(request, &response);
    EXPECT_EQ(200, response.status());
    EXPECT_STREQ("again", response.GetBody());
  }
}

TEST_F(TransportCurlTest, HttpStatusIsReported) {
  Request request;
  request.set_url(Url("/status/404").c_str());
  TestResponse response;
  TransportCurl curl;
  curl.Perform(request, &response);
  EXPECT_EQ(404, response.status());
  EXPECT_EQ(1, response.completed_calls());
}

TEST_F(TransportCurlTest, HttpGetAsync) {
  Request request;
  request.set_url(Url("/slow?ms=200").c_str());
  TestResponse response;
  TransportCurl curl;
  curl.set_is_async(true);
  auto start = std::chrono::steady_clock::now();
  curl.Perform(request, &response);
  auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - start)
                        .count();
  EXPECT_LT(elapsed_ms, 200);  // Returned before the server replied.
  ASSERT_TRUE(response.Wait());
  EXPECT_EQ(200, response.status());
  EXPECT_STREQ("test", response.GetBody());
}

TEST_F(TransportCurlTest, ConnectionRefusedCompletesWithoutStatus) {
  // Port 1 (tcpmux) on loopback refuses connections immediately on every
  // desktop platform. libcurl reports CURLE_COULDNT_CONNECT, which the
  // transport surfaces as MarkCompleted() with status 0 (no HTTP response).
  Request request;
  request.set_url("http://127.0.0.1:1/");
  TestResponse response;
  TransportCurl curl;
  curl.Perform(request, &response);
  EXPECT_EQ(0, response.status());
  EXPECT_EQ(1, response.completed_calls());
}

TEST_F(TransportCurlTest, ControllerReportsProgress) {
  const size_t kSize = 2 * 1024 * 1024;
  Request request;
  request.set_url(Url("/large?size=2097152").c_str());
  TestResponse response;
  TransportCurl curl;
  flatbuffers::unique_ptr<Controller> controller;
  curl.Perform(request, &response, &controller);
  ASSERT_NE(nullptr, controller.get());
  EXPECT_EQ(200, response.status());
  EXPECT_EQ(static_cast<int64_t>(kSize), controller->TransferSize());
  EXPECT_EQ(static_cast<int64_t>(kSize), controller->BytesTransferred());
  EXPECT_FLOAT_EQ(1.0f, controller->Progress());
  EXPECT_FALSE(controller->IsPaused());
  // The transfer is over: nothing left to pause or cancel.
  EXPECT_FALSE(controller->Pause());
  EXPECT_FALSE(controller->Cancel());
}

TEST_F(TransportCurlTest, ControllerReportsUploadProgress) {
  const std::string body(100000, 'u');
  Request request;
  request.set_url(Url("/echo").c_str());
  request.set_method("POST");
  request.set_post_fields(body.c_str());
  TestResponse response;
  TransportCurl curl;
  flatbuffers::unique_ptr<Controller> controller;
  curl.Perform(request, &response, &controller);
  EXPECT_EQ(200, response.status());
  EXPECT_EQ(static_cast<int64_t>(body.size()), controller->TransferSize());
  EXPECT_EQ(static_cast<int64_t>(body.size()), controller->BytesTransferred());
}

TEST_F(TransportCurlTest, ControllerPauseAndResume) {
  Request request;
  request.set_url(Url("/slow?ms=300").c_str());
  TestResponse response;
  TransportCurl curl;
  curl.set_is_async(true);
  flatbuffers::unique_ptr<Controller> controller;
  curl.Perform(request, &response, &controller);
  ASSERT_NE(nullptr, controller.get());

  EXPECT_TRUE(controller->Pause());
  EXPECT_TRUE(controller->IsPaused());
  EXPECT_FALSE(controller->Pause());  // Already paused.
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  EXPECT_TRUE(controller->Resume());
  EXPECT_FALSE(controller->IsPaused());
  EXPECT_FALSE(controller->Resume());  // Not paused.

  ASSERT_TRUE(response.Wait());
  EXPECT_EQ(200, response.status());
  EXPECT_STREQ("test", response.GetBody());
}

TEST_F(TransportCurlTest, ControllerCancelInFlight) {
  Request request;
  request.set_url(Url("/slow?ms=5000").c_str());
  TestResponse response;
  TransportCurl curl;
  curl.set_is_async(true);
  flatbuffers::unique_ptr<Controller> controller;
  curl.Perform(request, &response, &controller);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  auto start = std::chrono::steady_clock::now();
  EXPECT_TRUE(controller->Cancel());
  ASSERT_TRUE(response.Wait());
  auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - start)
                        .count();
  EXPECT_LT(elapsed_ms, 4000);  // Did not wait for the slow server.
  EXPECT_EQ(util::HttpNoContent, response.status());
  EXPECT_EQ(0, response.completed_calls());
  EXPECT_EQ(1, response.failed_calls());
  EXPECT_FALSE(response.header_completed());
  EXPECT_FALSE(response.body_completed());
  EXPECT_FALSE(request.completed());

  // A second cancel is a harmless no-op.
  EXPECT_FALSE(controller->Cancel());
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  EXPECT_EQ(1, response.failed_calls());
}

TEST_F(TransportCurlTest, ControllerCancelBeforeStart) {
  Request request;
  request.set_url(Url("/slow?ms=5000").c_str());
  TestResponse response;
  TransportCurl curl;
  curl.set_is_async(true);
  flatbuffers::unique_ptr<Controller> controller;
  curl.Perform(request, &response, &controller);
  // No delay: the I/O thread has most likely not even picked the transfer up.
  EXPECT_TRUE(controller->Cancel());
  ASSERT_TRUE(response.Wait());
  EXPECT_EQ(util::HttpNoContent, response.status());
  EXPECT_EQ(1, response.failed_calls());
  EXPECT_FALSE(request.completed());
}

TEST_F(TransportCurlTest, ControllerDestroyedBeforeTransferCompletes) {
  Request request;
  request.set_url(Url("/slow?ms=300").c_str());
  TestResponse response;
  TransportCurl curl;
  curl.set_is_async(true);
  {
    flatbuffers::unique_ptr<Controller> controller;
    curl.Perform(request, &response, &controller);
    // Dropping the controller must not cancel or crash the transfer.
  }
  ASSERT_TRUE(response.Wait());
  EXPECT_EQ(200, response.status());
}

TEST_F(TransportCurlTest, TimeoutFailsWithRequestTimeout) {
  Request request;
  request.set_url(Url("/slow?ms=5000").c_str());
  request.options().timeout_ms = 300;
  TestResponse response;
  TransportCurl curl;
  auto start = std::chrono::steady_clock::now();
  curl.Perform(request, &response);
  auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - start)
                        .count();
  EXPECT_GE(elapsed_ms, 250);
  EXPECT_LT(elapsed_ms, 4000);
  EXPECT_EQ(util::HttpRequestTimeout, response.status());
  EXPECT_EQ(0, response.completed_calls());
  EXPECT_EQ(1, response.failed_calls());
  EXPECT_FALSE(request.completed());
}

TEST_F(TransportCurlTest, ManyConcurrentTransportsComplete) {
  const int kCount = 16;
  std::vector<std::unique_ptr<Request>> requests;
  std::vector<std::unique_ptr<TestResponse>> responses;
  std::vector<std::unique_ptr<TransportCurl>> transports;
  auto start = std::chrono::steady_clock::now();
  for (int i = 0; i < kCount; ++i) {
    requests.push_back(std::make_unique<Request>());
    requests.back()->set_url(Url("/slow?ms=200").c_str());
    responses.push_back(std::make_unique<TestResponse>());
    transports.push_back(std::make_unique<TransportCurl>());
    transports.back()->set_is_async(true);
    transports.back()->Perform(*requests.back(), responses.back().get());
  }
  for (int i = 0; i < kCount; ++i) {
    ASSERT_TRUE(responses[i]->Wait()) << "transfer " << i;
    EXPECT_EQ(200, responses[i]->status()) << "transfer " << i;
    EXPECT_STREQ("test", responses[i]->GetBody()) << "transfer " << i;
  }
  auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - start)
                        .count();
  // Serialized they would take >= 16 * 200 ms.
  EXPECT_LT(elapsed_ms, kCount * 200);
}

TEST_F(TransportCurlTest, SequentialRequestsOnOneTransport) {
  TransportCurl curl;
  for (int i = 0; i < 5; ++i) {
    Request request;
    request.set_url(Url("/").c_str());
    TestResponse response;
    curl.Perform(request, &response);
    EXPECT_EQ(200, response.status()) << "request " << i;
    EXPECT_STREQ("test", response.GetBody()) << "request " << i;
  }
}

TEST_F(TransportCurlTest, DestructorWaitsForInFlightRequest) {
  Request request;
  request.set_url(Url("/slow?ms=300").c_str());
  TestResponse response;
  auto start = std::chrono::steady_clock::now();
  {
    TransportCurl curl;
    curl.set_is_async(true);
    flatbuffers::unique_ptr<Controller> controller;
    curl.Perform(request, &response, &controller);
    // ~Controller() runs first (reverse declaration order) while the transfer
    // is still in flight, then ~TransportCurl() must block until it is done.
  }
  auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - start)
                        .count();
  EXPECT_GE(elapsed_ms, 250);
  ASSERT_TRUE(response.Wait());
  EXPECT_EQ(200, response.status());
  EXPECT_STREQ("test", response.GetBody());
}

TEST_F(TransportCurlTest, CleanupWithInFlightRequestFailsIt) {
  struct ReinitGuard {
    ~ReinitGuard() {
      if (!curl::CurlIoLoop::IsRunning()) {
        InitTransportCurl();
      }
    }
  } reinit_guard;

  Request request;
  request.set_url(Url("/slow?ms=5000").c_str());
  TestResponse response;
  TransportCurl curl;
  curl.set_is_async(true);
  curl.Perform(request, &response);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // Drop the fixture's (only) reference: the loop shuts down and must fail the
  // in-flight transfer before returning.
  auto start = std::chrono::steady_clock::now();
  CleanupTransportCurl();
  auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - start)
                        .count();
  EXPECT_LT(elapsed_ms, 4000);
  EXPECT_FALSE(curl::CurlIoLoop::IsRunning());
  ASSERT_TRUE(response.Wait());
  EXPECT_EQ(util::HttpNoContent, response.status());
  EXPECT_EQ(1, response.failed_calls());
}

TEST_F(TransportCurlTest, FastRequestsNotDelayedByInFlightSlowRequest) {
  // Keep one slow transfer parked in curl_multi_poll() so the old select() +
  // 33 ms polling loop would stall new requests waiting for its poll cycle.
  Request slow_request;
  slow_request.set_url(Url("/slow?ms=2000").c_str());
  TestResponse slow_response;
  TransportCurl slow_transport;
  slow_transport.set_is_async(true);
  slow_transport.Perform(slow_request, &slow_response);
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  TransportCurl fast_transport;
  auto start = std::chrono::steady_clock::now();
  for (int i = 0; i < 10; ++i) {
    Request fast_request;
    fast_request.set_url(Url("/").c_str());
    TestResponse fast_response;
    fast_transport.Perform(fast_request, &fast_response);
    EXPECT_EQ(200, fast_response.status()) << "request " << i;
    EXPECT_STREQ("test", fast_response.GetBody()) << "request " << i;
  }
  auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - start)
                        .count();
  // 10 sequential wakeups + loopback HTTP round trips complete in single-digit
  // milliseconds via curl_multi_wakeup() (keep bound generous for CI).
  EXPECT_LT(elapsed_ms, 500);

  ASSERT_TRUE(slow_response.Wait());
  EXPECT_EQ(200, slow_response.status());
}

TEST_F(TransportCurlTest, PerformAndCancelAfterCleanupDoNotCrash) {
  struct ReinitGuard {
    ~ReinitGuard() {
      if (!curl::CurlIoLoop::IsRunning()) {
        InitTransportCurl();
      }
    }
  } reinit_guard;

  // Start a request and obtain a controller while the loop is running, then
  // shut down the loop via CleanupTransportCurl() and verify that calling
  // Cancel()/Pause()/Resume() on the controller or starting a new Perform()
  // after cleanup fails cleanly without asserting or crashing.
  Request in_flight_request;
  in_flight_request.set_url(Url("/slow?ms=5000").c_str());
  TestResponse in_flight_response;
  Request post_cleanup_request;
  post_cleanup_request.set_url(Url("/").c_str());
  TestResponse post_cleanup_response;

  TransportCurl curl;
  curl.set_is_async(true);
  flatbuffers::unique_ptr<Controller> in_flight_controller;
  curl.Perform(in_flight_request, &in_flight_response, &in_flight_controller);
  ASSERT_NE(nullptr, in_flight_controller.get());

  CleanupTransportCurl();
  EXPECT_FALSE(curl::CurlIoLoop::IsRunning());
  ASSERT_TRUE(in_flight_response.Wait());
  EXPECT_EQ(util::HttpNoContent, in_flight_response.status());
  EXPECT_EQ(1, in_flight_response.failed_calls());

  // Calling controller methods after CleanupTransportCurl() must be a safe
  // no-op.
  EXPECT_FALSE(in_flight_controller->Pause());
  EXPECT_FALSE(in_flight_controller->Resume());
  EXPECT_FALSE(in_flight_controller->Cancel());

  // Calling Perform() after CleanupTransportCurl() must immediately fail the
  // response via AbortWithoutLoop() instead of asserting in CurlIoLoop::Get().
  flatbuffers::unique_ptr<Controller> post_cleanup_controller;
  curl.Perform(post_cleanup_request, &post_cleanup_response,
               &post_cleanup_controller);
  ASSERT_TRUE(post_cleanup_response.Wait());
  EXPECT_EQ(util::HttpNoContent, post_cleanup_response.status());
  EXPECT_EQ(1, post_cleanup_response.failed_calls());
  ASSERT_NE(nullptr, post_cleanup_controller.get());
  EXPECT_FALSE(post_cleanup_controller->Cancel());
}

TEST_F(TransportCurlTest, ConcurrentOverlappingAsyncPerformsOnSingleTransport) {
  Request req1;
  req1.set_url(Url("/slow?ms=200").c_str());
  TestResponse resp1;

  Request req2;
  req2.set_url(Url("/echo").c_str());
  req2.set_method("POST");
  req2.set_post_fields("concurrent-second-transfer");
  TestResponse resp2;

  TransportCurl curl;
  curl.set_is_async(true);

  auto start = std::chrono::steady_clock::now();
  curl.Perform(req1, &resp1);
  curl.Perform(req2, &resp2);

  ASSERT_TRUE(resp2.Wait());
  ASSERT_TRUE(resp1.Wait());
  auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - start)
                        .count();
  EXPECT_EQ(200, resp1.status());
  EXPECT_STREQ("test", resp1.GetBody());
  EXPECT_EQ(200, resp2.status());
  EXPECT_STREQ("concurrent-second-transfer", resp2.GetBody());
  EXPECT_LT(elapsed_ms, 2000);
}

TEST_F(TransportCurlTest,
       StaleControllerCancelDoesNotAffectNewTransferAtSameResponseAddress) {
  using PlacementPtr =
      std::unique_ptr<TestResponse, void (*)(TestResponse*)>;
  auto destroy_placement = +[](TestResponse* p) {
    if (p) p->~TestResponse();
  };

  alignas(TestResponse) unsigned char response_storage[sizeof(TestResponse)];
  flatbuffers::unique_ptr<Controller> first_controller;

  {
    PlacementPtr first_response(new (response_storage) TestResponse(),
                                destroy_placement);
    Request first_request;
    first_request.set_url(Url("/").c_str());
    TransportCurl curl;
    curl.set_is_async(true);
    curl.Perform(first_request, first_response.get(), &first_controller);
    ASSERT_TRUE(first_response->Wait());
    EXPECT_EQ(200, first_response->status());
  }

  // Construct a new TestResponse at the exact same memory address and start a
  // slow transfer. Canceling first_controller must not cancel the new transfer.
  PlacementPtr second_response(new (response_storage) TestResponse(),
                               destroy_placement);
  Request second_request;
  second_request.set_url(Url("/slow?ms=200").c_str());
  flatbuffers::unique_ptr<Controller> second_controller;
  TransportCurl curl;
  curl.set_is_async(true);
  curl.Perform(second_request, second_response.get(), &second_controller);

  EXPECT_FALSE(first_controller->Cancel());
  ASSERT_TRUE(second_response->Wait());
  EXPECT_EQ(200, second_response->status());
  EXPECT_STREQ("test", second_response->GetBody());
}

#if !defined(_WIN32)
TEST_F(TransportCurlTest, HighFileDescriptorsAboveFdSetSizeDoNotStall) {
  // Large desktop game engines (Unreal / Unity) commonly hold >1,024 open file
  // descriptors (.pak archives, shader caches, audio banks). Under the legacy
  // curl_multi_fdset() + select() loop, any socket with fd >= FD_SETSIZE (1024)
  // was omitted from fd_set: if an earlier socket had fd < 1024, select()
  // blocked completely blind to the high-FD transfers; otherwise maxfd == -1
  // degraded into 33 ms sleep ticks. Verify curl_multi_poll() handles sockets
  // above FD_SETSIZE with zero stall even alongside a low-FD transfer.
  struct rlimit orig_limit {};
  ASSERT_EQ(0, getrlimit(RLIMIT_NOFILE, &orig_limit));
  const rlim_t kNeeded = static_cast<rlim_t>(FD_SETSIZE + 128);
  bool raised_limit = false;
  if (orig_limit.rlim_cur < kNeeded && orig_limit.rlim_max >= kNeeded) {
    struct rlimit raised = orig_limit;
    raised.rlim_cur =
        orig_limit.rlim_max < static_cast<rlim_t>(FD_SETSIZE + 256)
            ? orig_limit.rlim_max
            : static_cast<rlim_t>(FD_SETSIZE + 256);
    if (setrlimit(RLIMIT_NOFILE, &raised) == 0) {
      raised_limit = true;
    }
  }
  struct ScopedFdCleanup {
    std::vector<int> fds;
    bool restore_rlimit = false;
    struct rlimit saved {};
    ~ScopedFdCleanup() {
      for (int fd : fds) close(fd);
      if (restore_rlimit) setrlimit(RLIMIT_NOFILE, &saved);
    }
  } cleanup;
  cleanup.restore_rlimit = raised_limit;
  cleanup.saved = orig_limit;

  struct rlimit active_limit {};
  ASSERT_EQ(0, getrlimit(RLIMIT_NOFILE, &active_limit));
  if (active_limit.rlim_cur < kNeeded) {
    GTEST_SKIP() << "RLIMIT_NOFILE (" << active_limit.rlim_cur
                 << ") is below FD_SETSIZE + 128 (" << kNeeded << ")";
  }

  // 1. Start a slow request first so it acquires a low FD (< FD_SETSIZE).
  Request slow_request;
  slow_request.set_url(Url("/slow?ms=2000").c_str());
  TestResponse slow_response;
  TransportCurl slow_transport;
  slow_transport.set_is_async(true);
  slow_transport.Perform(slow_request, &slow_response);
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  // 2. Reserve dummy file descriptors past FD_SETSIZE so subsequent sockets
  // receive fd >= FD_SETSIZE + 32.
  const int kTargetFd = FD_SETSIZE + 32;
  cleanup.fds.reserve(kTargetFd + 16);
  int max_opened_fd = -1;
  while (max_opened_fd < kTargetFd) {
    int fd = open("/dev/null", O_RDONLY);
    if (fd < 0) break;
    cleanup.fds.push_back(fd);
    if (fd > max_opened_fd) max_opened_fd = fd;
  }
  ASSERT_GE(max_opened_fd, kTargetFd);

  // 3. Perform 8 fast requests whose sockets live above FD_SETSIZE.
  auto start = std::chrono::steady_clock::now();
  for (int i = 0; i < 8; ++i) {
    TransportCurl fast_transport;
    Request fast_request;
    fast_request.set_url(Url("/").c_str());
    TestResponse fast_response;
    fast_transport.Perform(fast_request, &fast_response);
    EXPECT_EQ(200, fast_response.status()) << "high-FD request " << i;
    EXPECT_STREQ("test", fast_response.GetBody()) << "high-FD request " << i;
  }
  auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - start)
                        .count();
  EXPECT_LT(elapsed_ms, 500);

  ASSERT_TRUE(slow_response.Wait());
  EXPECT_EQ(200, slow_response.status());
}
#endif  // !defined(_WIN32)

}  // namespace
}  // namespace rest
}  // namespace firebase
