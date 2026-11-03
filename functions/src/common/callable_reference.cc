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
void CleanupHttpsCallableReference(void* ref_void) {
  HttpsCallableReference* ref =
      reinterpret_cast<HttpsCallableReference*>(ref_void);
  // Overwrite the existing reference with an invalid reference (which
  // unregisters and deletes internal_).
  *ref = HttpsCallableReference();
}

void RegisterForCleanup(HttpsCallableReference* obj,
                        internal::HttpsCallableReferenceInternal* internal) {
  if (internal && internal->functions_internal()) {
    internal->functions_internal()->cleanup().RegisterObject(
        obj, CleanupHttpsCallableReference);
  }
}

void UnregisterForCleanup(
    HttpsCallableReference* obj,
    internal::HttpsCallableReferenceInternal* internal) noexcept {
  if (internal && internal->functions_internal()) {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
    try {
      internal->functions_internal()->cleanup().UnregisterObject(obj);
    } catch (...) {
      LogWarning("HttpsCallableReference: Failed to unregister object.");
    }
#else
    internal->functions_internal()->cleanup().UnregisterObject(obj);
#endif
  }
}

void SafeRegisterForCleanup(
    HttpsCallableReference* obj,
    internal::HttpsCallableReferenceInternal* internal) noexcept {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
  try {
    RegisterForCleanup(obj, internal);
  } catch (...) {
    LogWarning("HttpsCallableReference: Failed to register moved-to object.");
  }
#else
  RegisterForCleanup(obj, internal);
#endif
}

void RegisterForCleanupOrDelete(
    HttpsCallableReference* obj,
    internal::HttpsCallableReferenceInternal*& internal) {
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
  try {
    RegisterForCleanup(obj, internal);
  } catch (...) {
    delete internal;
    internal = nullptr;
    throw;
  }
#else
  RegisterForCleanup(obj, internal);
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
    : internal_(other.internal_ ? new internal::HttpsCallableReferenceInternal(
                                      *other.internal_)
                                : nullptr) {
  RegisterForCleanupOrDelete(this, internal_);
}

HttpsCallableReference& HttpsCallableReference::operator=(
    const HttpsCallableReference& other) {
  if (this == &other) {
    return *this;
  }
  std::unique_ptr<internal::HttpsCallableReferenceInternal> new_internal(
      other.internal_
          ? new internal::HttpsCallableReferenceInternal(*other.internal_)
          : nullptr);
  UnregisterForCleanup(this, internal_);
  delete internal_;
  internal_ = new_internal.release();
  RegisterForCleanupOrDelete(this, internal_);
  return *this;
}

HttpsCallableReference::HttpsCallableReference(
    HttpsCallableReference&& other) noexcept
    : internal_(std::exchange(other.internal_, nullptr)) {
  UnregisterForCleanup(&other, internal_);
  SafeRegisterForCleanup(this, internal_);
}

HttpsCallableReference& HttpsCallableReference::operator=(
    HttpsCallableReference&& other) noexcept {
  if (this == &other) {
    return *this;
  }
  UnregisterForCleanup(this, internal_);
  delete internal_;
  internal_ = std::exchange(other.internal_, nullptr);
  UnregisterForCleanup(&other, internal_);
  SafeRegisterForCleanup(this, internal_);
  return *this;
}

HttpsCallableReference::~HttpsCallableReference() {
  if (internal_) {
    UnregisterForCleanup(this, internal_);
    delete internal_;
    internal_ = nullptr;
  }
}

Functions* HttpsCallableReference::functions() {
  return internal_ ? internal_->functions() : nullptr;
}

Functions* HttpsCallableReference::functions() const {
  return internal_ ? internal_->functions() : nullptr;
}

Future<HttpsCallableResult> HttpsCallableReference::Call() {
  return internal_ ? internal_->Call() : Future<HttpsCallableResult>();
}

Future<HttpsCallableResult> HttpsCallableReference::Call() const {
  return internal_ ? internal_->Call() : Future<HttpsCallableResult>();
}

Future<HttpsCallableResult> HttpsCallableReference::Call(const Variant& data) {
  return internal_ ? internal_->Call(data) : Future<HttpsCallableResult>();
}

Future<HttpsCallableResult> HttpsCallableReference::Call(
    const Variant& data) const {
  return internal_ ? internal_->Call(data) : Future<HttpsCallableResult>();
}

Future<HttpsCallableResult> HttpsCallableReference::CallLastResult() const {
  return internal_ ? internal_->CallLastResult()
                   : Future<HttpsCallableResult>();
}

bool HttpsCallableReference::is_valid() const {
  return internal_ != nullptr && internal_->is_valid();
}

bool HttpsCallableReference::operator==(
    const HttpsCallableReference& other) const {
  if (internal_ == other.internal_) return true;
  if (!internal_ || !other.internal_) return false;
  return *internal_ == *other.internal_;
}

}  // namespace firebase::functions
