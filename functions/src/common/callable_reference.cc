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

#include "functions/src/include/firebase/functions/callable_reference.h"

#include <memory>
#include <utility>

#include "app/src/cleanup_notifier.h"
#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/include/firebase/internal/platform.h"
#include "app/src/include/firebase/variant.h"
#include "app/src/log.h"
#include "functions/src/include/firebase/functions.h"

// Include the header that matches the platform being used.
#if FIREBASE_PLATFORM_ANDROID
#include "functions/src/android/callable_reference_android.h"
#include "functions/src/android/functions_android.h"
#elif FIREBASE_PLATFORM_IOS || FIREBASE_PLATFORM_TVOS
#include "functions/src/ios/callable_reference_ios.h"
#include "functions/src/ios/functions_ios.h"
#else
#include "functions/src/desktop/callable_reference_desktop.h"
#include "functions/src/desktop/functions_desktop.h"
#endif  // FIREBASE_PLATFORM_ANDROID, FIREBASE_PLATFORM_IOS,
        // FIREBASE_PLATFORM_TVOS

namespace firebase::functions {

namespace {
Mutex& GetCallableRefLock() {
  static auto* lock = new Mutex();
  return *lock;
}

CleanupNotifier* GetNotifier(
    internal::HttpsCallableReferenceInternal* internal) noexcept {
  if (internal && internal->functions_internal()) {
    return &internal->functions_internal()->cleanup();
  }
  return nullptr;
}

void CleanupHttpsCallableReference(void* ref_void) {
  HttpsCallableReference* ref =
      reinterpret_cast<HttpsCallableReference*>(ref_void);
  // Overwrite the existing reference with an invalid reference (which
  // unregisters and deletes internal_).
  *ref = HttpsCallableReference();
}

void RegisterWithNotifier(HttpsCallableReference* obj,
                          CleanupNotifier* notifier) {
  if (notifier) {
    notifier->RegisterObject(obj, CleanupHttpsCallableReference);
  }
}

void UnregisterFromNotifier(HttpsCallableReference* obj,
                            CleanupNotifier* notifier) noexcept {
  if (notifier) {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
    try {
      notifier->UnregisterObject(obj);
    } catch (...) {
      LogWarning("HttpsCallableReference: Failed to unregister object.");
    }
#else
    notifier->UnregisterObject(obj);
#endif
  }
}

void SafeRegisterWithNotifier(HttpsCallableReference* obj,
                              CleanupNotifier* notifier) noexcept {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
  try {
    RegisterWithNotifier(obj, notifier);
  } catch (...) {
    LogWarning("HttpsCallableReference: Failed to register moved-to object.");
  }
#else
  RegisterWithNotifier(obj, notifier);
#endif
}

void RegisterForCleanupOrDelete(
    HttpsCallableReference* obj,
    internal::HttpsCallableReferenceInternal*& internal) {
  CleanupNotifier* notifier = GetNotifier(internal);
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
  try {
    RegisterWithNotifier(obj, notifier);
  } catch (...) {
    delete internal;
    internal = nullptr;
    throw;
  }
#else
  RegisterWithNotifier(obj, notifier);
#endif
}
}  // namespace

HttpsCallableReference::HttpsCallableReference(
    internal::HttpsCallableReferenceInternal* internal)
    : internal_(internal) {
  RegisterForCleanupOrDelete(this, internal_);
}

HttpsCallableReference::HttpsCallableReference(
    const HttpsCallableReference& other)
    : internal_(nullptr) {
  {
    MutexLock lock(GetCallableRefLock());
    if (other.internal_) {
      internal_ =
          new internal::HttpsCallableReferenceInternal(*other.internal_);
    }
  }
  RegisterForCleanupOrDelete(this, internal_);
}

HttpsCallableReference& HttpsCallableReference::operator=(
    const HttpsCallableReference& other) {
  if (this == &other) {
    return *this;
  }
  std::unique_ptr<internal::HttpsCallableReferenceInternal> new_internal;
  internal::HttpsCallableReferenceInternal* old_internal = nullptr;
  CleanupNotifier* old_notifier = nullptr;
  {
    MutexLock lock(GetCallableRefLock());
    if (other.internal_) {
      new_internal = std::make_unique<internal::HttpsCallableReferenceInternal>(
          *other.internal_);
    }
    old_internal = std::exchange(internal_, nullptr);
    old_notifier = GetNotifier(old_internal);
  }
  UnregisterFromNotifier(this, old_notifier);
  delete old_internal;

  internal::HttpsCallableReferenceInternal* raw_new = new_internal.release();
  CleanupNotifier* new_notifier = GetNotifier(raw_new);
  {
    MutexLock lock(GetCallableRefLock());
    internal_ = raw_new;
  }
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
  try {
    RegisterWithNotifier(this, new_notifier);
  } catch (...) {
    internal::HttpsCallableReferenceInternal* to_delete = nullptr;
    {
      MutexLock lock(GetCallableRefLock());
      to_delete = std::exchange(internal_, nullptr);
    }
    delete to_delete;
    throw;
  }
#else
  RegisterWithNotifier(this, new_notifier);
#endif
  return *this;
}

HttpsCallableReference::HttpsCallableReference(
    HttpsCallableReference&& other) noexcept
    : internal_(nullptr) {
  CleanupNotifier* other_notifier = nullptr;
  {
    MutexLock lock(GetCallableRefLock());
    other_notifier = GetNotifier(other.internal_);
  }
  // Unregister &other before stealing other.internal_ so concurrent
  // CleanupAll() either completes first or never runs on &other.
  UnregisterFromNotifier(&other, other_notifier);

  CleanupNotifier* new_notifier = nullptr;
  {
    MutexLock lock(GetCallableRefLock());
    internal_ = std::exchange(other.internal_, nullptr);
    new_notifier = GetNotifier(internal_);
  }
  SafeRegisterWithNotifier(this, new_notifier);
}

HttpsCallableReference& HttpsCallableReference::operator=(
    HttpsCallableReference&& other) noexcept {
  if (this == &other) {
    return *this;
  }
  CleanupNotifier* other_notifier = nullptr;
  {
    MutexLock lock(GetCallableRefLock());
    other_notifier = GetNotifier(other.internal_);
  }
  UnregisterFromNotifier(&other, other_notifier);

  internal::HttpsCallableReferenceInternal* old_internal = nullptr;
  CleanupNotifier* old_notifier = nullptr;
  CleanupNotifier* new_notifier = nullptr;
  {
    MutexLock lock(GetCallableRefLock());
    old_internal =
        std::exchange(internal_, std::exchange(other.internal_, nullptr));
    old_notifier = GetNotifier(old_internal);
    new_notifier = GetNotifier(internal_);
  }
  UnregisterFromNotifier(this, old_notifier);
  delete old_internal;
  SafeRegisterWithNotifier(this, new_notifier);
  return *this;
}

HttpsCallableReference::~HttpsCallableReference() {
  internal::HttpsCallableReferenceInternal* old_internal = nullptr;
  CleanupNotifier* old_notifier = nullptr;
  {
    MutexLock lock(GetCallableRefLock());
    old_internal = std::exchange(internal_, nullptr);
    old_notifier = GetNotifier(old_internal);
  }
  if (old_internal) {
    UnregisterFromNotifier(this, old_notifier);
    delete old_internal;
  }
}

Functions* HttpsCallableReference::functions() {
  return std::as_const(*this).functions();
}

Functions* HttpsCallableReference::functions() const {
  MutexLock lock(GetCallableRefLock());
  return internal_ ? internal_->functions() : nullptr;
}

Future<HttpsCallableResult> HttpsCallableReference::Call() {
  MutexLock lock(GetCallableRefLock());
  return internal_ ? internal_->Call() : Future<HttpsCallableResult>();
}

Future<HttpsCallableResult> HttpsCallableReference::Call() const {
  MutexLock lock(GetCallableRefLock());
  return internal_ ? internal_->Call() : Future<HttpsCallableResult>();
}

Future<HttpsCallableResult> HttpsCallableReference::Call(const Variant& data) {
  MutexLock lock(GetCallableRefLock());
  return internal_ ? internal_->Call(data) : Future<HttpsCallableResult>();
}

Future<HttpsCallableResult> HttpsCallableReference::Call(
    const Variant& data) const {
  MutexLock lock(GetCallableRefLock());
  return internal_ ? internal_->Call(data) : Future<HttpsCallableResult>();
}

Future<HttpsCallableResult> HttpsCallableReference::CallLastResult() const {
  MutexLock lock(GetCallableRefLock());
  return internal_ ? internal_->CallLastResult()
                   : Future<HttpsCallableResult>();
}

bool HttpsCallableReference::is_valid() const {
  MutexLock lock(GetCallableRefLock());
  return internal_ != nullptr && internal_->is_valid();
}

bool HttpsCallableReference::operator==(
    const HttpsCallableReference& other) const {
  MutexLock lock(GetCallableRefLock());
  if (internal_ == other.internal_) return true;
  if (!internal_ || !other.internal_) return false;
  return *internal_ == *other.internal_;
}

}  // namespace firebase::functions
