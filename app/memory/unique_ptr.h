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

#ifndef FIREBASE_APP_CLIENT_CPP_MEMORY_UNIQUE_PTR_H_
#define FIREBASE_APP_CLIENT_CPP_MEMORY_UNIQUE_PTR_H_

#include "app/meta/move.h"
#include "app/src/include/firebase/internal/type_traits.h"

#if !defined(FIREBASE_NAMESPACE)
#define FIREBASE_NAMESPACE firebase
#endif

namespace FIREBASE_NAMESPACE {

// Smart pointer that owns another object and releases it when destroyed.
//
// It is a move-only type and cannot be copied.
template <typename T>
class UniquePtr final {
  static_assert(
      !is_array<T>::value,
      "UniquePtr is not implemented for arrays, feel free to implement.");

 public:
  // Default constructor that creates an instance which does not own a pointer.
  UniquePtr() : UniquePtr(nullptr) {}

  // Constructor that takes ownership of a raw pointer.
  explicit UniquePtr(T* ptr) : ptr_(ptr) {}

  // Move constructor.
  //
  // Supports moves from UniquePtr's of a different type |U| as long as |U*| is
  // implicitly convertible to |T*|.
  template <typename U>
  UniquePtr(UniquePtr<U>&& other) : ptr_(other.release()) {}

  ~UniquePtr() { delete ptr_; }

  // Copy that actually performs a move.  This is useful for STL implementations
  // that do not support emplace (e.g stlport).
  UniquePtr(const UniquePtr& other)
      : ptr_(const_cast<UniquePtr*>(&other)->release()) {}

  // Move assignment.
  //
  // Supports moves from UniquePtr's of a different type |U| as long as |U*| is
  // implicitly convertible to |T*|.
  template <typename U>
  UniquePtr& operator=(UniquePtr<U>&& other) {
    reset(other.release());
    return *this;
  }

  UniquePtr& operator=(T* ptr) {
    reset(ptr);
    return *this;
  }

  // Copy that actually performs a move.  This is useful for STL implementations
  // that do not support emplace (e.g stlport).
  UniquePtr& operator=(const UniquePtr& other) {
    reset(const_cast<UniquePtr*>(&other)->release());
    return *this;
  }

  // Sets the pointer to |ptr| and releases the original pointer.
  void reset(T* ptr) {
    delete ptr_;
    ptr_ = ptr;
  }

  // Arrow operators that provide access to the underlying pointer through
  // UniquePtr.
  T* operator->() const { return ptr_; }

  // Dereference operators that provide access to the underlying pointer
  // through UniquePtr.
  T& operator*() const { return *ptr_; }

  // Retrieve the raw pointer, not giving up ownership of it.
  T* get() const { return ptr_; }

  // Retrieve the raw pointer, giving up ownership of it.
  T* release() {
    T* released = ptr_;
    ptr_ = nullptr;
    return released;
  }

  // Implicit conversion to bool, which is true if the managed pointer is not
  // null.
  operator bool() const { return ptr_ != nullptr; }  // NOLINT

 private:
  T* ptr_;
};

// Creates a UniquePtr that takes ownership of the |ptr| parameter.
template <typename T>
UniquePtr<T> WrapUnique(T* ptr) {
  return UniquePtr<T>(ptr);
}

// Creates a UniquePtr<T> given constructor arguments for T.
template <typename T, typename... Args>
UniquePtr<T> MakeUnique(Args&&... args) {
  return UniquePtr<T>(new T(Forward<Args>(args)...));
}

// NOLINTNEXTLINE - allow namespace overridden
}  // namespace FIREBASE_NAMESPACE

#endif  // FIREBASE_APP_CLIENT_CPP_MEMORY_UNIQUE_PTR_H_
