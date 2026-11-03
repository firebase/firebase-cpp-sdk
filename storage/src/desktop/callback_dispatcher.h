// Copyright 2026 Google LLC
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

#ifndef FIREBASE_STORAGE_SRC_DESKTOP_CALLBACK_DISPATCHER_H_
#define FIREBASE_STORAGE_SRC_DESKTOP_CALLBACK_DISPATCHER_H_

#include <functional>
#include <memory>
#include <thread>

namespace firebase::storage::internal {

// Runs user-facing callbacks (storage Listener notifications) on a dedicated
// worker thread, in the order they were posted.
//
// Transfers are driven by the shared curl I/O thread; running user code there
// would let a Listener that deletes its Storage, App or operation block that
// thread on itself (e.g. ~TransportCurl() waiting for a transfer only the I/O
// thread can finish). Callbacks posted here run on a thread that owns no
// transport state, so they may destroy any Storage object.
//
// The worker thread is started lazily by the first Post(). Its state is
// shared with the thread, so Shutdown() and the destructor may run on the
// worker thread itself (from inside a callback): the thread is then detached
// and exits as soon as that callback returns.
class CallbackDispatcher {
 public:
  CallbackDispatcher();
  ~CallbackDispatcher();

  CallbackDispatcher(const CallbackDispatcher&) = delete;
  CallbackDispatcher& operator=(const CallbackDispatcher&) = delete;
  CallbackDispatcher(CallbackDispatcher&&) = delete;
  CallbackDispatcher& operator=(CallbackDispatcher&&) = delete;

  // Queues `task`. Returns false, dropping `task`, after Shutdown().
  bool Post(std::function<void()> task);

  // Stops accepting tasks and drops queued ones. Unless called on the worker
  // thread, waits for a running task to return. Idempotent.
  void Shutdown();

  // Whether the calling thread is this dispatcher's worker thread.
  [[nodiscard]] bool IsWorkerThread() const;

 private:
  struct State;

  static void Run(std::shared_ptr<State> state);

  std::shared_ptr<State> state_;
};

}  // namespace firebase::storage::internal

#endif  // FIREBASE_STORAGE_SRC_DESKTOP_CALLBACK_DISPATCHER_H_
