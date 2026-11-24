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

// Wrapper that constructs an object of type T in aligned inline storage and
// intentionally never runs its destructor.
//
// Use this ONLY for genuinely process-wide statics (e.g. opt-in global App
// registry mutex/map, library user-agent registry) so static-storage-duration
// objects never register exit-time destructors (__cxa_atexit) and remain valid
// even if user code destroys static Firebase handles during process exit.
template <typename T>
class NoDestructor {
 public:
  template <typename... Args>
  explicit NoDestructor(Args&&... args) {
    ::new (static_cast<void*>(storage_)) T(std::forward<Args>(args)...);
  }

  NoDestructor(const NoDestructor&) = delete;
  NoDestructor& operator=(const NoDestructor&) = delete;
  NoDestructor(NoDestructor&&) = delete;
  NoDestructor& operator=(NoDestructor&&) = delete;

  ~NoDestructor() = default;

  T* get() noexcept { return std::launder(reinterpret_cast<T*>(storage_)); }
  const T* get() const noexcept {
    return std::launder(reinterpret_cast<const T*>(storage_));
  }

  T& operator*() noexcept { return *get(); }
  const T& operator*() const noexcept { return *get(); }

  T* operator->() noexcept { return get(); }
  const T* operator->() const noexcept { return get(); }

 private:
  alignas(T) unsigned char storage_[sizeof(T)];
};

}  // namespace internal
}  // namespace firebase

#endif  // FIREBASE_APP_SRC_NO_DESTRUCTOR_H_
