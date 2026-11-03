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
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

#include "firebase/storage/listener.h"

namespace firebase::storage::internal {

class ListenerInternal {
 public:
  explicit ListenerInternal(Listener* listener) { (void)listener; }

  ListenerInternal(const ListenerInternal&) = delete;
  ListenerInternal& operator=(const ListenerInternal&) = delete;
  ListenerInternal(ListenerInternal&&) = delete;
  ListenerInternal& operator=(ListenerInternal&&) = delete;

  ~ListenerInternal() { DetachAll(); }

  // Registers a callback under an auto-generated numeric token (never 0).
  uint64_t RegisterDetachCallback(std::function<void()> callback) {
    if (!callback) return 0;
    std::lock_guard<std::mutex> lock(mutex_);
    const uint64_t token = next_token_++;
    detach_callbacks_.emplace_back(Key{KeyKind::kNumeric, token},
                                   std::move(callback));
    return token;
  }

  // Registers (or replaces) a callback keyed by an object address. Pointer
  // keys live in a separate key space from numeric tokens, so they can never
  // collide.
  void RegisterDetachCallback(void* token, std::function<void()> callback) {
    if (token == nullptr || !callback) return;
    const Key key = PointerKey(token);
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& entry : detach_callbacks_) {
      if (entry.first == key) {
        entry.second = std::move(callback);
        return;
      }
    }
    detach_callbacks_.emplace_back(key, std::move(callback));
  }

  void UnregisterDetachCallback(uint64_t token) {
    if (token == 0) return;
    Unregister(Key{KeyKind::kNumeric, token});
  }

  void UnregisterDetachCallback(void* token) {
    if (token == nullptr) return;
    Unregister(PointerKey(token));
  }

  // Runs and removes every registered callback. Callbacks run without the
  // lock held; callbacks registered while DetachAll() is running are also
  // run. Returns only once no callback is executing on another thread.
  void DetachAll() {
    const std::thread::id self_id = std::this_thread::get_id();
    std::unique_lock<std::mutex> lock(mutex_);
    while (!detach_callbacks_.empty()) {
      auto entry = std::move(detach_callbacks_.front());
      detach_callbacks_.pop_front();
      active_tokens_.emplace_back(entry.first, self_id);
      lock.unlock();
      {
        // RAII: remove the active marker and wake waiters even if the
        // callback throws.
        ActiveScope scope{this, entry.first, self_id};
        if (entry.second) {
          entry.second();
        }
      }
      lock.lock();
    }
    cv_.wait(lock, [this, self_id]() {
      for (const auto& active : active_tokens_) {
        if (active.second != self_id) {
          return false;
        }
      }
      return true;
    });
  }

  void Detach() { DetachAll(); }

  static ListenerInternal* GetImpl(Listener* listener) {
    return listener ? listener->impl_ : nullptr;
  }

 private:
  enum class KeyKind { kNumeric, kPointer };
  using Key = std::pair<KeyKind, uint64_t>;

  static Key PointerKey(void* token) {
    return Key{KeyKind::kPointer,
               static_cast<uint64_t>(reinterpret_cast<uintptr_t>(token))};
  }

  struct ActiveScope {
    ListenerInternal* self;
    Key key;
    std::thread::id thread_id;
    ~ActiveScope() {
      std::lock_guard<std::mutex> lock(self->mutex_);
      auto& active = self->active_tokens_;
      for (auto it = active.begin(); it != active.end(); ++it) {
        if (it->first == key && it->second == thread_id) {
          active.erase(it);
          break;
        }
      }
      self->cv_.notify_all();
    }
  };

  void Unregister(const Key& key) {
    const std::thread::id self_id = std::this_thread::get_id();
    std::unique_lock<std::mutex> lock(mutex_);
    for (auto it = detach_callbacks_.begin(); it != detach_callbacks_.end();
         ++it) {
      if (it->first == key) {
        detach_callbacks_.erase(it);
        break;
      }
    }
    // Don't return while this key's callback is still running on another
    // thread (the caller may be about to destroy what it captured).
    cv_.wait(lock, [this, &key, self_id]() {
      for (const auto& active : active_tokens_) {
        if (active.first == key && active.second != self_id) {
          return false;
        }
      }
      return true;
    });
  }

  std::mutex mutex_;
  std::condition_variable cv_;
  uint64_t next_token_ = 1;
  std::deque<std::pair<Key, std::function<void()>>> detach_callbacks_;
  std::vector<std::pair<Key, std::thread::id>> active_tokens_;
};

}  // namespace firebase::storage::internal

#endif  // FIREBASE_STORAGE_SRC_STUB_LISTENER_STUB_H_
