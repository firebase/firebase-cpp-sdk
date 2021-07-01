// Copyright 2021 Google LLC

#import <Foundation/Foundation.h>

#import "FIRPerformance.h"
#import "FIRTrace.h"

#include "app/src/assert.h"
#include "performance/src/include/firebase/performance.h"
#import "performance/src/include/firebase/performance/trace.h"
#include "performance/src/performance_common.h"

namespace firebase {
namespace performance {

namespace internal {

// The Internal implementation of Trace as recommended by the pImpl design pattern.
// This class is thread safe as long as we can assume that raw ponter access is atomic on any of the
// platforms this will be used on.
class TraceInternal {
 public:
  explicit TraceInternal() { current_trace_ = nil; }

  ~TraceInternal() {
    if (current_trace_) {
      if (stop_on_destroy_) {
        StopTrace();
      } else {
        CancelTrace();
      }
    }
  }

  // Creates a Trace using the ObjC implementation. If this method is called before
  // stopping the previous trace, the previous trace is cancelled.
  void CreateTrace(const char* name, bool stop_on_destroy) {
    FIREBASE_ASSERT_RETURN_VOID(IsInitialized());

    stop_on_destroy_ = stop_on_destroy;

    if (WarnIf(name == nullptr, "Cannot create trace. Name cannot be null.")) return;

    FIRTrace* created_trace = [[FIRPerformance sharedInstance] traceWithName:@(name)];
    if (created_trace != nil) {
      current_trace_ = created_trace;
    }
  }

  void StartCreatedTrace() { [current_trace_ start]; }

  // Creates and Starts a Trace using the ObjC implementation. If this method is called before
  // stopping the previous trace, the previous trace is cancelled.
  void CreateAndStartTrace(const char* name) {
    CreateTrace(name, true);
    StartCreatedTrace();
  }

  // Stops the underlying ObjC trace if it has been started. Does nothing otherwise.
  void StopTrace() {
    FIREBASE_ASSERT_RETURN_VOID(IsInitialized());
    if (WarnIfNotCreated("Cannot stop Trace.")) return;

    [current_trace_ stop];
    current_trace_ = nil;
  }

  // Cancels the currently running trace if one exists, which prevents it from being logged to the
  // backend.
  void CancelTrace() {
    FIREBASE_ASSERT_RETURN_VOID(IsInitialized());
    if (WarnIfNotCreated("Cannot cancel Trace.")) return;

    current_trace_ = nil;
  }

  // Returns whether there is a trace that is currently active.
  bool IsTraceCreated() { return current_trace_ != nil; }

  // Sets a value for the given attribute for the given trace.
  void SetAttribute(const char* attribute_name, const char* attribute_value) {
    FIREBASE_ASSERT_RETURN_VOID(IsInitialized());
    if (WarnIf(attribute_name == nullptr, "Cannot SetAttribute for null attribute_name.")) return;
    if (WarnIfNotCreated("Cannot SetAttribute.")) return;

    if (attribute_value == nullptr) {
      [current_trace_ removeAttribute:@(attribute_name)];
    } else {
      [current_trace_ setValue:@(attribute_value) forAttribute:@(attribute_name)];
    }
  }

  // Gets the value of the custom attribute identified by the given
  // name or nullptr if it hasn't been set.
  std::string GetAttribute(const char* attribute_name) const {
    FIREBASE_ASSERT_RETURN("", IsInitialized());
    if (WarnIf(attribute_name == nullptr, "Cannot GetAttribute for null attribute_name."))
      return "";
    if (WarnIfNotCreated("Cannot GetAttribute.")) return "";

    NSString* attribute_value = [current_trace_ valueForAttribute:@(attribute_name)];
    if (attribute_value != nil) {
      return [attribute_value UTF8String];
    } else {
      return "";
    }
  }

  // Gets the value of the metric identified by the metric_name or 0 if it hasn't yet been set.
  int64_t GetLongMetric(const char* metric_name) const {
    FIREBASE_ASSERT_RETURN(0, IsInitialized());
    if (WarnIf(metric_name == nullptr, "Cannot GetLongMetric for null metric_name.")) return 0;
    if (WarnIfNotCreated("Cannot GetLongMetric.")) return 0;

    return [current_trace_ valueForIntMetric:@(metric_name)];
  }

  // Increments the existing value of the given metric by increment_by or sets
  // it to increment_by if the metric hasn't been set.
  void IncrementMetric(const char* metric_name, const int64_t increment_by) {
    FIREBASE_ASSERT_RETURN_VOID(IsInitialized());
    if (WarnIf(metric_name == nullptr, "Cannot IncrementMetric for null metric_name.")) return;
    if (WarnIfNotCreated("Cannot IncrementMetric.")) return;

    [current_trace_ incrementMetric:@(metric_name) byInt:increment_by];
  }

  // Sets the value of the given metric to metric_value.
  void SetMetric(const char* metric_name, const int64_t metric_value) {
    FIREBASE_ASSERT_RETURN_VOID(IsInitialized());
    if (WarnIf(metric_name == nullptr, "Cannot SetMetric for null metric_name.")) return;
    if (WarnIfNotCreated("Cannot SetMetric.")) return;

    [current_trace_ setIntValue:metric_value forMetric:@(metric_name)];
  }

 private:
  FIRTrace* current_trace_ = nil;

  // The unity implementation doesn't stop the underlying ObjC trace, whereas
  // the C++ implementation does. This flag is set when the ObjC trace is created
  // to track whether it should be stopped before deallocating the object.
  bool stop_on_destroy_ = false;

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
    if (condition) LogWarning(warning_message);
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

Trace::Trace(Trace&& other) {
  internal_ = other.internal_;
  other.internal_ = nullptr;
}

Trace& Trace::operator=(Trace&& other) {
  if (this != &other) {
    delete internal_;

    internal_ = other.internal_;
    other.internal_ = nullptr;
  }
  return *this;
}

bool Trace::is_started() {
  // In the C++ API we never allow a situation where an underlying Trace is created, but not
  // started, which is why this check is sufficient.
  return internal_->IsTraceCreated();
}

void Trace::Cancel() { internal_->CancelTrace(); }

void Trace::Stop() { internal_->StopTrace(); }

void Trace::Start(const char* name) {
  internal_->StopTrace();
  internal_->CreateAndStartTrace(name);
}

void Trace::SetAttribute(const char* attribute_name, const char* attribute_value) {
  internal_->SetAttribute(attribute_name, attribute_value);
}

std::string Trace::GetAttribute(const char* attribute_name) const {
  return internal_->GetAttribute(attribute_name);
}

int64_t Trace::GetLongMetric(const char* metric_name) const {
  return internal_->GetLongMetric(metric_name);
}

void Trace::IncrementMetric(const char* metric_name, const int64_t increment_by) {
  internal_->IncrementMetric(metric_name, increment_by);
}

void Trace::SetMetric(const char* metric_name, const int64_t metric_value) {
  internal_->SetMetric(metric_name, metric_value);
}

// Used only in Unity.

// We need to expose this functionality because the Unity implementation needs
// it. The Unity implementation (unlike the C++ implementation) can create a
// Trace without starting it, similar to the ObjC and Java APIs, which is
// why this is needed.

void Trace::Create(const char* name) { internal_->CreateTrace(name, false); }
void Trace::StartCreatedTrace() { internal_->StartCreatedTrace(); }

}  // namespace performance
}  // namespace firebase
