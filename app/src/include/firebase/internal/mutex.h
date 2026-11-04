/*
 * Copyright 2016 Google LLC
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
#ifndef FIREBASE_APP_SRC_INCLUDE_FIREBASE_INTERNAL_MUTEX_H_
#define FIREBASE_APP_SRC_INCLUDE_FIREBASE_INTERNAL_MUTEX_H_

#include <mutex>
#include <variant>

namespace firebase {

class Mutex {
 public:
  enum Mode {
    kModeNonRecursive = (1 << 0),
    kModeRecursive = (1 << 1),
  };

  Mutex() : Mutex(kModeRecursive) {}
  explicit Mutex(Mode mode) {
    if (mode & kModeRecursive) {
      mutex_.emplace<std::recursive_mutex>();
    } else {
      mutex_.emplace<std::mutex>();
    }
  }
  ~Mutex() = default;
  Mutex(const Mutex&) = delete;
  Mutex& operator=(const Mutex&) = delete;

  void Acquire() {
    std::visit([](auto& m) { m.lock(); }, mutex_);
  }
  void Release() {
    std::visit([](auto& m) { m.unlock(); }, mutex_);
  }

 private:
  std::variant<std::mutex, std::recursive_mutex> mutex_;
};

class MutexLock {
 public:
  explicit MutexLock(Mutex& mutex) : mutex_(&mutex) { mutex_->Acquire(); }
  ~MutexLock() { mutex_->Release(); }
  MutexLock(const MutexLock&) = delete;
  MutexLock& operator=(const MutexLock&) = delete;

 private:
  Mutex* mutex_;
};

}  // namespace firebase

#endif  // FIREBASE_APP_SRC_INCLUDE_FIREBASE_INTERNAL_MUTEX_H_
