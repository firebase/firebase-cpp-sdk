// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef FIREBASE_APP_CLIENT_CPP_SRC_SAFE_REFERENCE_H_
#define FIREBASE_APP_CLIENT_CPP_SRC_SAFE_REFERENCE_H_

#include "app/memory/shared_ptr.h"
#include "app/src/mutex.h"

#if !defined(FIREBASE_NAMESPACE)
#define FIREBASE_NAMESPACE firebase
#endif

namespace FIREBASE_NAMESPACE {
namespace internal {

// TODO(b/130544650): Add unit test to this file.

// SafeReference owns a pointers to an object which can be deleted in anytime.
// SafeReference can be shared to different thread that potentially have longer
// life-time than the object itself.
// For example, when an object try to share "this" pointer to a scheduled
// callback but do not want to keep track of every callback it scheduled.
// When the object is about to be deleted, the object itself or the owner of
// the object is responsible to use ClearReference() to clear the reference.
// When any thread need to get the reference, it should either lock the mutex
// before calling GetReferenceUnsafe() or simply use SafeReferenceLock.
template <typename T>
class SafeReference {
 public:
  explicit SafeReference(T* ref) : data_(new ReferenceData(ref)) {}

  Mutex& GetMutex() { return data_->mutex; }

  T* GetReferenceUnsafe() { return data_->ref; }

  void ClearReference() {
    MutexLock lock(data_->mutex);
    data_->ref = nullptr;
  }

 private:
  struct ReferenceData {
    explicit ReferenceData(T* ref) : mutex(Mutex::kModeRecursive), ref(ref) {}

    Mutex mutex;
    T* ref;
  };

  SharedPtr<ReferenceData> data_;
};

// SafeReferenceLock is used to lock and to safely obtain the reference.
// When this is created, it locks the reference immediately so that no other
// thread can modify the reference.  It releases the reference once deleted.
template <typename T>
class SafeReferenceLock {
 public:
  explicit SafeReferenceLock(SafeReference<T>* ref)
      : ref_(ref), lock_(ref->GetMutex()) {}

  T* GetReference() { return ref_->GetReferenceUnsafe(); }

 private:
  SafeReference<T>* ref_;
  MutexLock lock_;
};

// A macro to check if the safe_reference is valid.  Early out if not.
#define SAFE_REFERENCE_RETURN_VOID_IF_INVALID(lock_type, lock_name, \
                                              safe_reference)       \
  lock_type lock_name(&safe_reference);                             \
  if (!lock_name.GetReference()) return;

}  // namespace internal
}  // namespace FIREBASE_NAMESPACE

#endif  // FIREBASE_APP_CLIENT_CPP_SRC_SAFE_REFERENCE_H_
