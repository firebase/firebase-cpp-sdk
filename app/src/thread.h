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
// Representation of a single thread of execution.
//
// Limitations:
// * At this point capturing lambdas are not supported due to difficulty of
//   implementation and an extra allocation needed to store the closure.
// * An instance of Thread must be Join()'ed/Detach()'ed before it is
//   destructed.
class Thread {
  // Helper type required for successful template type deduction in the
  // constructor below.
  template <typename T>
  struct OneArgDeductionHelper {
    using type = void (*)(T*);
  };

 public:
  // Function pointer that takes a void* which is inherently unsafe.
  using UnsafeRoutine = void (*)(void*);

  // Function pointer that takes no arguments.
  using NoArgRoutine = void (*)();

  // This workaround is required for the constructor below.
  // Using the normal using (using OneArgRoutine = void (*)(T)) prevents the
  // constructor from matching.
  template <typename T>
  using OneArgRoutine = typename OneArgDeductionHelper<T>::type;

  // Returns the ID of the current thread.
  using Id = std::thread::id;
  static Id CurrentId() { return std::this_thread::get_id(); }
  static bool IsCurrentThread(const Id& thread_id) {
    return std::this_thread::get_id() == thread_id;
  }

  // A default constructed "empty" Thread does not consume any resources and has
  // no actual thread attached to it.
  Thread();

  // When a Thread is destroyed the underlying thread must be either join()'ed
  // or detach()'ed. Otherwise the process will be aborted (this is in line with
  // what std::thread does).
  ~Thread();

  // Creates a new thread of execution that invokes start_routine(arg).
  Thread(UnsafeRoutine start_routine, void* arg);

  // Creates a new thread of execution that invokes start_routine().
  explicit Thread(NoArgRoutine start_routine);

  // Creates a new thread of execution that invokes start_routine(arg).
  template <typename T>
  Thread(OneArgRoutine<T> start_routine, T* arg);

  // Copies are not allowed.
  Thread(const Thread& other) = delete;
  Thread& operator=(const Thread& other) = delete;

  // Move and move assignment.
  Thread(Thread&& other) noexcept;
  Thread& operator=(Thread&& other) noexcept;

  // A thread is joinable if it is not "empty" and has not been join()'ed or
  // detach()'ed.
  bool Joinable() const;

  // Block until the thread finishes execution. A non-detached thread must be
  // joined before the Thread object is destructed.
  void Join();

  // Releases the thread from the current object, the system will automatically
  // free resources associated with the thread when it finishes execution.
  void Detach();

 private:
  std::thread thread_;
};

template <typename T>
Thread::Thread(Thread::OneArgRoutine<T> start_routine, T* arg)
    : thread_(start_routine, arg) {}
// NOLINTNEXTLINE - allow namespace overridden
}  // namespace firebase

#endif  // FIREBASE_APP_SRC_THREAD_H_
