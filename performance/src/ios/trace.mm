// Copyright 2021 Google LLC

#include "performance/src/include/firebase/performance/trace.h"

#import <Foundation/Foundation.h>

#import "FIRPerformance.h"
#import "FIRTrace.h"

#include <cstdint>
#include <string>
#include <utility>

#include "app/src/assert.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "performance/src/include/firebase/performance.h"
#include "performance/src/performance_common.h"

namespace firebase {
namespace performance {

namespace internal {

// The internal implementation of Trace using the pImpl design pattern.
// Access to the underlying Objective-C reference and lifecycle state is
// synchronized with mutex_.
class TraceInternal {
 public:
  TraceInternal() = default;
  TraceInternal(const TraceInternal&) = delete;
  TraceInternal& operator=(const TraceInternal&) = delete;

  ~TraceInternal() {
    MutexLock lock(mutex_);
    if (IsInitialized() && current_trace_) {
      if (stop_on_destroy_ && is_started_) {
        StopTrace();
      } else {
        CancelTrace();
      }
    }
    current_trace_ = nil;
    is_started_ = false;
  }

  // Creates a Trace using the ObjC implementation. If this method is called before
  // stopping the previous trace, the previous trace is cancelled.
  bool CreateTrace(const char* name, bool stop_on_destroy) {
    FIREBASE_ASSERT_RETURN(false, IsInitialized());

    if (WarnIf(name == nullptr, "Cannot create trace. Name cannot be null.")) return false;
    NSString* name_ns = [NSString stringWithUTF8String:name];
    if (WarnIf(name_ns == nil, "Cannot create trace. Name is not valid UTF-8.")) return false;

    MutexLock lock(mutex_);
    if (current_trace_ != nil) {
      CancelTrace();
    }

    stop_on_destroy_ = stop_on_destroy;
    is_started_ = false;

    FIRTrace* created_trace = [[FIRPerformance sharedInstance] traceWithName:name_ns];
    current_trace_ = created_trace;
    return current_trace_ != nil;
  }

  void StartCreatedTrace() {
    FIREBASE_ASSERT_RETURN_VOID(IsInitialized());
    MutexLock lock(mutex_);
    if (WarnIfNotCreated("Cannot start Trace.")) return;
    if (!is_started_) {
      [current_trace_ start];
      is_started_ = true;
    }
  }

  // Creates and Starts a Trace using the ObjC implementation. If this method is called before
  // stopping the previous trace, the previous trace is cancelled.
  void CreateAndStartTrace(const char* name) {
    MutexLock lock(mutex_);
    if (CreateTrace(name, true)) {
      StartCreatedTrace();
    }
  }

  // Stops the underlying ObjC trace if it has been started. Does nothing otherwise.
  void StopTrace() {
    FIREBASE_ASSERT_RETURN_VOID(IsInitialized());
    MutexLock lock(mutex_);
    if (WarnIfNotCreated("Cannot stop Trace.")) return;

    if (is_started_) {
      [current_trace_ stop];
    }
    current_trace_ = nil;
    is_started_ = false;
  }

  // Cancels the currently running trace if one exists, which prevents it from being logged to the
  // backend.
  void CancelTrace() {
    FIREBASE_ASSERT_RETURN_VOID(IsInitialized());
    MutexLock lock(mutex_);
    if (WarnIfNotCreated("Cannot cancel Trace.")) return;

    current_trace_ = nil;
    is_started_ = false;
  }

  // Returns whether the trace is currently started.
  bool IsStarted() const {
    MutexLock lock(mutex_);
    return current_trace_ != nil && is_started_;
  }

  // Sets a value for the given attribute for the given trace.
  void SetAttribute(const char* attribute_name, const char* attribute_value) {
    FIREBASE_ASSERT_RETURN_VOID(IsInitialized());
    if (WarnIf(attribute_name == nullptr, "Cannot SetAttribute for null attribute_name.")) return;
    MutexLock lock(mutex_);
    if (WarnIfNotCreated("Cannot SetAttribute.")) return;
    NSString* attr_name_ns = [NSString stringWithUTF8String:attribute_name];
    if (WarnIf(attr_name_ns == nil, "attribute_name is not valid UTF-8.")) return;

    if (attribute_value == nullptr) {
      [current_trace_ removeAttribute:attr_name_ns];
    } else {
      NSString* attr_val_ns = [NSString stringWithUTF8String:attribute_value];
      if (WarnIf(attr_val_ns == nil, "attribute_value is not valid UTF-8.")) return;
      [current_trace_ setValue:attr_val_ns forAttribute:attr_name_ns];
    }
  }

  // Gets the value of the custom attribute identified by the given
  // name or an empty string if it hasn't been set.
  std::string GetAttribute(const char* attribute_name) const {
    FIREBASE_ASSERT_RETURN("", IsInitialized());
    if (WarnIf(attribute_name == nullptr, "Cannot GetAttribute for null attribute_name."))
      return "";
    MutexLock lock(mutex_);
    if (WarnIfNotCreated("Cannot GetAttribute.")) return "";
    NSString* attr_name_ns = [NSString stringWithUTF8String:attribute_name];
    if (WarnIf(attr_name_ns == nil, "attribute_name is not valid UTF-8.")) return "";

    NSString* attribute_value = [current_trace_ valueForAttribute:attr_name_ns];
    const char* utf8_value = attribute_value != nil ? [attribute_value UTF8String] : nullptr;
    return utf8_value != nullptr ? std::string(utf8_value) : "";
  }

  // Gets the value of the metric identified by the metric_name or 0 if it hasn't yet been set.
  int64_t GetLongMetric(const char* metric_name) const {
    FIREBASE_ASSERT_RETURN(0, IsInitialized());
    if (WarnIf(metric_name == nullptr, "Cannot GetLongMetric for null metric_name.")) return 0;
    MutexLock lock(mutex_);
    if (WarnIfNotCreated("Cannot GetLongMetric.")) return 0;
    NSString* metric_name_ns = [NSString stringWithUTF8String:metric_name];
    if (WarnIf(metric_name_ns == nil, "metric_name is not valid UTF-8.")) return 0;

    return [current_trace_ valueForIntMetric:metric_name_ns];
  }

  // Increments the existing value of the given metric by increment_by or sets
  // it to increment_by if the metric hasn't been set.
  void IncrementMetric(const char* metric_name, const int64_t increment_by) {
    FIREBASE_ASSERT_RETURN_VOID(IsInitialized());
    if (WarnIf(metric_name == nullptr, "Cannot IncrementMetric for null metric_name.")) return;
    MutexLock lock(mutex_);
    if (WarnIfNotCreated("Cannot IncrementMetric.")) return;
    NSString* metric_name_ns = [NSString stringWithUTF8String:metric_name];
    if (WarnIf(metric_name_ns == nil, "metric_name is not valid UTF-8.")) return;

    [current_trace_ incrementMetric:metric_name_ns byInt:increment_by];
  }

  // Sets the value of the given metric to metric_value.
  void SetMetric(const char* metric_name, const int64_t metric_value) {
    FIREBASE_ASSERT_RETURN_VOID(IsInitialized());
    if (WarnIf(metric_name == nullptr, "Cannot SetMetric for null metric_name.")) return;
    MutexLock lock(mutex_);
    if (WarnIfNotCreated("Cannot SetMetric.")) return;
    NSString* metric_name_ns = [NSString stringWithUTF8String:metric_name];
    if (WarnIf(metric_name_ns == nil, "metric_name is not valid UTF-8.")) return;

    [current_trace_ setIntValue:metric_value forMetric:metric_name_ns];
  }

 private:
  mutable Mutex mutex_;
  FIRTrace* current_trace_ = nil;

  // The unity implementation doesn't stop the underlying ObjC trace, whereas
  // the C++ implementation does. This flag is set when the ObjC trace is created
  // to track whether it should be stopped before deallocating the object.
  bool stop_on_destroy_ = false;
  bool is_started_ = false;

  bool WarnIfNotCreated(const char* warning_message_details) const {
    if (!current_trace_) {
      LogWarning("%s Trace is not active. Please create a new "
                 "Trace.",
                 warning_message_details);
      return true;
    }
    return false;
  }

  bool WarnIf(bool condition, const char* warning_message) const {
    if (condition) LogWarning("%s", warning_message);
    return condition;
  }
};

}  // namespace internal

Trace::Trace() {
  FIREBASE_ASSERT(internal::IsInitialized());
  internal_ = new internal::TraceInternal();
}

Trace::Trace(const char* name) {
  FIREBASE_ASSERT(internal::IsInitialized());
  internal_ = new internal::TraceInternal();
  internal_->CreateAndStartTrace(name);
}

Trace::~Trace() { delete internal_; }

Trace::Trace(Trace&& other) noexcept
    : internal_(std::exchange(other.internal_, nullptr)) {}

Trace& Trace::operator=(Trace&& other) noexcept {
  if (this != &other) {
    delete internal_;
    internal_ = std::exchange(other.internal_, nullptr);
  }
  return *this;
}

bool Trace::is_started() const {
  return internal_ != nullptr && internal_->IsStarted();
}

void Trace::Cancel() {
  if (!internal_) return;
  internal_->CancelTrace();
}

void Trace::Stop() {
  if (!internal_) return;
  internal_->StopTrace();
}

void Trace::Start(const char* name) {
  if (!internal_) {
    internal_ = new internal::TraceInternal();
  }
  internal_->CreateAndStartTrace(name);
}

void Trace::SetAttribute(const char* attribute_name, const char* attribute_value) {
  if (!internal_) return;
  internal_->SetAttribute(attribute_name, attribute_value);
}

std::string Trace::GetAttribute(const char* attribute_name) const {
  if (!internal_) return "";
  return internal_->GetAttribute(attribute_name);
}

int64_t Trace::GetLongMetric(const char* metric_name) const {
  if (!internal_) return 0;
  return internal_->GetLongMetric(metric_name);
}

void Trace::IncrementMetric(const char* metric_name, const int64_t increment_by) {
  if (!internal_) return;
  internal_->IncrementMetric(metric_name, increment_by);
}

void Trace::SetMetric(const char* metric_name, const int64_t metric_value) {
  if (!internal_) return;
  internal_->SetMetric(metric_name, metric_value);
}

// Used only in Unity.

// We need to expose this functionality because the Unity implementation needs
// it. The Unity implementation (unlike the C++ implementation) can create a
// Trace without starting it, similar to the ObjC and Java APIs, which is
// why this is needed.

void Trace::Create(const char* name) {
  if (!internal_) {
    internal_ = new internal::TraceInternal();
  }
  internal_->CreateTrace(name, false);
}
void Trace::StartCreatedTrace() {
  if (!internal_) return;
  internal_->StartCreatedTrace();
}

}  // namespace performance
}  // namespace firebase
