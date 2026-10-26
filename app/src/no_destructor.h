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
#include <utility>

namespace firebase {

// Wraps a T so that its destructor is never run.
//
// Intended for function-local statics that must outlive static destruction,
// such as the registries and mutexes that Firebase instances touch from their
// own destructors. A plain `static T t;` is destroyed at exit in reverse order
// of construction, so an instance owned by a user's static object can find it
// already gone. A `static T* t = new T;` avoids that but hands callers a raw
// pointer and a heap allocation. NoDestructor keeps the T inline, constructs it
// in place on first use, and simply never destroys it.
//
//   static Mutex& RegistryMutex() {
//     static NoDestructor<Mutex> mutex;
//     return *mutex;
//   }
//
// The object is reachable from the static for the lifetime of the process, so
// leak checkers do not report it.
template <typename T>
class NoDestructor {
 public:
  // Constructs the wrapped T in place, forwarding any arguments.
  template <typename... Args>
  explicit NoDestructor(Args&&... args) {
    new (&storage_) T(std::forward<Args>(args)...);
  }

  NoDestructor(const NoDestructor&) = delete;
  NoDestructor& operator=(const NoDestructor&) = delete;

  // Intentionally does not destroy the wrapped T.
  ~NoDestructor() = default;

  T& operator*() { return *get(); }
  T* operator->() { return get(); }
  T* get() { return reinterpret_cast<T*>(&storage_); }

  const T& operator*() const { return *get(); }
  const T* operator->() const { return get(); }
  const T* get() const { return reinterpret_cast<const T*>(&storage_); }

 private:
  alignas(T) unsigned char storage_[sizeof(T)];
};

}  // namespace firebase

#endif  // FIREBASE_APP_SRC_NO_DESTRUCTOR_H_
