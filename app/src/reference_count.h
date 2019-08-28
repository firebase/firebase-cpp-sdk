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

#ifndef FIREBASE_APP_CLIENT_CPP_SRC_REFERENCE_COUNT_H_
#define FIREBASE_APP_CLIENT_CPP_SRC_REFERENCE_COUNT_H_

#include "app/src/mutex.h"

namespace FIREBASE_NAMESPACE {
namespace internal {

// Reference counter.
// To use this in a thread safe fashion, this class should be used in with
// ReferenceCountLock or ReferenceCountedInitializer.
class ReferenceCount {
 public:
  // Initialize with no references.
  ReferenceCount() : references_(0) {}

  // Increase the reference count, returning the previous number of references.
  int AddReference() {
    MutexLock lock(mutex_);
    int previous_references = references_;
    references_++;
    return previous_references;
  }

  // Decrease the reference count, returning the previous number of references.
  // If the object has no references the count is not decreased.
  int RemoveReference() {
    MutexLock lock(mutex_);
    int previous_references = references_;
    if (references_) references_--;
    return previous_references;
  }

  // Remove all references to this object.
  // This should only be used to clean up during initialization of an object
  // while holding the mutex.
  int RemoveAllReferences() {
    MutexLock lock(mutex_);
    int previous_references = references_;
    references_ = 0;
    return previous_references;
  }

  // Get the current number of references.
  int references() {
    MutexLock lock(mutex_);
    return references_;
  }

  // Get the mutex that guards this object.
  Mutex& mutex() { return mutex_; }

 private:
  // Number of references to this object.
  int references_;
  Mutex mutex_;  // Allows users to guard references_.
};

// Increases ReferenceCount (of type T) while the lock is active.
template <typename T>
class ReferenceCountLock {
 public:
  // Acquire the reference count lock and hold a reference for the lifetime of
  // this object.
  explicit ReferenceCountLock(T* reference_count)
      : reference_count_(reference_count), lock_(reference_count->mutex()) {
    reference_count->AddReference();
  }

  ~ReferenceCountLock() { reference_count_->RemoveReference(); }

  // Increase the number of references returning the previous count excluding
  // the reference added by this lock.
  int AddReference() {
    return GetBaseReferences(reference_count_->AddReference());
  }

  // Decrease the number of references returning the previous count excluding
  // the reference added by this lock.
  // If the object has no references the count is not decreased.
  int RemoveReference() {
    return GetBaseReferences(reference_count_->RemoveReference());
  }

  // Remove all references to this object.
  // This should only be used to clean up during initialization of an object
  // while holding the mutex.
  int RemoveAllReferences() {
    return GetBaseReferences(reference_count_->RemoveAllReferences());
  }

  // Get the current number of references excluding the reference added by this
  // lock.
  int references() const {
    return GetBaseReferences(reference_count_->references());
  }

 private:
  // Remove the lock's reference from the specified reference count.
  static int GetBaseReferences(int count) { return count ? count - 1 : count; }

 private:
  T* reference_count_;
  MutexLock lock_;
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
// static ReferenceCountedInitializer g_initializer(
//   InitializeInternal, TerminateInternal, nullptr);  // NOLINT
//
// bool Initialize() {
//   return g_initializer.AddReference() > 0;
// }
//
// void DoSomethingWithModuleState() {
//   ReferenceCountLock<ReferenceCountedInitializer> lock(&g_initializer);
//   assert(g_initializer.references() > 0);  // Initialized?
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
  // Called when the reference count is increased from 0 to 1.
  typedef bool (*Initialize)(T* context);
  // Called when the reference count is decreased from 1 to 0.
  typedef void (*Terminate)(T* context);

 public:
  // Construct the object with no initialize or terminate methods.
  ReferenceCountedInitializer()
      : initialize_(nullptr), terminate_(nullptr), context_(nullptr) {}

  // Construct the object with just a terminate method.
  ReferenceCountedInitializer(Terminate terminate, T* context)
      : initialize_(nullptr), terminate_(terminate), context_(context) {}

  // Construct the object, both initialize and terminate are optional.
  ReferenceCountedInitializer(Initialize initialize, Terminate terminate,
                              T* context)
      : initialize_(initialize), terminate_(terminate), context_(context) {}

  // Increase the reference count calling the specified initialization method
  // with context if increasing the reference count from 0 to 1, returning the
  // previous reference count.
  // A reference count of -1 is returned if initialization fails.
  int AddReference(Initialize initialize, T* context) {
    ReferenceCountLock<ReferenceCount> lock(&count_);
    int previous_references = lock.AddReference();
    if (previous_references == 0 && initialize && !initialize(context)) {
      lock.RemoveReference();
      return -1;
    }
    return previous_references;
  }

  // Increase the reference count calling the initialize method if increasing
  // the reference count from 0 to 1, returning the previous reference count.
  // A reference count of -1 is returned if initialization fails.
  int AddReference() { return AddReference(initialize_, context_); }

  // Decrease the reference count, returning the previous number of references.
  int RemoveReference() {
    ReferenceCountLock<ReferenceCount> lock(&count_);
    int previous_references = lock.RemoveReference();
    if (previous_references == 1) ExecuteTerminate();
    return previous_references;
  }

  // Clear the reference count and run the registered terminate method.
  // This can be used to reset the reference count during initialization.
  int RemoveAllReferences() {
    MutexLock lock(count_.mutex());
    int previous_references = count_.RemoveAllReferences();
    if (previous_references) ExecuteTerminate();
    return previous_references;
  }

  // Clear the reference count without running the register terminate method.
  // This can be used to reset the reference count during initialization.
  int RemoveAllReferencesWithoutTerminate() {
    MutexLock lock(count_.mutex());
    return count_.RemoveAllReferences();
  }

  // Get the current number of references.
  int references() { return count_.references(); }

  // Get the mutex that guards this object.
  Mutex& mutex() { return count_.mutex(); }

  // Get the initialize method.
  Initialize initialize() const { return initialize_; }

  // Get the terminate method.
  Terminate terminate() const { return terminate_; }

  // Set the initialization context.
  void set_context(T* new_context) {
    MutexLock lock(mutex());
    context_ = new_context;
  }

  // Get the context for the initializer.
  T* context() const { return context_; }

 private:
  // Execute the terminate method.
  void ExecuteTerminate() { if (terminate_) terminate_(context_); }

 private:
  ReferenceCount count_;
  Initialize initialize_;
  Terminate terminate_;
  T* context_;
};

}  // namespace internal
}  // namespace FIREBASE_NAMESPACE

#endif  // FIREBASE_APP_CLIENT_CPP_SRC_REFERENCE_COUNT_H_
