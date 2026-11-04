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

class TraceInternal {
 public:
  TraceInternal() = default;
  TraceInternal(const TraceInternal&) = delete;
  TraceInternal& operator=(const TraceInternal&) = delete;

  ~TraceInternal() {
    MutexLock lock(mutex_);
    if (IsInitialized() && current_trace_) {
      if (stop_on_destroy_ && is_started_)
        StopTrace();
      else
        CancelTrace();
    }
    current_trace_ = nil;
    is_started_ = false;
  }

  bool CreateTrace(const char* name, bool stop_on_destroy) {
    FIREBASE_ASSERT_RETURN(false, IsInitialized());
    if (WarnIf(!name, "Cannot create trace. Name cannot be null."))
      return false;
    NSString* name_ns = [NSString stringWithUTF8String:name];
    if (WarnIf(!name_ns, "Cannot create trace. Name is not valid UTF-8."))
      return false;
    MutexLock lock(mutex_);
    if (current_trace_ != nil) CancelTrace();
    stop_on_destroy_ = stop_on_destroy;
    is_started_ = false;
    current_trace_ = [[FIRPerformance sharedInstance] traceWithName:name_ns];
    return current_trace_ != nil;
  }

  void StartCreatedTrace() {
    FIREBASE_ASSERT_RETURN_VOID(IsInitialized());
    MutexLock lock(mutex_);
    if (!WarnIfNotCreated("Cannot start Trace.") && !is_started_) {
      [current_trace_ start];
      is_started_ = true;
    }
  }

  void CreateAndStartTrace(const char* name) {
    MutexLock lock(mutex_);
    if (CreateTrace(name, true)) StartCreatedTrace();
  }

  void StopTrace() {
    FIREBASE_ASSERT_RETURN_VOID(IsInitialized());
    MutexLock lock(mutex_);
    if (WarnIfNotCreated("Cannot stop Trace.")) return;
    if (is_started_) [current_trace_ stop];
    current_trace_ = nil;
    is_started_ = false;
  }

  void CancelTrace() {
    FIREBASE_ASSERT_RETURN_VOID(IsInitialized());
    MutexLock lock(mutex_);
    if (!WarnIfNotCreated("Cannot cancel Trace.")) {
      current_trace_ = nil;
      is_started_ = false;
    }
  }

  bool IsStarted() const {
    MutexLock lock(mutex_);
    return current_trace_ != nil && is_started_;
  }

  void SetAttribute(const char* name, const char* value) {
    FIREBASE_ASSERT_RETURN_VOID(IsInitialized());
    if (WarnIf(!name, "Cannot SetAttribute for null attribute_name.")) return;
    MutexLock lock(mutex_);
    if (WarnIfNotCreated("Cannot SetAttribute.")) return;
    NSString* k = [NSString stringWithUTF8String:name];
    if (WarnIf(!k, "attribute_name is not valid UTF-8.")) return;
    if (!value) {
      [current_trace_ removeAttribute:k];
    } else if (NSString* v = [NSString stringWithUTF8String:value]) {
      [current_trace_ setValue:v forAttribute:k];
    } else {
      LogWarning("attribute_value is not valid UTF-8.");
    }
  }

  std::string GetAttribute(const char* name) const {
    FIREBASE_ASSERT_RETURN("", IsInitialized());
    if (WarnIf(!name, "Cannot GetAttribute for null attribute_name."))
      return "";
    MutexLock lock(mutex_);
    if (WarnIfNotCreated("Cannot GetAttribute.")) return "";
    NSString* k = [NSString stringWithUTF8String:name];
    if (WarnIf(!k, "attribute_name is not valid UTF-8.")) return "";
    NSString* v = [current_trace_ valueForAttribute:k];
    const char* s = v ? [v UTF8String] : nullptr;
    return s ? std::string(s) : "";
  }

  int64_t GetLongMetric(const char* metric_name) const {
    FIREBASE_ASSERT_RETURN(0, IsInitialized());
    if (WarnIf(!metric_name, "Cannot GetLongMetric for null metric_name."))
      return 0;
    MutexLock lock(mutex_);
    if (WarnIfNotCreated("Cannot GetLongMetric.")) return 0;
    NSString* k = [NSString stringWithUTF8String:metric_name];
    return WarnIf(!k, "metric_name is not valid UTF-8.")
               ? 0
               : [current_trace_ valueForIntMetric:k];
  }

  void IncrementMetric(const char* metric_name, const int64_t increment_by) {
    FIREBASE_ASSERT_RETURN_VOID(IsInitialized());
    if (WarnIf(!metric_name, "Cannot IncrementMetric for null metric_name."))
      return;
    MutexLock lock(mutex_);
    if (WarnIfNotCreated("Cannot IncrementMetric.")) return;
    if (NSString* k = [NSString stringWithUTF8String:metric_name]) {
      [current_trace_ incrementMetric:k byInt:increment_by];
    } else {
      LogWarning("metric_name is not valid UTF-8.");
    }
  }

  void SetMetric(const char* metric_name, const int64_t metric_value) {
    FIREBASE_ASSERT_RETURN_VOID(IsInitialized());
    if (WarnIf(!metric_name, "Cannot SetMetric for null metric_name.")) return;
    MutexLock lock(mutex_);
    if (WarnIfNotCreated("Cannot SetMetric.")) return;
    if (NSString* k = [NSString stringWithUTF8String:metric_name]) {
      [current_trace_ setIntValue:metric_value forMetric:k];
    } else {
      LogWarning("metric_name is not valid UTF-8.");
    }
  }

 private:
  mutable Mutex mutex_;
  FIRTrace* current_trace_ = nil;
  bool stop_on_destroy_ = false;
  bool is_started_ = false;

  bool WarnIfNotCreated(const char* msg) const {
    if (!current_trace_) {
      LogWarning("%s Trace is not active. Please create a new Trace.", msg);
      return true;
    }
    return false;
  }

  static bool WarnIf(bool cond, const char* msg) {
    if (cond) LogWarning("%s", msg);
    return cond;
  }
};

}  // namespace internal

Trace::Trace() : internal_(new internal::TraceInternal()) {
  FIREBASE_ASSERT(internal::IsInitialized());
}
Trace::Trace(const char* name) : Trace() {
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
bool Trace::is_started() const { return internal_ && internal_->IsStarted(); }
void Trace::Cancel() {
  if (internal_) internal_->CancelTrace();
}
void Trace::Stop() {
  if (internal_) internal_->StopTrace();
}
void Trace::Start(const char* name) {
  if (!internal_) internal_ = new internal::TraceInternal();
  internal_->CreateAndStartTrace(name);
}
void Trace::SetAttribute(const char* name, const char* value) {
  if (internal_) internal_->SetAttribute(name, value);
}
std::string Trace::GetAttribute(const char* name) const {
  return internal_ ? internal_->GetAttribute(name) : "";
}
int64_t Trace::GetLongMetric(const char* metric_name) const {
  return internal_ ? internal_->GetLongMetric(metric_name) : 0;
}
void Trace::IncrementMetric(const char* metric_name,
                            const int64_t increment_by) {
  if (internal_) internal_->IncrementMetric(metric_name, increment_by);
}
void Trace::SetMetric(const char* metric_name, const int64_t metric_value) {
  if (internal_) internal_->SetMetric(metric_name, metric_value);
}
void Trace::Create(const char* name) {
  if (!internal_) internal_ = new internal::TraceInternal();
  internal_->CreateTrace(name, false);
}
void Trace::StartCreatedTrace() {
  if (internal_) internal_->StartCreatedTrace();
}

}  // namespace performance
}  // namespace firebase
