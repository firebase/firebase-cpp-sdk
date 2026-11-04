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

#ifndef FIREBASE_APP_SRC_THREAD_H_
#define FIREBASE_APP_SRC_THREAD_H_

#include <thread>  // NOLINT

namespace firebase {

class Thread {
  template <typename T>
  struct OneArgDeductionHelper {
    using type = void (*)(T*);
  };

 public:
  using UnsafeRoutine = void (*)(void*);
  using NoArgRoutine = void (*)();
  template <typename T>
  using OneArgRoutine = typename OneArgDeductionHelper<T>::type;
  using Id = std::thread::id;

  static Id CurrentId() { return std::this_thread::get_id(); }
  static bool IsCurrentThread(const Id& thread_id) {
    return std::this_thread::get_id() == thread_id;
  }

  Thread();
  ~Thread();
  Thread(UnsafeRoutine start_routine, void* arg);
  explicit Thread(NoArgRoutine start_routine);
  template <typename T>
  Thread(OneArgRoutine<T> start_routine, T* arg)
      : thread_(start_routine, arg) {}

  Thread(const Thread& other) = delete;
  Thread& operator=(const Thread& other) = delete;
  Thread(Thread&& other) noexcept;
  Thread& operator=(Thread&& other) noexcept;

  bool Joinable() const;
  void Join();
  void Detach();

 private:
  std::thread thread_;
};

}  // namespace firebase

#endif  // FIREBASE_APP_SRC_THREAD_H_
