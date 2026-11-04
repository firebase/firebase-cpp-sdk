/*
 * Copyright 2017 Google LLC
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

#ifndef FIREBASE_APP_SRC_SEMAPHORE_H_
#define FIREBASE_APP_SRC_SEMAPHORE_H_

#include <chrono>
#include <condition_variable>
#include <mutex>

#include "app/src/time.h"

namespace firebase {

class Semaphore {
 public:
  explicit Semaphore(int initial_count) : count_(initial_count) {}
  ~Semaphore() { std::lock_guard<std::mutex> lock(mutex_); }
  Semaphore(const Semaphore&) = delete;
  Semaphore& operator=(const Semaphore&) = delete;

  void Post() {
    std::lock_guard<std::mutex> lock(mutex_);
    ++count_;
    cond_.notify_one();
  }

  void Wait() {
    std::unique_lock<std::mutex> lock(mutex_);
    cond_.wait(lock, [this] { return count_ > 0; });
    --count_;
  }

  bool TryWait() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (count_ <= 0) return false;
    --count_;
    return true;
  }

  bool TimedWait(int milliseconds) {
    if (milliseconds <= 0) return TryWait();
    std::unique_lock<std::mutex> lock(mutex_);
    if (!cond_.wait_for(lock, std::chrono::milliseconds(milliseconds),
                        [this] { return count_ > 0; })) {
      return false;
    }
    --count_;
    return true;
  }

 private:
  std::mutex mutex_;
  std::condition_variable cond_;
  int count_;
};

}  // namespace firebase

#endif  // FIREBASE_APP_SRC_SEMAPHORE_H_
