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

#include "app/rest/curl/curl_global.h"
#include "app/rest/curl/curl_handles.h"
#include "app/rest/curl/transfer.h"
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
    SetOpt(easy_.get(), CURLOPT_URL, url.c_str());
    SetOpt(easy_.get(), CURLOPT_WRITEFUNCTION, &DiscardBody);
    SetOpt(easy_.get(), CURLOPT_WRITEDATA, static_cast<void*>(this));
  }

  CURL* easy() override { return easy_.get(); }

  void OnDone(CURLcode result) override {
    result_ = result;
    long status = 0;  // NOLINT
    curl_easy_getinfo(easy_.get(), CURLINFO_RESPONSE_CODE, &status);
    http_status_ = static_cast<int>(status);
    on_done_thread_was_io_thread_ = CurlIoLoop::Get().IsOnIoThread();
    done_called_ = true;
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
  EXPECT_EQ(0, CurlGlobal::ref_count());
  // ...and re-acquiring starts a fresh one (balanced by TearDown).
  CurlIoLoop::Acquire();
  EXPECT_TRUE(CurlIoLoop::IsRunning());
  EXPECT_EQ(1, CurlGlobal::ref_count());
  Drain(CurlIoLoop::Get());
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
    transfers.push_back(
        std::make_unique<TestTransfer>(server_->BaseUrl() + "/slow?ms=200"));
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

TEST_F(CurlIoLoopTest, ExtraFdReadinessCallback) {
  CurlIoLoop& loop = CurlIoLoop::Get();
  test::TcpSocketPair pair;
  ASSERT_TRUE(pair.valid());

  std::atomic<int> ready_count{0};
  std::atomic<short> last_revents{0};
  Semaphore ready(0);
  loop.Post([&]() {
    loop.AddExtraFd(static_cast<curl_socket_t>(pair.b()), CURL_WAIT_POLLIN,
                    [&](short revents) {
                      last_revents = revents;
                      ready_count++;
                      // Consume the byte so the fd goes quiet again.
                      char byte;
                      recv(pair.b(), &byte, 1, 0);
                      ready.Post();
                    });
  });
  Drain(loop);
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  EXPECT_EQ(0, ready_count.load());

  // Make b() readable by writing from a().
  ASSERT_TRUE(test::SendAll(pair.a(), "x", 1));
  ASSERT_TRUE(ready.TimedWait(kWaitTimeoutMs));
  EXPECT_EQ(1, ready_count.load());
  EXPECT_NE(0, last_revents.load() & CURL_WAIT_POLLIN);

  // Switching to POLLOUT fires immediately (a connected socket is writable)
  // and the callback can remove its own registration.
  Semaphore writable(0);
  loop.Post([&]() {
    loop.AddExtraFd(static_cast<curl_socket_t>(pair.b()), CURL_WAIT_POLLIN,
                    [&](short revents) {
                      last_revents = revents;
                      loop.RemoveExtraFd(static_cast<curl_socket_t>(pair.b()));
                      writable.Post();
                    });
    loop.SetExtraFdEvents(static_cast<curl_socket_t>(pair.b()),
                          CURL_WAIT_POLLOUT);
  });
  ASSERT_TRUE(writable.TimedWait(kWaitTimeoutMs));
  EXPECT_NE(0, last_revents.load() & CURL_WAIT_POLLOUT);

  // Removed: further writes must not trigger anything.
  ready_count = 0;
  ASSERT_TRUE(test::SendAll(pair.a(), "y", 1));
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  EXPECT_FALSE(writable.TimedWait(50));
  EXPECT_EQ(0, ready_count.load());
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

TEST_F(CurlIoLoopTest, PostIfRunningReturnsFalseWhenStoppedAndTrueWhenRunning) {
  std::atomic<bool> ran_while_running{false};
  Semaphore done(0);
  EXPECT_TRUE(CurlIoLoop::PostIfRunning([&]() {
    ran_while_running = true;
    done.Post();
  }));
  ASSERT_TRUE(done.TimedWait(kWaitTimeoutMs));
  EXPECT_TRUE(ran_while_running.load());

  // Drop the fixture's reference so the loop is stopped.
  CurlIoLoop::Release();
  EXPECT_FALSE(CurlIoLoop::IsRunning());
  std::atomic<bool> ran_after_release{false};
  EXPECT_FALSE(CurlIoLoop::PostIfRunning([&]() { ran_after_release = true; }));
  EXPECT_FALSE(ran_after_release.load());

  // Re-acquire so TearDown's Release() balances.
  CurlIoLoop::Acquire();
}

TEST_F(CurlIoLoopTest, ExtraFdGenerationPreventsStaleCallbackWhenReplacedInBatch) {
  CurlIoLoop& loop = CurlIoLoop::Get();
  test::TcpSocketPair pair1;
  test::TcpSocketPair pair2;
  ASSERT_TRUE(pair1.valid());
  ASSERT_TRUE(pair2.valid());

  std::atomic<int> first_cb_count{0};
  std::atomic<int> replaced_cb_count{0};
  std::atomic<int> original_second_cb_count{0};
  Semaphore batch_done(0);

  const auto fd1 = static_cast<curl_socket_t>(pair1.b());
  const auto fd2 = static_cast<curl_socket_t>(pair2.b());

  // Make both sockets readable BEFORE registering them on the loop so
  // curl_multi_poll deterministically returns both fd1 and fd2 ready in the
  // same poll batch.
  ASSERT_TRUE(test::SendAll(pair1.a(), "a", 1));
  ASSERT_TRUE(test::SendAll(pair2.a(), "b", 1));

  loop.Post([&]() {
    // Register fd1 and fd2 in the same posted task so both are polled together.
    // When fd1's callback runs first in the batch, it drains pair2.b() and
    // replaces fd2's registration via AddExtraFd(fd2, ...), bumping fd2's
    // generation counter while keeping fd2 in extra_fds_. The stale revents
    // for fd2 in the current batch must not fire the replacement callback.
    loop.AddExtraFd(fd1, CURL_WAIT_POLLIN, [&](short) {
      first_cb_count++;
      char byte;
      recv(pair1.b(), &byte, 1, 0);
      recv(pair2.b(), &byte, 1, 0);
      loop.RemoveExtraFd(fd1);
      loop.AddExtraFd(fd2, CURL_WAIT_POLLIN, [&](short) {
        replaced_cb_count++;
      });
      batch_done.Post();
    });
    loop.AddExtraFd(fd2, CURL_WAIT_POLLIN, [&](short) {
      original_second_cb_count++;
    });
  });

  ASSERT_TRUE(batch_done.TimedWait(kWaitTimeoutMs));
  Drain(loop);
  loop.Post([&]() { loop.RemoveExtraFd(fd2); });
  Drain(loop);

  EXPECT_EQ(1, first_cb_count.load());
  EXPECT_EQ(0, original_second_cb_count.load());
  EXPECT_EQ(0, replaced_cb_count.load());
}

}  // namespace
}  // namespace curl
}  // namespace rest
}  // namespace firebase
