// Copyright 2026 Google LLC

#include "storage/src/desktop/callback_dispatcher.h"

#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>

namespace firebase::storage::internal {

struct CallbackDispatcher::State {
  std::mutex mutex;
  std::condition_variable cv;
  std::deque<std::function<void()>> queue;
  std::thread thread;
  std::thread::id worker_id;
  bool stopped = false;
};

CallbackDispatcher::CallbackDispatcher() : state_(std::make_shared<State>()) {}
CallbackDispatcher::~CallbackDispatcher() { Shutdown(); }

bool CallbackDispatcher::Post(std::function<void()> task) {
  if (!task) return false;
  {
    std::scoped_lock lock(state_->mutex);
    if (state_->stopped) return false;
    if (!state_->thread.joinable()) {
      state_->thread = std::thread(&CallbackDispatcher::Run, state_);
      state_->worker_id = state_->thread.get_id();
    }
    state_->queue.push_back(std::move(task));
  }
  state_->cv.notify_one();
  return true;
}

void CallbackDispatcher::Shutdown() {
  std::deque<std::function<void()>> dropped;
  std::thread thread;
  {
    std::scoped_lock lock(state_->mutex);
    state_->stopped = true;
    dropped.swap(state_->queue);
    thread = std::move(state_->thread);
  }
  state_->cv.notify_all();
  dropped.clear();
  if (thread.joinable()) {
    if (thread.get_id() == std::this_thread::get_id()) {
      thread.detach();
    } else {
      thread.join();
    }
  }
}

bool CallbackDispatcher::IsWorkerThread() const {
  std::scoped_lock lock(state_->mutex);
  return state_->worker_id == std::this_thread::get_id();
}

void CallbackDispatcher::Run(std::shared_ptr<State> state) {
  while (true) {
    std::function<void()> task;
    {
      std::unique_lock<std::mutex> lock(state->mutex);
      state->cv.wait(lock, [&state]() { return state->stopped || !state->queue.empty(); });
      if (state->stopped) return;
      task = std::move(state->queue.front());
      state->queue.pop_front();
    }
    task();
  }
}

}  // namespace firebase::storage::internal
