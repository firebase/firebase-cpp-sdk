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
#include <utility>

#include "app/src/assert.h"
#include "app/src/log.h"

namespace firebase {
namespace rest {
namespace curl {

namespace {

// Upper bound on how long curl_multi_poll() sleeps when nothing happens. This
// is purely a safety net: libcurl shortens the wait to its own next timer
// (connect timeouts, CURLOPT_TIMEOUT_MS, retries, ...) and Post() wakes the
// thread immediately via curl_multi_wakeup(), so in practice the loop only
// ever wakes when there is something to do.
constexpr int kPollTimeoutMs = 1000;

// Singleton state. The mutex is intentionally leaked so it stays valid during
// static destruction, matching the other process-wide mutexes in app/rest.
Mutex* g_loop_mutex = new Mutex();
int g_loop_ref_count = 0;
// Read lock-free by Get() so the I/O thread itself can call Get() while
// Release() is joining it under g_loop_mutex.
std::atomic<CurlIoLoop*> g_loop{nullptr};
// Non-null only on the live CurlIoLoop worker thread inside Run(), allowing
// PostIfRunning() and IsCurrentThreadIoThread() to avoid locking g_loop_mutex
// (which Release() holds while joining the worker thread).
thread_local CurlIoLoop* g_tls_io_loop = nullptr;

}  // namespace

Transfer::~Transfer() {}

// ---------------------------------------------------------------------------
// Singleton management.
// ---------------------------------------------------------------------------

void CurlIoLoop::Acquire() {
  MutexLock lock(*g_loop_mutex);
  if (g_loop_ref_count == 0) {
    FIREBASE_ASSERT(g_loop.load() == nullptr);
    CURLcode code = curl_global_init(CURL_GLOBAL_ALL);
    FIREBASE_ASSERT_MESSAGE(code == CURLE_OK,
                            "curl_global_init failed with code %d",
                            static_cast<int>(code));
    g_loop.store(new CurlIoLoop());
  }
  ++g_loop_ref_count;
}

void CurlIoLoop::Release() {
  MutexLock lock(*g_loop_mutex);
  FIREBASE_ASSERT_MESSAGE(g_loop_ref_count > 0,
                          "CurlIoLoop::Release() without matching Acquire()");
  if (--g_loop_ref_count == 0) {
    CurlIoLoop* loop = g_loop.load();
    FIREBASE_ASSERT_MESSAGE(!loop->IsOnIoThread(),
                            "CurlIoLoop::Release() of the last reference must "
                            "not be called from the I/O thread");
    // Destroy before clearing the pointer so that Get() keeps working for
    // code running on the I/O thread during shutdown (OnDone handlers).
    delete loop;
    g_loop.store(nullptr);
    curl_global_cleanup();
  }
}

CurlIoLoop& CurlIoLoop::Get() {
  CurlIoLoop* loop = g_loop.load();
  FIREBASE_ASSERT_MESSAGE(loop != nullptr,
                          "CurlIoLoop::Get() called without a prior "
                          "CurlIoLoop::Acquire() / InitTransportCurl()");
  return *loop;
}

bool CurlIoLoop::IsRunning() { return g_loop.load() != nullptr; }

bool CurlIoLoop::IsCurrentThreadIoThread() {
  return g_tls_io_loop != nullptr;
}

bool CurlIoLoop::PostIfRunning(std::function<void()> fn) {
  if (g_tls_io_loop != nullptr) {
    return g_tls_io_loop->Post(std::move(fn));
  }
  MutexLock lock(*g_loop_mutex);
  CurlIoLoop* loop = g_loop.load();
  return loop != nullptr && loop->Post(std::move(fn));
}

// ---------------------------------------------------------------------------
// Construction / destruction.
// ---------------------------------------------------------------------------

CurlIoLoop::CurlIoLoop()
    : multi_(nullptr),
      thread_(),
      thread_id_(),
      thread_started_(0),
      mutex_(Mutex::kModeNonRecursive),
      posted_(),
      stopped_(false),
      quit_(false),
      transfers_(),
      extra_fds_(),
      next_extra_fd_generation_(0),
      wait_fds_(),
      wait_fd_generations_() {
  multi_.reset(curl_multi_init());
  FIREBASE_ASSERT_MESSAGE(multi_ != nullptr, "curl_multi_init() failed");
  thread_ = Thread(ThreadMain, this);
  // Wait until the thread has published its id so IsOnIoThread() is reliable
  // for every caller from here on.
  thread_started_.Wait();
}

CurlIoLoop::~CurlIoLoop() {
  Post([this]() { quit_ = true; });
  thread_.Join();
  // Synchronize with any Post() that observed stopped_ == false (it holds
  // mutex_ while it wakes us) before multi_ is destroyed below.
  MutexLock lock(mutex_);
  FIREBASE_ASSERT(stopped_);
  FIREBASE_ASSERT(transfers_.empty());
}

// ---------------------------------------------------------------------------
// Cross-thread entry points.
// ---------------------------------------------------------------------------

bool CurlIoLoop::Post(std::function<void()> fn) {
  FIREBASE_ASSERT(fn);
  MutexLock lock(mutex_);
  if (stopped_) {
    LogError("CurlIoLoop::Post() called after the I/O loop stopped; dropping");
    return false;
  }
  posted_.push_back(std::move(fn));
  // Called under mutex_ so the loop cannot stop (and multi_ cannot be
  // destroyed) between the push and the wakeup. curl_multi_wakeup() is
  // thread-safe and merely writes to an internal socket pair.
  CURLMcode code = curl_multi_wakeup(multi_.get());
  if (code != CURLM_OK) {
    LogError("curl_multi_wakeup() failed: %s", curl_multi_strerror(code));
  }
  return true;
}

bool CurlIoLoop::IsOnIoThread() const {
  return Thread::IsCurrentThread(thread_id_);
}

void CurlIoLoop::AssertOnIoThread(const char* what) const {
  FIREBASE_ASSERT_MESSAGE(IsOnIoThread(),
                          "CurlIoLoop::%s() must be called on the I/O thread "
                          "(use CurlIoLoop::Post())",
                          what);
}

// ---------------------------------------------------------------------------
// I/O-thread-only API.
// ---------------------------------------------------------------------------

CURLMcode CurlIoLoop::Add(Transfer& transfer) {
  AssertOnIoThread("Add");
  CURL* easy = transfer.easy();
  FIREBASE_ASSERT_MESSAGE(easy != nullptr,
                          "Transfer::easy() returned nullptr in Add()");
  if (transfers_.find(easy) != transfers_.end()) {
    LogError("CurlIoLoop::Add(): easy handle %p is already registered", easy);
    return CURLM_ADDED_ALREADY;
  }
  CURLMcode code = curl_multi_add_handle(multi_.get(), easy);
  if (code != CURLM_OK) {
    LogError("curl_multi_add_handle() failed: %s", curl_multi_strerror(code));
    return code;
  }
  transfers_.emplace(easy, &transfer);
  return CURLM_OK;
}

bool CurlIoLoop::Remove(Transfer& transfer) {
  AssertOnIoThread("Remove");
  CURL* easy = transfer.easy();
  auto it = transfers_.find(easy);
  if (it == transfers_.end() || it->second != &transfer) return false;
  transfers_.erase(it);
  CURLMcode code = curl_multi_remove_handle(multi_.get(), easy);
  if (code != CURLM_OK) {
    LogError("curl_multi_remove_handle() failed: %s",
             curl_multi_strerror(code));
  }
  return true;
}

void CurlIoLoop::AddExtraFd(curl_socket_t fd, short events,
                            std::function<void(short)> on_ready) {
  AssertOnIoThread("AddExtraFd");
  FIREBASE_ASSERT(on_ready);
  const uint64_t generation = ++next_extra_fd_generation_;
  for (ExtraFd& extra : extra_fds_) {
    if (extra.fd == fd) {
      extra.events = events;
      extra.generation = generation;
      extra.on_ready = std::move(on_ready);
      return;
    }
  }
  extra_fds_.push_back(ExtraFd{fd, events, generation, std::move(on_ready)});
}

void CurlIoLoop::RemoveExtraFd(curl_socket_t fd) {
  AssertOnIoThread("RemoveExtraFd");
  for (auto it = extra_fds_.begin(); it != extra_fds_.end(); ++it) {
    if (it->fd == fd) {
      extra_fds_.erase(it);
      return;
    }
  }
}

void CurlIoLoop::SetExtraFdEvents(curl_socket_t fd, short events) {
  AssertOnIoThread("SetExtraFdEvents");
  for (ExtraFd& extra : extra_fds_) {
    if (extra.fd == fd) {
      extra.events = events;
      return;
    }
  }
}

CURLM* CurlIoLoop::multi() {
  AssertOnIoThread("multi");
  return multi_.get();
}

size_t CurlIoLoop::transfer_count() const {
  AssertOnIoThread("transfer_count");
  return transfers_.size();
}

// ---------------------------------------------------------------------------
// The loop.
// ---------------------------------------------------------------------------

void CurlIoLoop::ThreadMain(void* loop) {
  static_cast<CurlIoLoop*>(loop)->Run();
}

size_t CurlIoLoop::RunPosted() {
  size_t count = 0;
  for (;;) {
    std::deque<std::function<void()>> batch;
    {
      MutexLock lock(mutex_);
      if (posted_.empty()) return count;
      batch.swap(posted_);
    }
    for (std::function<void()>& fn : batch) {
      fn();
      ++count;
    }
    // Functions posted by the batch we just ran are picked up by the next
    // pass so that "post then wait for I/O" sequences never miss a wakeup.
  }
}

void CurlIoLoop::DispatchDone() {
  int messages_left = 0;
  CURLMsg* message = nullptr;
  while ((message = curl_multi_info_read(multi_.get(), &messages_left)) !=
         nullptr) {
    if (message->msg != CURLMSG_DONE) continue;
    // Copy out everything we need: the message is invalidated by
    // curl_multi_remove_handle().
    CURL* easy = message->easy_handle;
    CURLcode result = message->data.result;
    curl_multi_remove_handle(multi_.get(), easy);
    auto it = transfers_.find(easy);
    if (it == transfers_.end()) {
      LogWarning("CurlIoLoop: CURLMSG_DONE for unknown easy handle %p", easy);
      continue;
    }
    Transfer* transfer = it->second;
    transfers_.erase(it);
    transfer->OnDone(result);
  }
}

void CurlIoLoop::AbortAllTransfers() {
  while (!transfers_.empty()) {
    auto it = transfers_.begin();
    CURL* easy = it->first;
    Transfer* transfer = it->second;
    transfers_.erase(it);
    curl_multi_remove_handle(multi_.get(), easy);
    transfer->OnDone(CURLE_ABORTED_BY_CALLBACK);
  }
}

void CurlIoLoop::Run() {
  g_tls_io_loop = this;
  struct ScopedTlsReset {
    ~ScopedTlsReset() { g_tls_io_loop = nullptr; }
  } reset_tls;

  thread_id_ = Thread::CurrentId();
  thread_started_.Post();

  while (!quit_) {
    RunPosted();
    if (quit_) break;

    int running_handles = 0;
    CURLMcode code = curl_multi_perform(multi_.get(), &running_handles);
    if (code != CURLM_OK) {
      LogError("curl_multi_perform() failed: %s", curl_multi_strerror(code));
    }
    DispatchDone();

    // Sleep until a libcurl socket or timer fires, an extra fd is ready, or
    // Post() wakes us. libcurl clamps the timeout to its own next timer.
    wait_fds_.clear();
    wait_fd_generations_.clear();
    for (const ExtraFd& extra : extra_fds_) {
      curl_waitfd wait_fd;
      wait_fd.fd = extra.fd;
      wait_fd.events = extra.events;
      wait_fd.revents = 0;
      wait_fds_.push_back(wait_fd);
      wait_fd_generations_.push_back(extra.generation);
    }
    int num_fds = 0;
    code = curl_multi_poll(
        multi_.get(), wait_fds_.empty() ? nullptr : wait_fds_.data(),
        static_cast<unsigned int>(wait_fds_.size()), kPollTimeoutMs, &num_fds);
    if (code != CURLM_OK) {
      LogError("curl_multi_poll() failed: %s", curl_multi_strerror(code));
    }

    // Dispatch extra-fd readiness. Match both fd and generation on each
    // iteration (and copy the callback) so that removing or replacing an fd—or
    // closing an fd whose integer value is recycled by a newly opened socket in
    // the same wakeup batch—never delivers stale revents to the new callback.
    for (size_t i = 0; i < wait_fds_.size(); ++i) {
      const curl_waitfd& wait_fd = wait_fds_[i];
      if (wait_fd.revents == 0) continue;
      const uint64_t expected_gen = wait_fd_generations_[i];
      std::function<void(short)> on_ready;
      for (const ExtraFd& extra : extra_fds_) {
        if (extra.fd == wait_fd.fd && extra.generation == expected_gen) {
          on_ready = extra.on_ready;
          break;
        }
      }
      if (on_ready) on_ready(wait_fd.revents);
    }
  }

  // Shutdown: run anything posted after the quit request, then abort every
  // transfer still in flight so owners waiting on them are released, then run
  // anything those OnDone() handlers posted. Further Post() calls are rejected
  // once stopped_ is set.
  RunPosted();
  AbortAllTransfers();
  std::deque<std::function<void()>> remaining;
  {
    MutexLock lock(mutex_);
    stopped_ = true;
    remaining.swap(posted_);
  }
  for (std::function<void()>& fn : remaining) fn();
  // A transfer added by one of the functions above can never complete.
  if (!transfers_.empty()) {
    LogError(
        "CurlIoLoop stopped with %d transfer(s) added during shutdown; "
        "aborting them",
        static_cast<int>(transfers_.size()));
    AbortAllTransfers();
  }
}

}  // namespace curl
}  // namespace rest
}  // namespace firebase
