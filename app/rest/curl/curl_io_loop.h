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

#ifndef FIREBASE_APP_REST_CURL_CURL_IO_LOOP_H_
#define FIREBASE_APP_REST_CURL_CURL_IO_LOOP_H_

#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <unordered_map>

#include "app/rest/curl/curl_handles.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/semaphore.h"
#include "app/src/thread.h"
#include "curl/curl.h"

namespace firebase {
namespace rest {
namespace curl {

// One libcurl easy handle driven by the shared CurlIoLoop.
//
// A Transfer is the unit of work the I/O loop understands: it owns one
// configured CURL easy handle and is notified on the I/O thread when libcurl
// finishes driving it.
//
// Lifetime and threading contract:
//  * All methods are invoked on the I/O thread only.
//  * The Transfer must stay alive from CurlIoLoop::Add() until either
//    CurlIoLoop::Remove() returns or OnDone() has been called, whichever
//    happens first. After OnDone() the loop holds no reference to it, so
//    OnDone() may `delete this`.
//  * easy() must return the same handle for the whole time the transfer is
//    registered with the loop.
class Transfer {
 public:
  virtual ~Transfer();

  // The easy handle this transfer drives. Must already be configured when the
  // transfer is passed to CurlIoLoop::Add().
  virtual CURL* easy() = 0;

  // Called exactly once on the I/O thread when libcurl reports the transfer
  // finished (CURLMSG_DONE) or when the loop is shutting down with the
  // transfer still in flight (result == CURLE_ABORTED_BY_CALLBACK). The loop
  // has forgotten the Transfer and removed the easy handle from the multi
  // handle by the time this runs, so it is safe to destroy both from here.
  //
  // IMPORTANT: this runs on the single process-wide I/O thread that services
  // every REST transfer in the process. Implementations MUST keep it short:
  // record the result, hand off to the owner's own executor / scheduler /
  // semaphore, and return. Never block, never perform network I/O, and never
  // run product or application code here.
  virtual void OnDone(CURLcode result) = 0;
};

// The single process-wide libcurl I/O thread for REST transfers.
//
// CurlIoLoop owns one CURLM multi handle and one thread that drives it with
// the idiomatic libcurl event loop:
//
//   while (!quit) {
//     run functions queued with Post();
//     curl_multi_perform();
//     for each CURLMSG_DONE: curl_multi_remove_handle(); Transfer::OnDone();
//     curl_multi_poll(multi, nullptr, 0, timeout, &numfds);   // sleeps
//   }
//
// curl_multi_poll() blocks until a libcurl socket is ready, a libcurl timer
// expires, or another thread calls Post() (which uses curl_multi_wakeup()).
// There is no timer-based polling: an idle loop costs zero CPU and reacts to
// new work in microseconds. The one shared multi handle also means connection
// reuse, HTTP/2 multiplexing, DNS cache and TLS session cache are shared by
// every REST product in the process.
//
// ---------------------------------------------------------------------------
// Thread-safety contract
// ---------------------------------------------------------------------------
//  * Every CURL easy handle driven by the loop is created, configured, driven
//    and destroyed on the I/O thread.
//  * Every other thread interacts with the loop exclusively through Post(),
//    which is thread-safe and runs the function on the I/O thread.
//  * Add(), Remove() and multi() assert that they are called on the I/O
//    thread, i.e. from inside a posted function or Transfer::OnDone().
//  * Functions run by the loop (posted functions, Transfer::OnDone()) must be
//    short and must never block: they share the one thread with every transfer
//    in the process. Hand real work to your own executor.
//
// ---------------------------------------------------------------------------
// Lifetime
// ---------------------------------------------------------------------------
// The loop is a reference-counted singleton. Acquire() initializes libcurl's
// global state and starts the thread on the first reference; Release() stops
// the thread and releases libcurl's global state when the last reference goes
// away. Products call these through rest::InitTransportCurl() /
// CleanupTransportCurl(). Shutdown posts a quit request and joins the thread;
// any Transfer still registered at that point is removed from the multi handle
// and receives OnDone(CURLE_ABORTED_BY_CALLBACK) on the I/O thread before
// Release() returns, so owners blocked waiting for completion are always
// released.
//
// Release() must not be called from the I/O thread itself (it would join
// itself).
class CurlIoLoop {
 public:
  // Reference counting. Thread-safe. Acquire() returns once the I/O thread is
  // running; Release() returns once it has stopped (on the last reference).
  static void Acquire();
  static void Release();
  // The live loop. Only valid between Acquire() and the matching Release();
  // asserts otherwise. Lock-free, so it may be called from the I/O thread.
  static CurlIoLoop& Get();
  // True if a loop is currently alive (for diagnostics/tests).
  static bool IsRunning();
  // True when the calling thread is a live CurlIoLoop worker thread. Safe to
  // call from any thread regardless of whether a loop is running.
  static bool IsCurrentThreadIoThread();
  // Atomically posts `fn` to the active loop if one is running, serialized
  // against Acquire()/Release() when called off the I/O thread. Returns false
  // without asserting if no loop is running or the loop has already stopped.
  static bool PostIfRunning(std::function<void()> fn);

  // Queues fn to run on the I/O thread as soon as possible and wakes the
  // thread. Thread-safe. Functions run in FIFO order relative to each other.
  //
  // When called *from* the I/O thread the function is still enqueued (never
  // run inline): it will run on the next pass of the loop, after the current
  // callback returns. This keeps FIFO ordering simple and makes it safe to
  // Post() from inside OnDone() and other posted functions without
  // re-entrancy concerns.
  //
  // Posting after the loop has stopped returns false and drops the function
  // (with an error log); this can only happen if a caller outlives its own
  // Release(), which is a bug in the caller. Returns true if fn was queued.
  bool Post(std::function<void()> fn);

  // True when called on the I/O thread.
  bool IsOnIoThread() const;

  // Registers a transfer: curl_multi_add_handle(transfer.easy()). I/O thread
  // only. On success the loop remembers the transfer and will call its
  // OnDone() exactly once. On failure (returns != CURLM_OK) the transfer is
  // not registered and OnDone() will never be called for it.
  CURLMcode Add(Transfer& transfer);

  // Unregisters a transfer before it finished: curl_multi_remove_handle() and
  // forget it. I/O thread only. OnDone() will NOT be called for it. Safe to
  // call for a transfer that is not (or no longer) registered; returns false
  // in that case.
  bool Remove(Transfer& transfer);

  // The multi handle, for consumers that need curl_multi_setopt() or
  // CURLMOPT_* tuning. I/O thread only.
  CURLM* multi();

  // Number of registered transfers (for tests). I/O thread only.
  size_t transfer_count() const;

  CurlIoLoop(const CurlIoLoop&) = delete;
  CurlIoLoop& operator=(const CurlIoLoop&) = delete;

 private:
  CurlIoLoop();
  ~CurlIoLoop();

  static void ThreadMain(void* loop);
  void Run();
  // Runs every queued function. Returns the number run.
  size_t RunPosted();
  // Handles every pending CURLMSG_DONE.
  void DispatchDone();
  // Removes and aborts every registered transfer (shutdown path).
  void AbortAllTransfers();
  void AssertOnIoThread(const char* what) const;

  UniqueMulti multi_;
  Thread thread_;
  Thread::Id thread_id_;
  Semaphore thread_started_;

  // Guards posted_ and stopped_.
  Mutex mutex_;
  std::deque<std::function<void()>> posted_;
  bool stopped_;

  // I/O thread only from here on.
  bool quit_;
  std::unordered_map<CURL*, Transfer*> transfers_;
};

}  // namespace curl
}  // namespace rest
}  // namespace firebase

#endif  // FIREBASE_APP_REST_CURL_CURL_IO_LOOP_H_
