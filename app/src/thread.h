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

#ifndef FIREBASE_APP_CLIENT_CPP_SRC_THREAD_H_
#define FIREBASE_APP_CLIENT_CPP_SRC_THREAD_H_

#include <utility>

#if defined(_STLPORT_VERSION)
#include <pthread.h>
#else
#include <thread>  // NOLINT
#endif

#if !defined(FIREBASE_NAMESPACE)
#define FIREBASE_NAMESPACE firebase
#endif

namespace FIREBASE_NAMESPACE {
// Representation of a single thread of execution.
//
// Limitations:
// * At this point capturing lambdas are not supported due difficulty of
//   implementation and an extra allocation needed to store the closure.
// * An instance of Thread must be join()'ed/detach()'ed and destructed on the
//   thread that created it.
class Thread {
  // Helper type required for successful template type deduction in the
  // constructor below.
  template <typename T>
  struct OneArgDeductionHelper {
    typedef void (*type)(T*);
  };

 public:
  // Function pointer that takes a void* which is inherently unsafe.
  typedef void (*UnsafeRoutine)(void*);

  // Function pointer that takes no arguments.
  typedef void (*NoArgRoutine)();

  // This workaround is required for the constructor below.
  // Using the normal using (using OneArgRoutine = void (*)(T)) prevents the
  // constructor from matching.
  template <typename T>
  using OneArgRoutine = typename OneArgDeductionHelper<T>::type;

  // Returns the ID of the current thread.
#if defined(_STLPORT_VERSION)
  typedef pthread_t Id;
  static Id CurrentId() { return pthread_self(); }
  static bool IsCurrentThread(const Id& thread_id) {
    // pthread_t is an "opaque" C type and must be compared with pthread_equal.
    return pthread_equal(pthread_self(), thread_id) != 0;
  }
#else
  typedef std::thread::id Id;
  static Id CurrentId() { return std::this_thread::get_id(); }
  static bool IsCurrentThread(const Id& thread_id) {
    return std::this_thread::get_id() == thread_id;
  }
#endif

  // A default constructed "empty" Thread does not consume any resources and has
  // no actual thread attached to it.
  Thread();

  // When a Thread is destroyed the underlying thread must be either join()'ed
  // or detach()'ed. Otherwise the process will be aborted (this is in line with
  // what std::thread does).
  ~Thread();

  // Starts a new thread and executes start_routine(arg).
  //
  // Like std::thread, it is guaranteed that the constructor returns after the
  // thread has started execution.
  Thread(UnsafeRoutine start_routine, void* arg);

  // Starts a new thread and executes start_routine().
  //
  // Like std::thread, it is guaranteed that the constructor returns after the
  // thread has started execution.
  explicit Thread(NoArgRoutine start_routine);

  // Starts a new thread and executes start_routine(arg).
  //
  // Like std::thread, it is guaranteed that the constructor returns after the
  // thread has started execution.
  template <typename T>
  Thread(OneArgRoutine<T> start_routine, T* arg);

  // Copies are not allowed.
  Thread(const Thread& other) = delete;

  // Move and move assignment.
  Thread(Thread&& other);
  Thread& operator=(Thread&& other);

  // A thread is joinable if it is not "empty" and has not been join()'ed or
  // detach()'ed.
  bool Joinable() const;

  // Block until the thread finishes execution. A non-detached thread must be
  // joined before the Thread object is destructed.
  void Join();

  // Releases the thread from the current object, the system will automatically
  // free resources associated with the thread when it finishes execution.
  void Detach();

#if defined(_STLPORT_VERSION)
  // Provides an extension mechanism to implement Thread constructors.
  typedef void (*AdapterRoutine)(UnsafeRoutine routine, void* arg);

 private:
  Thread(UnsafeRoutine start_routine, void* arg,
         AdapterRoutine adapter_routine);

  pthread_t thread_;
  // TODO(vkryachko): replace with atomic<bool> + cas (or guard modifications
  // with a mutex) to allow moving the object between threads.
  bool is_joinable_;
#else

 private:
  std::thread thread_;
#endif
};

#if defined(_STLPORT_VERSION)
template <typename T>
Thread::Thread(Thread::OneArgRoutine<T> start_routine, T* arg)
    : Thread(reinterpret_cast<Thread::UnsafeRoutine>(start_routine), arg,
             [](Thread::UnsafeRoutine start_routine, void* arg) {
               reinterpret_cast<Thread::OneArgRoutine<T>>(start_routine)(
                   static_cast<T*>(arg));
             }) {}
#else
template <typename T>
Thread::Thread(Thread::OneArgRoutine<T> start_routine, T* arg)
    : thread_(start_routine, arg) {}
#endif
// NOLINTNEXTLINE - allow namespace overridden
}  // namespace FIREBASE_NAMESPACE

#endif  // FIREBASE_APP_CLIENT_CPP_SRC_THREAD_H_
