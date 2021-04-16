// Copyright 2017 Google LLC
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

#include "app/src/assert.h"
#include "app/src/include/firebase/internal/platform.h"

// CallableReference is defined in these 3 files, one implementation for each
// OS.
#if FIREBASE_PLATFORM_ANDROID
#include "functions/src/android/callable_reference_android.h"
#include "functions/src/android/functions_android.h"
#elif FIREBASE_PLATFORM_IOS
#include "functions/src/ios/callable_reference_ios.h"
#include "functions/src/ios/functions_ios.h"
#else
#include "functions/src/desktop/callable_reference_desktop.h"
#include "functions/src/desktop/functions_desktop.h"
#endif  // FIREBASE_PLATFORM_ANDROID, FIREBASE_PLATFORM_IOS

namespace firebase {
class Variant;

namespace functions {

using internal::HttpsCallableReferenceInternal;

static void CleanupCallableReference(void* functions_reference_void) {
  auto functions_reference =
      reinterpret_cast<HttpsCallableReference*>(functions_reference_void);
  // Overwrite the existing reference with an invalid reference.
  *functions_reference = HttpsCallableReference();
}

static void RegisterForCleanup(
    HttpsCallableReference* obj,
    internal::HttpsCallableReferenceInternal* internal) {
  if (internal && internal->functions_internal()) {
    internal->functions_internal()->cleanup().RegisterObject(
        obj, CleanupCallableReference);
  }
}

static void UnregisterForCleanup(
    HttpsCallableReference* obj,
    internal::HttpsCallableReferenceInternal* internal) {
  if (internal && internal->functions_internal()) {
    internal->functions_internal()->cleanup().UnregisterObject(obj);
  }
}

HttpsCallableReference::HttpsCallableReference(
    HttpsCallableReferenceInternal* internal)
    : internal_(internal) {
  RegisterForCleanup(this, internal_);
}

HttpsCallableReference::HttpsCallableReference(
    const HttpsCallableReference& other)
    : internal_(other.internal_
                    ? new HttpsCallableReferenceInternal(*other.internal_)
                    : nullptr) {
  RegisterForCleanup(this, internal_);
}

HttpsCallableReference& HttpsCallableReference::operator=(
    const HttpsCallableReference& other) {
  UnregisterForCleanup(this, internal_);
  delete internal_;
  internal_ = other.internal_
                  ? new HttpsCallableReferenceInternal(*other.internal_)
                  : nullptr;
  RegisterForCleanup(this, internal_);
  return *this;
}

#if defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)
HttpsCallableReference::HttpsCallableReference(HttpsCallableReference&& other) {
  UnregisterForCleanup(&other, other.internal_);
  internal_ = other.internal_;
  other.internal_ = nullptr;
  RegisterForCleanup(this, internal_);
}

HttpsCallableReference& HttpsCallableReference::operator=(
    HttpsCallableReference&& other) {
  UnregisterForCleanup(this, internal_);
  delete internal_;
  UnregisterForCleanup(&other, other.internal_);
  internal_ = other.internal_;
  other.internal_ = nullptr;
  RegisterForCleanup(this, internal_);
  return *this;
}
#endif  // defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)

HttpsCallableReference::~HttpsCallableReference() {
  UnregisterForCleanup(this, internal_);
  delete internal_;
}

Functions* HttpsCallableReference::functions() {
  return internal_ ? internal_->functions() : nullptr;
}

Future<HttpsCallableResult> HttpsCallableReference::Call() {
  return internal_ ? internal_->Call() : Future<HttpsCallableResult>();
}

Future<HttpsCallableResult> HttpsCallableReference::Call(const Variant& data) {
  return internal_ ? internal_->Call(data) : Future<HttpsCallableResult>();
}

bool HttpsCallableReference::is_valid() const { return internal_ != nullptr; }

}  // namespace functions
}  // namespace firebase
