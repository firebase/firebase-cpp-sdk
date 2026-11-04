/*
 * Copyright 2026 Google LLC
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

#include "app/rest/curl/curl_io_loop.h"

#include <atomic>
#include <chrono>  // NOLINT
#include <string>
#include <thread>  // NOLINT
#include <vector>

#include "app/rest/tests/test_http_server.h"
#include "app/src/semaphore.h"
#include "curl/curl.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace firebase {
namespace rest {
namespace curl {
namespace {

using ::testing::ElementsAre;

constexpr int kWaitTimeoutMs = 10000;

// Blocks until the I/O thread has run everything posted before this call.
void Drain(CurlIoLoop& loop) {
  Semaphore done(0);
  loop.Post([&done]() { done.Post(); });
  ASSERT_TRUE(done.TimedWait(kWaitTimeoutMs));
}

// A Transfer that owns its own easy handle, fetches a URL, discards the body
// and records OnDone().
class TestTransfer : public Transfer {
 public:
  explicit TestTransfer(const std::string& url)
      : easy_(curl_easy_init()),
        done_(0),
        done_called_(false),
        result_(CURLE_OK),
        http_status_(0) {
    curl_easy_setopt(easy_.get(), CURLOPT_URL, url.c_str());
    curl_easy_setopt(easy_.get(), CURLOPT_WRITEFUNCTION, &DiscardBody);
    curl_easy_setopt(easy_.get(), CURLOPT_WRITEDATA, static_cast<void*>(this));
  }

  CURL* easy() override { return easy_.get(); }

  void OnDone(CURLcode result) override {
    done_called_ = true;
    result_ = result;
    long status = 0;  // NOLINT
    curl_easy_getinfo(easy_.get(), CURLINFO_RESPONSE_CODE, &status);
    http_status_ = static_cast<int>(status);
    on_done_thread_was_io_thread_ = CurlIoLoop::Get().IsOnIoThread();
    done_.Post();
  }

  bool WaitForDone(int timeout_ms = kWaitTimeoutMs) {
    return done_.TimedWait(timeout_ms);
  }
  bool done_called() const { return done_called_.load(); }
  CURLcode result() const { return result_; }
  int http_status() const { return http_status_; }
  bool on_done_thread_was_io_thread() const {
    return on_done_thread_was_io_thread_;
  }

 private:
  static size_t DiscardBody(char*, size_t size, size_t nmemb, void*) {
    return size * nmemb;
  }

  UniqueEasy easy_;
  Semaphore done_;
  std::atomic<bool> done_called_;
  CURLcode result_;
  int http_status_;
  bool on_done_thread_was_io_thread_ = false;
};

class CurlIoLoopTest : public ::testing::Test {
 protected:
  static void SetUpTestSuite() {
    server_ = new test::TestHttpServer();
    ASSERT_TRUE(server_->Start());
  }
  static void TearDownTestSuite() {
    delete server_;
    server_ = nullptr;
  }

  void SetUp() override { CurlIoLoop::Acquire(); }
  void TearDown() override { CurlIoLoop::Release(); }

  static test::TestHttpServer* server_;
};

test::TestHttpServer* CurlIoLoopTest::server_ = nullptr;

TEST_F(CurlIoLoopTest, AcquireStartsThreadAndReleaseStopsIt) {
  EXPECT_TRUE(CurlIoLoop::IsRunning());
  EXPECT_FALSE(CurlIoLoop::Get().IsOnIoThread());
}

TEST_F(CurlIoLoopTest, AcquireReleaseIsReentrant) {
  // The fixture already holds one reference.
  CurlIoLoop* first = &CurlIoLoop::Get();
  CurlIoLoop::Acquire();
  CurlIoLoop::Acquire();
  EXPECT_EQ(first, &CurlIoLoop::Get());
  CurlIoLoop::Release();
  EXPECT_TRUE(CurlIoLoop::IsRunning());
  EXPECT_EQ(first, &CurlIoLoop::Get());
  CurlIoLoop::Release();
  EXPECT_TRUE(CurlIoLoop::IsRunning());
  EXPECT_EQ(first, &CurlIoLoop::Get());

  // Dropping the last reference really stops the thread...
  CurlIoLoop::Release();
  EXPECT_FALSE(CurlIoLoop::IsRunning());
  // ...and re-acquiring re-initializes libcurl and starts a fresh loop
  // (balanced by TearDown).
  CurlIoLoop::Acquire();
  EXPECT_TRUE(CurlIoLoop::IsRunning());
  Drain(CurlIoLoop::Get());
  TestTransfer transfer(server_->BaseUrl() + "/");
  CurlIoLoop::Get().Post(
      [&]() { ASSERT_EQ(CURLM_OK, CurlIoLoop::Get().Add(transfer)); });
  ASSERT_TRUE(transfer.WaitForDone());
  EXPECT_EQ(200, transfer.http_status());
}

TEST_F(CurlIoLoopTest, PostRunsOnIoThreadInFifoOrder) {
  CurlIoLoop& loop = CurlIoLoop::Get();
  std::vector<int> order;
  std::atomic<int> on_io_thread_count{0};
  const int kCount = 100;
  for (int i = 0; i < kCount; ++i) {
    loop.Post([&, i]() {
      if (loop.IsOnIoThread()) on_io_thread_count++;
      order.push_back(i);
    });
  }
  Drain(loop);
  ASSERT_EQ(kCount, static_cast<int>(order.size()));
  for (int i = 0; i < kCount; ++i) EXPECT_EQ(i, order[i]);
  EXPECT_EQ(kCount, on_io_thread_count.load());
}

TEST_F(CurlIoLoopTest, PostWakesTheThreadPromptly) {
  CurlIoLoop& loop = CurlIoLoop::Get();
  // Let the loop settle into curl_multi_poll() first.
  Drain(loop);
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  int64_t worst_us = 0;
  for (int i = 0; i < 5; ++i) {
    Semaphore ran(0);
    auto start = std::chrono::steady_clock::now();
    loop.Post([&ran]() { ran.Post(); });
    ASSERT_TRUE(ran.TimedWait(kWaitTimeoutMs));
    int64_t elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(
                             std::chrono::steady_clock::now() - start)
                             .count();
    if (elapsed_us > worst_us) worst_us = elapsed_us;
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
  // Nominal latency is tens of microseconds; the loop's safety-net poll
  // timeout is 1000 ms, so anything far below that proves the wakeup works.
  // Keep the bound loose to stay robust on loaded CI machines.
  EXPECT_LT(worst_us, 500 * 1000) << "Post() wake latency too high";
}

TEST_F(CurlIoLoopTest, PostFromIoThreadIsEnqueuedNotRunInline) {
  CurlIoLoop& loop = CurlIoLoop::Get();
  std::vector<std::string> events;
  Semaphore done(0);
  loop.Post([&]() {
    events.push_back("outer-begin");
    loop.Post([&]() {
      events.push_back("inner");
      done.Post();
    });
    events.push_back("outer-end");
  });
  ASSERT_TRUE(done.TimedWait(kWaitTimeoutMs));
  EXPECT_THAT(events, ElementsAre("outer-begin", "outer-end", "inner"));
}

TEST_F(CurlIoLoopTest, AddRunsTransferAndDeliversOnDone) {
  CurlIoLoop& loop = CurlIoLoop::Get();
  TestTransfer transfer(server_->BaseUrl() + "/");
  CURLMcode add_result = CURLM_LAST;
  loop.Post([&]() {
    add_result = loop.Add(transfer);
    EXPECT_EQ(1u, loop.transfer_count());
  });
  ASSERT_TRUE(transfer.WaitForDone());
  EXPECT_EQ(CURLM_OK, add_result);
  EXPECT_EQ(CURLE_OK, transfer.result());
  EXPECT_EQ(200, transfer.http_status());
  EXPECT_TRUE(transfer.on_done_thread_was_io_thread());
  loop.Post([&]() { EXPECT_EQ(0u, loop.transfer_count()); });
  Drain(loop);
  EXPECT_EQ("GET", server_->last_request().method);
}

TEST_F(CurlIoLoopTest, AddSameHandleTwiceFails) {
  CurlIoLoop& loop = CurlIoLoop::Get();
  TestTransfer transfer(server_->BaseUrl() + "/slow?ms=300");
  CURLMcode second = CURLM_OK;
  loop.Post([&]() {
    ASSERT_EQ(CURLM_OK, loop.Add(transfer));
    second = loop.Add(transfer);
  });
  ASSERT_TRUE(transfer.WaitForDone());
  EXPECT_EQ(CURLM_ADDED_ALREADY, second);
  EXPECT_EQ(CURLE_OK, transfer.result());
}

TEST_F(CurlIoLoopTest, RemoveStopsTransferWithoutOnDone) {
  CurlIoLoop& loop = CurlIoLoop::Get();
  TestTransfer transfer(server_->BaseUrl() + "/slow?ms=5000");
  loop.Post([&]() { ASSERT_EQ(CURLM_OK, loop.Add(transfer)); });
  Drain(loop);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  bool removed = false;
  bool removed_again = true;
  loop.Post([&]() {
    removed = loop.Remove(transfer);
    removed_again = loop.Remove(transfer);
    EXPECT_EQ(0u, loop.transfer_count());
  });
  Drain(loop);
  EXPECT_TRUE(removed);
  EXPECT_FALSE(removed_again);
  EXPECT_FALSE(transfer.done_called());
  // Give the loop a chance to misbehave.
  EXPECT_FALSE(transfer.WaitForDone(200));
}

TEST_F(CurlIoLoopTest, ManyConcurrentTransfersComplete) {
  CurlIoLoop& loop = CurlIoLoop::Get();
  const int kCount = 16;
  std::vector<std::unique_ptr<TestTransfer>> transfers;
  for (int i = 0; i < kCount; ++i) {
    transfers.emplace_back(
        new TestTransfer(server_->BaseUrl() + "/slow?ms=200"));
  }
  auto start = std::chrono::steady_clock::now();
  loop.Post([&]() {
    for (auto& t : transfers) ASSERT_EQ(CURLM_OK, loop.Add(*t));
  });
  for (auto& t : transfers) {
    ASSERT_TRUE(t->WaitForDone());
    EXPECT_EQ(CURLE_OK, t->result());
    EXPECT_EQ(200, t->http_status());
  }
  auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - start)
                        .count();
  // Serialized they would take >= 16 * 200 ms.
  EXPECT_LT(elapsed_ms, 16 * 200);
}

TEST_F(CurlIoLoopTest, PostIfRunningHandlesStoppedAndRunningStates) {
  EXPECT_FALSE(CurlIoLoop::IsCurrentThreadIoThread());
  std::atomic<bool> ran_on_io_thread{false};
  Semaphore ran(0);
  EXPECT_TRUE(CurlIoLoop::PostIfRunning([&]() {
    ran_on_io_thread = CurlIoLoop::IsCurrentThreadIoThread();
    ran.Post();
  }));
  ASSERT_TRUE(ran.TimedWait(kWaitTimeoutMs));
  EXPECT_TRUE(ran_on_io_thread.load());

  // Stop the loop completely; PostIfRunning() must return false without
  // asserting or invoking the callback.
  CurlIoLoop::Release();
  EXPECT_FALSE(CurlIoLoop::IsRunning());
  std::atomic<bool> ran_when_stopped{false};
  EXPECT_FALSE(CurlIoLoop::PostIfRunning([&]() { ran_when_stopped = true; }));
  EXPECT_FALSE(ran_when_stopped.load());

  // Re-acquire so TearDown() balances.
  CurlIoLoop::Acquire();
}

TEST_F(CurlIoLoopTest, ShutdownAbortsInFlightTransfer) {
  CurlIoLoop& loop = CurlIoLoop::Get();
  TestTransfer transfer(server_->BaseUrl() + "/slow?ms=10000");
  loop.Post([&]() { ASSERT_EQ(CURLM_OK, loop.Add(transfer)); });
  Drain(loop);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  EXPECT_FALSE(transfer.done_called());

  // Drop the fixture's (only) reference: the loop must abort the transfer on
  // the I/O thread *before* Release() returns.
  auto start = std::chrono::steady_clock::now();
  CurlIoLoop::Release();
  auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - start)
                        .count();
  EXPECT_FALSE(CurlIoLoop::IsRunning());
  EXPECT_TRUE(transfer.done_called());
  EXPECT_EQ(CURLE_ABORTED_BY_CALLBACK, transfer.result());
  EXPECT_LT(elapsed_ms, 5000);  // Did not wait for the slow server.

  // Re-acquire so TearDown's Release() balances.
  CurlIoLoop::Acquire();
}

TEST_F(CurlIoLoopTest, PostedWorkDuringShutdownStillRuns) {
  // Functions posted by OnDone() during shutdown must still be executed.
  CurlIoLoop& loop = CurlIoLoop::Get();
  class PostingTransfer : public TestTransfer {
   public:
    explicit PostingTransfer(const std::string& url) : TestTransfer(url) {}
    void OnDone(CURLcode result) override {
      CurlIoLoop::Get().Post([this]() { follow_up_ran = true; });
      TestTransfer::OnDone(result);
    }
    std::atomic<bool> follow_up_ran{false};
  };
  PostingTransfer transfer(server_->BaseUrl() + "/slow?ms=10000");
  loop.Post([&]() { ASSERT_EQ(CURLM_OK, loop.Add(transfer)); });
  Drain(loop);
  CurlIoLoop::Release();
  EXPECT_TRUE(transfer.done_called());
  EXPECT_TRUE(transfer.follow_up_ran.load());
  CurlIoLoop::Acquire();
}

}  // namespace
}  // namespace curl
}  // namespace rest
}  // namespace firebase
