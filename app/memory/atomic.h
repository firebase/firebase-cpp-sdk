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

#ifndef FIREBASE_APP_CLIENT_CPP_MEMORY_ATOMIC_H_
#define FIREBASE_APP_CLIENT_CPP_MEMORY_ATOMIC_H_
#include <cstddef>
#include <cstdint>
#if !defined(_STLPORT_VERSION)
#include <atomic>
#endif  // !defined(_STLPORT_VERSION)

#if !defined(FIREBASE_NAMESPACE)
#define FIREBASE_NAMESPACE firebase
#endif

namespace FIREBASE_NAMESPACE {
namespace compat {

// For now only the types below are allowed to be atomic. Feel free to add more
// as needed in accordance with:
// `GCC allows any integral scalar or pointer type that is 1, 2, 4, or 8 bytes
// in length.`
template <typename T>
struct CanBeAtomic {
  static constexpr bool value = false;
};

template <>
struct CanBeAtomic<int32_t> {
  static constexpr bool value = true;
};

template <>
struct CanBeAtomic<uint32_t> {
  static constexpr bool value = true;
};

template <>
struct CanBeAtomic<int64_t> {
  static constexpr bool value = true;
};

template <>
struct CanBeAtomic<uint64_t> {
  static constexpr bool value = true;
};

// Provides a minimal atomic counter, required to implement SharedPtr.
// If std::atomic is present, delegates to it. Otherwise(when compiling against
// STLPort on Android) delegates to gcc- and clang-provided atomic built-ins.
// See: https://gcc.gnu.org/onlinedocs/gcc-4.4.5/gcc/Atomic-Builtins.html
//
// Note: Only Sequential Consistency memory order is currently supported.
template <typename T>
class Atomic {
  static_assert(CanBeAtomic<T>::value, "Provided type cannot be atomic.");

 public:
  // Default constructed counter is initialized with 0.
  Atomic() : Atomic(0) {}

  explicit Atomic(T value) { store(value); }

  Atomic& operator=(T value) {
    store(value);
    return *this;
  }

  // Atomically loads the currently stored value.
  T load() const;

  // Atomically stores a new value.
  void store(T value);

  // Atomically adds the value of arg to the currenly stored value.
  // Returns the value as observed before the operation.
  T fetch_add(T arg);

  // Atomically subtracts the value of arg from the currenly stored value.
  // Returns the value as observed before the operation.
  T fetch_sub(T arg);

 private:
#if defined(_STLPORT_VERSION)
  T value_;
#else   // defined(_STLPORT_VERSION)
  std::atomic<T> value_;
#endif  // defined(_STLPORT_VERSION)
};

#if defined(_STLPORT_VERSION)

template <typename T>
T Atomic<T>::load() const {
  return __atomic_load_n(&value_, __ATOMIC_SEQ_CST);
}

template <typename T>
void Atomic<T>::store(T value) {
  __atomic_store_n(&value_, value, __ATOMIC_SEQ_CST);
}

template <typename T>
T Atomic<T>::fetch_add(T arg) {
  return __atomic_fetch_add(&value_, arg, __ATOMIC_SEQ_CST);
}

template <typename T>
T Atomic<T>::fetch_sub(T arg) {
  return __atomic_fetch_sub(&value_, arg, __ATOMIC_SEQ_CST);
}

#else  // defined(_STLPORT_VERSION)

template <typename T>
T Atomic<T>::load() const {
  return value_.load();
}

template <typename T>
void Atomic<T>::store(T value) {
  value_.store(value);
}

template <typename T>
T Atomic<T>::fetch_add(T arg) {
  return value_.fetch_add(arg);
}

template <typename T>
T Atomic<T>::fetch_sub(T arg) {
  return value_.fetch_sub(arg);
}

#endif  // defined(_STLPORT_VERSION)

}  // namespace compat
// NOLINTNEXTLINE - allow namespace overridden
}  // namespace FIREBASE_NAMESPACE
#endif  // FIREBASE_APP_CLIENT_CPP_MEMORY_ATOMIC_H_
