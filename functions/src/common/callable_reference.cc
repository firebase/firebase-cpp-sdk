
#include "functions/src/include/firebase/functions/callable_reference.h"
#include <utility>
#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/include/firebase/variant.h"
#include "functions/src/include/firebase/functions.h"

#if FIREBASE_PLATFORM_ANDROID
#include "functions/src/android/callable_reference_android.h"
#elif FIREBASE_PLATFORM_IOS || FIREBASE_PLATFORM_TVOS
#include "functions/src/ios/callable_reference_ios.h"
#else
#include "functions/src/desktop/callable_reference_desktop.h"
#endif

namespace firebase {
namespace functions {
namespace {
Mutex& GetCallableRefLock() {
  static auto* lock = new Mutex();
  return *lock;
}
}

HttpsCallableReference::HttpsCallableReference(
    internal::HttpsCallableReferenceInternal* internal)
    : internal_(internal) {}

HttpsCallableReference::HttpsCallableReference(const HttpsCallableReference& other) : internal_(nullptr) {
  MutexLock lock(GetCallableRefLock());
  if (other.internal_) {
    internal_ = new internal::HttpsCallableReferenceInternal(*other.internal_);
  }
}

HttpsCallableReference& HttpsCallableReference::operator=(const HttpsCallableReference& other) {
  if (this == &other) return *this;
  internal::HttpsCallableReferenceInternal* new_internal = nullptr;
  {
    MutexLock lock(GetCallableRefLock());
    if (other.internal_) {
      new_internal = new internal::HttpsCallableReferenceInternal(*other.internal_);
    }
  }
  internal::HttpsCallableReferenceInternal* old_internal = nullptr;
  {
    MutexLock lock(GetCallableRefLock());
    old_internal = internal_;
    internal_ = new_internal;
  }
  delete old_internal;
  return *this;
}

HttpsCallableReference::HttpsCallableReference(HttpsCallableReference&& other) noexcept : internal_(nullptr) {
  MutexLock lock(GetCallableRefLock());
  internal_ = std::exchange(other.internal_, nullptr);
}

HttpsCallableReference& HttpsCallableReference::operator=(HttpsCallableReference&& other) noexcept {
  if (this == &other) return *this;
  internal::HttpsCallableReferenceInternal* old_internal = nullptr;
  {
    MutexLock lock(GetCallableRefLock());
    old_internal = internal_;
    internal_ = std::exchange(other.internal_, nullptr);
  }
  delete old_internal;
  return *this;
}

HttpsCallableReference::~HttpsCallableReference() {
  internal::HttpsCallableReferenceInternal* old_internal = nullptr;
  {
    MutexLock lock(GetCallableRefLock());
    old_internal = internal_;
    internal_ = nullptr;
  }
  delete old_internal;
}

Functions* HttpsCallableReference::functions() { return std::as_const(*this).functions(); }

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
Future<HttpsCallableResult> HttpsCallableReference::Call(const Variant& data) const {
  MutexLock lock(GetCallableRefLock());
  return internal_ ? internal_->Call(data) : Future<HttpsCallableResult>();
}
Future<HttpsCallableResult> HttpsCallableReference::CallLastResult() const {
  MutexLock lock(GetCallableRefLock());
  return internal_ ? internal_->CallLastResult() : Future<HttpsCallableResult>();
}
bool HttpsCallableReference::is_valid() const {
  MutexLock lock(GetCallableRefLock());
  return internal_ != nullptr && internal_->is_valid();
}
bool HttpsCallableReference::operator==(const HttpsCallableReference& other) const {
  MutexLock lock(GetCallableRefLock());
  if (internal_ == other.internal_) return true;
  if (!internal_ || !other.internal_) return false;
  return *internal_ == *other.internal_;
}
}
}
