// Copyright 2018 Google LLC
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

#ifndef FIREBASE_STORAGE_SRC_STUB_LISTENER_STUB_H_
#define FIREBASE_STORAGE_SRC_STUB_LISTENER_STUB_H_

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

#include "firebase/storage/listener.h"

namespace firebase::storage::internal {

// Android implementation of ListenerInternal: it only tracks "detach"
// callbacks that unbind in-flight operations from the Listener. Detach() /
// ~ListenerInternal() run those internal callbacks; they never invoke the
// user's Listener::OnProgress()/OnPaused().
class ListenerInternal {
 public:
  explicit ListenerInternal(Listener* listener)
      : state_(std::make_shared<State>()) {
    (void)listener;
  }

  ListenerInternal(const ListenerInternal&) = delete;
  ListenerInternal& operator=(const ListenerInternal&) = delete;
  ListenerInternal(ListenerInternal&&) = delete;
  ListenerInternal& operator=(ListenerInternal&&) = delete;

  ~ListenerInternal() { DetachAll(); }

  // Registers a callback under an auto-generated numeric token (never 0).
  uint64_t RegisterDetachCallback(std::function<void()> callback) {
    if (!callback) return 0;
    std::lock_guard<std::mutex> lock(state_->mutex);
    const uint64_t token = state_->next_token++;
    state_->callbacks.emplace_back(Key{KeyKind::kNumeric, token},
                                   std::move(callback));
    return token;
  }

  // Registers (or replaces) a callback keyed by an object address. Pointer
  // keys live in a separate key space from numeric tokens, so they can never
  // collide.
  void RegisterDetachCallback(void* token, std::function<void()> callback) {
    if (token == nullptr || !callback) return;
    const Key key = PointerKey(token);
    std::lock_guard<std::mutex> lock(state_->mutex);
    for (auto& entry : state_->callbacks) {
      if (entry.first == key) {
        entry.second = std::move(callback);
        return;
      }
    }
    state_->callbacks.emplace_back(key, std::move(callback));
  }

  void UnregisterDetachCallback(uint64_t token) {
    if (token == 0) return;
    Unregister(state_, Key{KeyKind::kNumeric, token});
  }

  void UnregisterDetachCallback(void* token) {
    if (token == nullptr) return;
    Unregister(state_, PointerKey(token));
  }

  // Runs and removes every registered callback, including callbacks
  // registered while DetachAll() is running. Callbacks run without the lock
  // held. Returns once the queue is empty and no callback is executing on
  // another thread. Safe if a callback destroys this ListenerInternal: all
  // bookkeeping lives in the shared State, kept alive by local copies.
  void DetachAll() {
    std::shared_ptr<State> state = state_;
    const std::thread::id self_id = std::this_thread::get_id();
    std::unique_lock<std::mutex> lock(state->mutex);
    while (true) {
      while (!state->callbacks.empty()) {
        auto entry = std::move(state->callbacks.front());
        state->callbacks.pop_front();
        state->active.emplace_back(entry.first, self_id);
        lock.unlock();
        {
          // RAII: remove the active marker and wake waiters on every path.
          ActiveScope scope{state, entry.first, self_id};
          RunCallback(entry.second);
        }
        lock.lock();
      }
      state->cv.wait(lock, [&state, self_id]() {
        if (!state->callbacks.empty()) return true;
        for (const auto& active : state->active) {
          if (active.second != self_id) return false;
        }
        return true;
      });
      // Re-check: a callback may have been registered while we waited.
      if (state->callbacks.empty()) break;
    }
  }

  void Detach() { DetachAll(); }

  static ListenerInternal* GetImpl(Listener* listener) {
    return listener ? listener->impl_ : nullptr;
  }

 private:
  enum class KeyKind { kNumeric, kPointer };
  using Key = std::pair<KeyKind, uint64_t>;

  struct State {
    std::mutex mutex;
    std::condition_variable cv;
    uint64_t next_token = 1;
    std::deque<std::pair<Key, std::function<void()>>> callbacks;
    std::vector<std::pair<Key, std::thread::id>> active;
  };

  static Key PointerKey(void* token) {
    return Key{KeyKind::kPointer,
               static_cast<uint64_t>(reinterpret_cast<uintptr_t>(token))};
  }

  // Runs an internal detach callback. These must not throw; if one does
  // anyway (exceptions enabled), contain it so the remaining callbacks still
  // run and no exception escapes ~ListenerInternal().
  static void RunCallback(const std::function<void()>& callback) {
    if (!callback) return;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
    try {
      callback();
    } catch (...) {
    }
#else
    callback();
#endif
  }

  struct ActiveScope {
    std::shared_ptr<State> state;
    Key key;
    std::thread::id thread_id;
    ~ActiveScope() {
      std::lock_guard<std::mutex> lock(state->mutex);
      auto& active = state->active;
      for (auto it = active.begin(); it != active.end(); ++it) {
        if (it->first == key && it->second == thread_id) {
          active.erase(it);
          break;
        }
      }
      state->cv.notify_all();
    }
  };

  static void Unregister(const std::shared_ptr<State>& state, const Key& key) {
    const std::thread::id self_id = std::this_thread::get_id();
    std::unique_lock<std::mutex> lock(state->mutex);
    for (auto it = state->callbacks.begin(); it != state->callbacks.end();
         ++it) {
      if (it->first == key) {
        state->callbacks.erase(it);
        break;
      }
    }
    // Don't return while this key's callback is still running on another
    // thread (the caller may be about to destroy what it captured).
    state->cv.wait(lock, [&state, &key, self_id]() {
      for (const auto& active : state->active) {
        if (active.first == key && active.second != self_id) {
          return false;
        }
      }
      return true;
    });
  }

  std::shared_ptr<State> state_;
};

}  // namespace firebase::storage::internal

#endif  // FIREBASE_STORAGE_SRC_STUB_LISTENER_STUB_H_
