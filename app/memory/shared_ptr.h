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

#ifndef FIREBASE_APP_CLIENT_CPP_MEMORY_SHARED_PTR_H_
#define FIREBASE_APP_CLIENT_CPP_MEMORY_SHARED_PTR_H_
#include <cstdint>

#include "app/memory/atomic.h"
#include "app/memory/unique_ptr.h"
#include "app/meta/move.h"
#include "app/src/include/firebase/internal/type_traits.h"

#if !defined(FIREBASE_NAMESPACE)
#define FIREBASE_NAMESPACE firebase
#endif

namespace FIREBASE_NAMESPACE {

namespace internal {

// Control structure that contains an atomic reference count.
// Used to implement SharedPtr.
class ControlBlock {
 public:
  ControlBlock() : ref_count_(1) {}

  // Increase the reference count by one.
  // Returns the newly updated reference count.
  uint64_t ref() { return ref_count_.fetch_add(1) + 1; }

  // Decrease the reference count by one.
  // Returns the newly updated reference count.
  uint64_t deref() { return ref_count_.fetch_sub(1) - 1; }

  // Returns current reference count.
  uint64_t ref_count() const { return ref_count_.load(); }

 private:
  ::FIREBASE_NAMESPACE::compat::Atomic<uint64_t> ref_count_;
};

}  // namespace internal

// A simple implementation of shared smart pointer similar to std::shared_ptr.
//
// Note: does not support custom deleters as std::shared_ptr does.
template <typename T>
class SharedPtr final {
  static_assert(!is_array<T>::value,
                "SharedPtr is not implemented for arrays, b/68390001.");

 public:
  // Default consturcted SharedPtr's contain no managed pointer.
  SharedPtr() : ptr_(nullptr), ctrl_(nullptr) {}

  // Takes ownership of the provided ptr argument.
  template <typename U>
  explicit SharedPtr(U* ptr);

  // Copy and move constructors.
  //
  // Note: They are needed in addition to the templated "converting"
  // constructors below because, if they skipped, the compiler will generate
  // default copy and move constructors which don't behave as needed.
  SharedPtr(const SharedPtr& other);
  SharedPtr(SharedPtr&& other);

  // Copy constructor that allows creating SharedPtr<Base> from a
  // SharedPtr<Derived>.
  template <typename U>
  SharedPtr(const SharedPtr<U>& other);  // NOLINT

  // Move constructor that allows creating SharedPtr<Base> from a
  // SharedPtr<Derived>.
  template <typename U>
  SharedPtr(SharedPtr<U>&& other);  // NOLINT

  SharedPtr& operator=(const SharedPtr& other) {
    MaybeDestroy();
    ptr_ = other.ptr_;
    ctrl_ = other.ctrl_;
    ctrl_->ref();
    return *this;
  }

  SharedPtr& operator=(SharedPtr&& other) {
    MaybeDestroy();
    ptr_ = other.ptr_;
    ctrl_ = other.ctrl_;
    other.Clear();
    return *this;
  }

  // Returns the contained pointer, nullptr if the SharedPtr is empty.
  T* get() const { return ptr_; }

  // Returns the contained pointer, nullptr if the SharedPtr is empty.
  T* operator->() const { return ptr_; }

  // Returns a reference to the object cointed to by the managed pointer.
  //
  // Warning: It is undefined behavior to dereference an empty SharedPtr.
  T& operator*() const { return *ptr_; }

  // Returns the number of SharedPtr instances that point to the managed
  // pointer.
  //
  // Note: Returns 0 if called on an empty SharedPtr.
  uint64_t use_count() const { return ptr_ ? ctrl_->ref_count() : 0; }

  // Releases ownership of the managed pointer and clear the pointer. SharedPtr
  // is reusable after reset() is called.
  void reset();

  ~SharedPtr();

  // Implicit conversion to bool, which is true if the managed pointer is not
  // null.
  operator bool() const { return ptr_ != nullptr; }  // NOLINT

  // Required to implement converting constructors.
  template <typename U>
  friend class SharedPtr;

 private:
  // If the ptr_ is not null, decreases ref-count and deletes the ptr_ if
  // ref-count becomes zero.
  void MaybeDestroy();

  // Sets ptr_ and ctrl_ to null.
  void Clear();

  // Managed pointer.
  T* ptr_;

  // Control block that holds the reference count.
  internal::ControlBlock* ctrl_;
};

// Creates a SharedPtr that takes ownership of the |ptr| parameter.
template <typename T>
SharedPtr<T> WrapShared(T* ptr) {
  return SharedPtr<T>(ptr);
}

// Creates a SharedPtr<T> given constructor arguments for T.
template <typename T, typename... Args>
SharedPtr<T> MakeShared(Args&&... args) {
  return SharedPtr<T>(new T(Forward<Args>(args)...));
}

template <typename T>
template <typename U>
SharedPtr<T>::SharedPtr(U* ptr) : ptr_(ptr), ctrl_(nullptr) {
  if (ptr_) {
    // ControlBlock allocation can throw, so we wrap ptr_ in UniquePtr to make
    // sure it is released unless released after the allocation.
    auto hold = WrapUnique(ptr_);
    ctrl_ = new internal::ControlBlock();
    hold.release();
  }
}

template <typename T>
SharedPtr<T>::SharedPtr(const SharedPtr& other)
    : ptr_(other.ptr_), ctrl_(other.ctrl_) {
  if (ptr_) {
    ctrl_->ref();
  }
}

template <typename T>
template <typename U>
SharedPtr<T>::SharedPtr(const SharedPtr<U>& other)
    : ptr_(other.ptr_), ctrl_(other.ctrl_) {
  if (ptr_) {
    ctrl_->ref();
  }
}

template <typename T>
SharedPtr<T>::SharedPtr(SharedPtr&& other)
    : ptr_(other.ptr_), ctrl_(other.ctrl_) {
  other.Clear();
}

template <typename T>
template <typename U>
SharedPtr<T>::SharedPtr(SharedPtr<U>&& other)
    : ptr_(other.ptr_), ctrl_(other.ctrl_) {
  other.Clear();
}

template <typename T>
SharedPtr<T>::~SharedPtr() {
  MaybeDestroy();
}

template <typename T>
void SharedPtr<T>::reset() {
  MaybeDestroy();
  Clear();
}

template <typename T>
void SharedPtr<T>::MaybeDestroy() {
  if (ptr_ != nullptr && ctrl_->deref() == 0) {
    delete ptr_;
    delete ctrl_;
  }
}

template <typename T>
void SharedPtr<T>::Clear() {
  ptr_ = nullptr;
  ctrl_ = nullptr;
}
// NOLINTNEXTLINE - allow namespace overridden
}  // namespace FIREBASE_NAMESPACE
#endif  // FIREBASE_APP_CLIENT_CPP_MEMORY_SHARED_PTR_H_
