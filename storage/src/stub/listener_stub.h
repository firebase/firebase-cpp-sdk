// Copyright 2018 Google LLC
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

class ListenerInternal {
 public:
  explicit ListenerInternal(Listener* /*listener*/)
      : state_(std::make_shared<State>()) {}
  ListenerInternal(const ListenerInternal&) = delete;
  ListenerInternal& operator=(const ListenerInternal&) = delete;
  ListenerInternal(ListenerInternal&&) = delete;
  ListenerInternal& operator=(ListenerInternal&&) = delete;
  ~ListenerInternal() { DetachAll(); }

  uint64_t RegisterDetachCallback(std::function<void()> callback) {
    if (!callback) return 0;
    std::lock_guard<std::mutex> lock(state_->mutex);
    const uint64_t token = state_->next_token++;
    state_->callbacks.emplace_back(Key{KeyKind::kNumeric, token},
                                   std::move(callback));
    return token;
  }

  void RegisterDetachCallback(void* token, std::function<void()> callback) {
    if (!token || !callback) return;
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
    if (token != 0) Unregister(state_, Key{KeyKind::kNumeric, token});
  }
  void UnregisterDetachCallback(void* token) {
    if (token) Unregister(state_, PointerKey(token));
  }

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
          ActiveScope scope{state, entry.first, self_id};
          RunCallback(entry.second);
        }
        lock.lock();
      }
      state->cv.wait(lock, [&state, self_id]() {
        if (!state->callbacks.empty()) return true;
        for (const auto& a : state->active) {
          if (a.second != self_id) return false;
        }
        return true;
      });
      if (state->callbacks.empty()) break;
    }
  }

  void Detach() { DetachAll(); }
  static ListenerInternal* GetImpl(Listener* l) { return l ? l->impl_ : nullptr; }

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
      for (auto it = state->active.begin(); it != state->active.end(); ++it) {
        if (it->first == key && it->second == thread_id) {
          state->active.erase(it);
          break;
        }
      }
      state->cv.notify_all();
    }
  };

  static void Unregister(std::shared_ptr<State> state, const Key& key) {
    const std::thread::id self_id = std::this_thread::get_id();
    std::unique_lock<std::mutex> lock(state->mutex);
    for (auto it = state->callbacks.begin(); it != state->callbacks.end();
         ++it) {
      if (it->first == key) {
        state->callbacks.erase(it);
        break;
      }
    }
    state->cv.wait(lock, [&state, &key, self_id]() {
      for (const auto& a : state->active) {
        if (a.first == key && a.second != self_id) return false;
      }
      return true;
    });
  }

  std::shared_ptr<State> state_;
};

}  // namespace firebase::storage::internal

#endif  // FIREBASE_STORAGE_SRC_STUB_LISTENER_STUB_H_
