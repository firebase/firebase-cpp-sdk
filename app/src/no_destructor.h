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

#ifndef FIREBASE_APP_SRC_NO_DESTRUCTOR_H_
#define FIREBASE_APP_SRC_NO_DESTRUCTOR_H_

#include <new>
#include <type_traits>
#include <utility>

namespace firebase {
namespace internal {

// Wraps an instance of T in inline aligned storage whose destructor is never
// called.
//
// Intended for function-local statics that must outlive process teardown, such
// as global registries, mutexes, and loggers accessed when user-owned static
// Firebase objects run their destructors during exit():
//
// - A plain `static T value;` registers an exit-time destructor (`__cxa_atexit`
//   / `atexit`) that runs in reverse construction order, so destructors of
//   other static objects constructed earlier can observe a destroyed `T`.
// - A namespace-scope `static T* value = new T();` avoids exit-time
//   destruction, but introduces dynamic initialization order hazards across
//   translation units before `main()`.
// - A function-local `static NoDestructor<T> value;` is initialized thread-
//   safely on first call (C++11 [stmt.dcl]), stores `T` inline with no extra
//   heap allocation, has a trivial destructor so no exit-time destructor is
//   registered, and remains reachable via static storage so leak checkers do
//   not report it as leaked.
//
// Example:
//   static Mutex& RegistryMutex() {
//     static NoDestructor<Mutex> mutex;
//     return *mutex;
//   }
template <typename T>
class NoDestructor {
 public:
  // Constructs the wrapped T in place, forwarding any arguments.
  // Disabled when decaying a single argument yields NoDestructor itself so
  // non-const lvalue copy attempts hit the deleted copy constructor instead of
  // forwarding into T's constructors (for example, firebase::Variant).
  template <typename... Args,
            typename std::enable_if<
                !std::is_same<void(typename std::decay<Args>::type&...),
                              void(NoDestructor&)>::value,
                int>::type = 0>
  explicit NoDestructor(Args&&... args) {
    new (storage_) T(std::forward<Args>(args)...);
  }

  // Explicit copy/move overloads for T so brace-enclosed initializers (such as
  // `NoDestructor<std::vector<int>>({1, 2})`) can deduce T.
  explicit NoDestructor(const T& x) { new (storage_) T(x); }
  explicit NoDestructor(T&& x) { new (storage_) T(std::move(x)); }

  NoDestructor(const NoDestructor&) = delete;
  NoDestructor& operator=(const NoDestructor&) = delete;

  // Trivially destructible: intentionally never destroys the wrapped T.
  ~NoDestructor() = default;

  T& operator*() { return *get(); }
  T* operator->() { return get(); }
  T* get() { return std::launder(reinterpret_cast<T*>(storage_)); }

  const T& operator*() const { return *get(); }
  const T* operator->() const { return get(); }
  const T* get() const {
    return std::launder(reinterpret_cast<const T*>(storage_));
  }

 private:
  alignas(T) unsigned char storage_[sizeof(T)];
};

}  // namespace internal

using internal::NoDestructor;

}  // namespace firebase

#endif  // FIREBASE_APP_SRC_NO_DESTRUCTOR_H_
