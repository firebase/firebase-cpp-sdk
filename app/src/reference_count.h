/*
 * Copyright 2019 Google LLC
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

#ifndef FIREBASE_APP_SRC_REFERENCE_COUNT_H_
#define FIREBASE_APP_SRC_REFERENCE_COUNT_H_

#include "app/src/include/firebase/internal/mutex.h"
#include <atomic>
#include <mutex>
#include <condition_variable>

namespace firebase {
namespace internal {

// Reference counter.
// To use this in a thread safe fashion, this class should be used in with
// ReferenceCountLock or ReferenceCountedInitializer.
class ReferenceCount {
 public:
  ReferenceCount() : references_(0) {}
  ReferenceCount(const ReferenceCount&) = delete;
  ReferenceCount& operator=(const ReferenceCount&) = delete;

  int AddReference() {
    return references_.fetch_add(1, std::memory_order_acq_rel);
  }
  int RemoveReference() {
    int expected = references_.load(std::memory_order_acquire);
    while (expected > 0) {
      if (references_.compare_exchange_weak(expected, expected - 1, std::memory_order_acq_rel)) {
        break;
      }
    }
    return expected;
  }
  int RemoveAllReferences() {
    return references_.exchange(0, std::memory_order_acq_rel);
  }
  int references() const {
    return references_.load(std::memory_order_acquire);
  }
 private:
  std::atomic<int> references_;
};

// Increases ReferenceCount (of type T) while the lock is active.
template <typename T>
class ReferenceCountLock {
 public:
  explicit ReferenceCountLock(T* reference_count)
      : reference_count_(reference_count),
        holds_reference_(reference_count->AddReference() >= 0) {}
  ~ReferenceCountLock() {
    if (holds_reference_) {
      reference_count_->RemoveReference();
    }
  }
  ReferenceCountLock(const ReferenceCountLock&) = delete;
  ReferenceCountLock& operator=(const ReferenceCountLock&) = delete;
  int AddReference() {
    int previous = reference_count_->AddReference();
    if (previous < 0) return previous;
    return GetBaseReferences(previous);
  }
  int RemoveReference() {
    if (references() == 0) return 0;
    return GetBaseReferences(reference_count_->RemoveReference());
  }
  int RemoveAllReferences() {
    int previous = GetBaseReferences(reference_count_->RemoveAllReferences());
    holds_reference_ = false;
    return previous;
  }
  int references() const {
    return GetBaseReferences(reference_count_->references());
  }
 private:
  int GetBaseReferences(int count) const {
    return (holds_reference_ && count > 0) ? count - 1 : count;
  }
  T* reference_count_;
  bool holds_reference_;
};

// Object which calls the registered Initialize method when the reference
// count is increased to 1 and the registered Terminate method when the
// reference count is decreased to 0.
//
// For example, in a module:
//
// static bool InitializeInternal(void* context) {
//   // Allocate resources for the module.
// }
//
// static void TerminateInternal(void* context) {
//   // Free resources for the module.
// }
//
// static ReferenceCountedInitializer<void> g_initializer(
//   InitializeInternal, TerminateInternal, nullptr);  // NOLINT
//
// bool Initialize() {
//   return g_initializer.AddReference() >= 0;
// }
//
// void DoSomethingWithModuleState() {
//   ReferenceCountLock<ReferenceCountedInitializer<void>> lock(&g_initializer);
//   assert(lock.references() > 0);  // Initialized?
//   // Use global state.
//   // When lock is destroyed, if another thread decremented the reference
//   // count to 1 the module will be cleaned up.
// }
//
// void Terminate() {
//   g_initializer.RemoveReference();
// }
template <typename T>
class ReferenceCountedInitializer {
 public:
  using Initialize = bool (*)(T* context);
  using Terminate = void (*)(T* context);

  ReferenceCountedInitializer() : initialize_(nullptr), terminate_(nullptr), context_(nullptr) {}
  ReferenceCountedInitializer(Terminate terminate, T* context) : initialize_(nullptr), terminate_(terminate), context_(context) {}
  ReferenceCountedInitializer(Initialize initialize, Terminate terminate, T* context) : initialize_(initialize), terminate_(terminate), context_(context) {}
  ReferenceCountedInitializer(const ReferenceCountedInitializer&) = delete;
  ReferenceCountedInitializer& operator=(const ReferenceCountedInitializer&) = delete;

  int AddReference(Initialize initialize, T* context) {
    std::unique_lock<std::mutex> lock(sync_mutex_);
    sync_cv_.wait(lock, [this]() { return !sync_active_; });

    int previous_references = count_.references();
    if (previous_references == 0) {
      sync_active_ = true;
      lock.unlock();

      bool success = true;
      if (initialize) {
        success = initialize(context);
      }

      lock.lock();
      sync_active_ = false;
      sync_cv_.notify_all();

      if (!success) {
        return -1;
      }
      context_ = context;
    }
    count_.AddReference();
    return previous_references;
  }

  int AddReference() {
    return AddReference(initialize_, context_);
  }

  int RemoveReference() {
    std::unique_lock<std::mutex> lock(sync_mutex_);
    sync_cv_.wait(lock, [this]() { return !sync_active_; });

    int previous_references = count_.RemoveReference();
    if (previous_references == 1) {
      sync_active_ = true;
      lock.unlock();

      ExecuteTerminate();

      lock.lock();
      sync_active_ = false;
      sync_cv_.notify_all();
    }
    return previous_references;
  }

  int RemoveAllReferences() {
    std::unique_lock<std::mutex> lock(sync_mutex_);
    sync_cv_.wait(lock, [this]() { return !sync_active_; });

    int previous_references = count_.RemoveAllReferences();
    if (previous_references > 0) {
      sync_active_ = true;
      lock.unlock();

      ExecuteTerminate();

      lock.lock();
      sync_active_ = false;
      sync_cv_.notify_all();
    }
    return previous_references;
  }

  int RemoveAllReferencesWithoutTerminate() {
    return count_.RemoveAllReferences();
  }

  int references() const { return count_.references(); }
  Initialize initialize() const { return initialize_; }
  Terminate terminate() const { return terminate_; }

  void set_context(T* new_context) {
    std::lock_guard<std::mutex> lock(sync_mutex_);
    context_ = new_context;
  }

  T* context() const {
    std::lock_guard<std::mutex> lock(sync_mutex_);
    return context_;
  }

 private:
  void ExecuteTerminate() {
    if (terminate_) terminate_(context_);
  }

  ReferenceCount count_;
  Initialize initialize_;
  Terminate terminate_;
  T* context_;
  
  mutable std::mutex sync_mutex_;
  std::condition_variable sync_cv_;
  bool sync_active_ = false;
};

}  // namespace internal
}  // namespace firebase

#endif  // FIREBASE_APP_SRC_REFERENCE_COUNT_H_
