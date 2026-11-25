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

#ifndef FIREBASE_APP_SRC_NATIVE_HANDLE_TABLE_H_
#define FIREBASE_APP_SRC_NATIVE_HANDLE_TABLE_H_

#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <utility>

namespace firebase {
namespace internal {

// Thread-safe table mapping opaque, non-zero, ABA-proof int64_t handles
// (suitable for JNI jlong / ObjC bridge tokens) to std::weak_ptr<T>.
//
// Safe to store in NoDestructor<NativeHandleTable<T>> with zero exit-time
// destructors. Locking a handle returns a strong std::shared_ptr<T> that keeps
// the target alive for the duration of the native callback, or nullptr if the
// handle was unregistered or the target has already been destroyed.
template <typename T>
class NativeHandleTable {
 public:
  NativeHandleTable() = default;
  NativeHandleTable(const NativeHandleTable&) = delete;
  NativeHandleTable& operator=(const NativeHandleTable&) = delete;

  int64_t Register(std::weak_ptr<T> target) {
    std::lock_guard<std::mutex> lock(mutex_);
    // Lazily prune expired entries periodically to prevent unbounded growth if
    // a bridge drops without calling Unregister().
    if (entries_.size() > 64) {
      for (auto it = entries_.begin(); it != entries_.end();) {
        if (it->second.expired()) {
          it = entries_.erase(it);
        } else {
          ++it;
        }
      }
    }
    int64_t id = next_id_++;
    if (id == 0) {
      id = next_id_++;
    }
    entries_.emplace(id, std::move(target));
    return id;
  }

  std::shared_ptr<T> Lock(int64_t id) const {
    if (id == 0) return {};
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = entries_.find(id);
    if (it == entries_.end()) return {};
    return it->second.lock();
  }

  void Unregister(int64_t id) {
    if (id == 0) return;
    std::lock_guard<std::mutex> lock(mutex_);
    entries_.erase(id);
  }

 private:
  mutable std::mutex mutex_;
  int64_t next_id_ = 1;
  std::map<int64_t, std::weak_ptr<T>> entries_;
};

// Thread-safe table mapping opaque int64_t handles to owned std::shared_ptr<T>
// objects (e.g., JNI callback wrappers whose lifetime ends when unregistered or
// taken). Dropped shared_ptrs are always destroyed outside the table's mutex.
template <typename T>
class OwnedNativeHandleTable {
 public:
  OwnedNativeHandleTable() = default;
  OwnedNativeHandleTable(const OwnedNativeHandleTable&) = delete;
  OwnedNativeHandleTable& operator=(const OwnedNativeHandleTable&) = delete;

  int64_t Register(std::shared_ptr<T> target) {
    if (!target) return 0;
    std::lock_guard<std::mutex> lock(mutex_);
    int64_t id = next_id_++;
    if (id == 0) {
      id = next_id_++;
    }
    entries_.emplace(id, std::move(target));
    return id;
  }

  std::shared_ptr<T> Get(int64_t id) const {
    if (id == 0) return {};
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = entries_.find(id);
    if (it == entries_.end()) return {};
    return it->second;
  }

  std::shared_ptr<T> Take(int64_t id) {
    if (id == 0) return {};
    std::shared_ptr<T> taken;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      auto it = entries_.find(id);
      if (it == entries_.end()) return {};
      taken = std::move(it->second);
      entries_.erase(it);
    }
    return taken;
  }

  void Unregister(int64_t id) {
    // Take returns the shared_ptr so it is destroyed outside `mutex_`.
    (void)Take(id);
  }

 private:
  mutable std::mutex mutex_;
  int64_t next_id_ = 1;
  std::map<int64_t, std::shared_ptr<T>> entries_;
};

}  // namespace internal
}  // namespace firebase

#endif  // FIREBASE_APP_SRC_NATIVE_HANDLE_TABLE_H_
