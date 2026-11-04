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

namespace firebase {
namespace functions {

namespace {
Mutex& GetCallableRefLock() {
  static auto* lock = new Mutex();
  return *lock;
}
}  // namespace

HttpsCallableReference::HttpsCallableReference(
    internal::HttpsCallableReferenceInternal* internal)
    : internal_(internal) {
}

HttpsCallableReference::HttpsCallableReference(
    const HttpsCallableReference& other)
    : internal_(nullptr) {
  MutexLock lock(GetCallableRefLock());
  if (other.internal_) {
    internal_ = new internal::HttpsCallableReferenceInternal(*other.internal_);
  }
}

HttpsCallableReference& HttpsCallableReference::operator=(
    const HttpsCallableReference& other) {
  if (this == &other) {
    return *this;
  }
  MutexLock lock(GetCallableRefLock());
  delete internal_;
  if (other.internal_) {
    internal_ = new internal::HttpsCallableReferenceInternal(*other.internal_);
  } else {
    internal_ = nullptr;
  }
  return *this;
}

HttpsCallableReference::HttpsCallableReference(
    HttpsCallableReference&& other) noexcept
    : internal_(nullptr) {
  MutexLock lock(GetCallableRefLock());
  internal_ = std::exchange(other.internal_, nullptr);
}

HttpsCallableReference& HttpsCallableReference::operator=(
    HttpsCallableReference&& other) noexcept {
  if (this == &other) {
    return *this;
  }
  MutexLock lock(GetCallableRefLock());
  delete internal_;
  internal_ = std::exchange(other.internal_, nullptr);
  return *this;
}

HttpsCallableReference::~HttpsCallableReference() {
  MutexLock lock(GetCallableRefLock());
  delete internal_;
  internal_ = nullptr;
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

}  // namespace functions
}  // namespace firebase
