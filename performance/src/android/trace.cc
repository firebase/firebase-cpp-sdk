// Copyright 2021 Google LLC

#include "performance/src/include/firebase/performance/trace.h"

#include <cstdint>
#include <string>
#include <utility>

#include "app/src/app_android.h"
#include "app/src/assert.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/jni/exception.h"
#include "app/src/jni/string.h"
#include "performance/src/android/performance_android_internal.h"
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
    if (active_trace_) {
      JNIEnv* env = nullptr;
      if (IsInitialized()) {
        env = GetFirebaseApp()->GetJNIEnv();
        if (stop_on_destroy_ && is_started_) {
          env->CallVoidMethod(active_trace_,
                              trace_jni::GetMethodId(trace_jni::kStopTrace));
          jni::CheckAndClearException(env);
        }
      } else if (App::GetInstance()) {
        env = App::GetInstance()->GetJNIEnv();
      }
      if (env) env->DeleteGlobalRef(active_trace_);
    }
    active_trace_ = nullptr;
    is_started_ = false;
  }

  bool CreateTrace(const char* name, bool stop_on_destroy) {
    FIREBASE_ASSERT_RETURN(false, IsInitialized());
    if (WarnIf(!name, "Cannot start trace. Name cannot be null.")) return false;
    MutexLock lock(mutex_);
    return CreateTraceLocked(name, stop_on_destroy);
  }

  void StartCreatedTrace() {
    FIREBASE_ASSERT_RETURN_VOID(IsInitialized());
    MutexLock lock(mutex_);
    StartCreatedTraceLocked();
  }

  void CreateAndStartTrace(const char* name) {
    FIREBASE_ASSERT_RETURN_VOID(IsInitialized());
    if (WarnIf(!name, "Cannot start trace. Name cannot be null.")) return;
    MutexLock lock(mutex_);
    if (CreateTraceLocked(name, true)) StartCreatedTraceLocked();
  }

  void StopTrace() {
    FIREBASE_ASSERT_RETURN_VOID(IsInitialized());
    MutexLock lock(mutex_);
    if (WarnIfNotCreated("Cannot stop Trace.")) return;
    if (is_started_) {
      JNIEnv* env = GetFirebaseApp()->GetJNIEnv();
      env->CallVoidMethod(active_trace_,
                          trace_jni::GetMethodId(trace_jni::kStopTrace));
      jni::CheckAndClearException(env);
    }
    CancelTraceLocked();
  }

  void CancelTrace() {
    FIREBASE_ASSERT_RETURN_VOID(IsInitialized());
    MutexLock lock(mutex_);
    if (!WarnIfNotCreated("Cannot cancel Trace.")) CancelTraceLocked();
  }

  bool IsStarted() const {
    MutexLock lock(mutex_);
    return active_trace_ != nullptr && is_started_;
  }

  void SetAttribute(const char* name, const char* value) {
    FIREBASE_ASSERT_RETURN_VOID(IsInitialized());
    if (WarnIf(!name, "Cannot SetAttribute for null attribute_name.")) return;
    MutexLock lock(mutex_);
    if (WarnIfNotCreated("Cannot SetAttribute.")) return;
    JNIEnv* env = GetFirebaseApp()->GetJNIEnv();
    jstring jname = env->NewStringUTF(name);
    if (!value) {
      env->CallVoidMethod(active_trace_,
                          trace_jni::GetMethodId(trace_jni::kRemoveAttribute),
                          jname);
    } else {
      jstring jval = env->NewStringUTF(value);
      env->CallVoidMethod(active_trace_,
                          trace_jni::GetMethodId(trace_jni::kSetAttribute),
                          jname, jval);
      env->DeleteLocalRef(jval);
    }
    jni::CheckAndClearException(env);
    env->DeleteLocalRef(jname);
  }

  std::string GetAttribute(const char* name) const {
    FIREBASE_ASSERT_RETURN("", IsInitialized());
    if (WarnIf(!name, "Cannot GetAttribute for null attribute_name."))
      return "";
    MutexLock lock(mutex_);
    if (WarnIfNotCreated("Cannot GetAttribute.")) return "";
    JNIEnv* env = GetFirebaseApp()->GetJNIEnv();
    jstring jname = env->NewStringUTF(name);
    auto jval = static_cast<jstring>(env->CallObjectMethod(
        active_trace_, trace_jni::GetMethodId(trace_jni::kGetAttribute),
        jname));
    bool exc = jni::CheckAndClearException(env);
    env->DeleteLocalRef(jname);
    std::string res = (!exc && jval) ? jni::JStringToString(env, jval) : "";
    if (jval) env->DeleteLocalRef(jval);
    return res;
  }

  int64_t GetLongMetric(const char* metric_name) const {
    FIREBASE_ASSERT_RETURN(0, IsInitialized());
    if (WarnIf(!metric_name, "Cannot GetLongMetric for null metric_name."))
      return 0;
    MutexLock lock(mutex_);
    if (WarnIfNotCreated("Cannot GetLongMetric.")) return 0;
    JNIEnv* env = GetFirebaseApp()->GetJNIEnv();
    jstring jname = env->NewStringUTF(metric_name);
    jlong val = env->CallLongMethod(
        active_trace_, trace_jni::GetMethodId(trace_jni::kGetLongMetric),
        jname);
    bool exc = jni::CheckAndClearException(env);
    env->DeleteLocalRef(jname);
    return exc ? 0 : val;
  }

  void IncrementMetric(const char* metric_name, const int64_t increment_by) {
    MutateMetric(trace_jni::kIncrementMetric, metric_name, increment_by,
                 "IncrementMetric");
  }

  void SetMetric(const char* metric_name, const int64_t metric_value) {
    MutateMetric(trace_jni::kPutMetric, metric_name, metric_value, "SetMetric");
  }

 private:
  mutable Mutex mutex_;
  jobject active_trace_ = nullptr;
  bool stop_on_destroy_ = false;
  bool is_started_ = false;

  void MutateMetric(trace_jni::Method m, const char* name, int64_t val,
                    const char* op) {
    FIREBASE_ASSERT_RETURN_VOID(IsInitialized());
    if (!name) {
      LogWarning("Cannot %s for null metric_name.", op);
      return;
    }
    MutexLock lock(mutex_);
    if (!active_trace_) {
      LogWarning("Cannot %s. Trace is not active. Please create a new Trace.",
                 op);
      return;
    }
    JNIEnv* env = GetFirebaseApp()->GetJNIEnv();
    jstring jname = env->NewStringUTF(name);
    env->CallVoidMethod(active_trace_, trace_jni::GetMethodId(m), jname, val);
    jni::CheckAndClearException(env);
    env->DeleteLocalRef(jname);
  }

  bool CreateTraceLocked(const char* name, bool stop_on_destroy) {
    if (active_trace_) CancelTraceLocked();
    stop_on_destroy_ = stop_on_destroy;
    is_started_ = false;
    JNIEnv* env = GetFirebaseApp()->GetJNIEnv();
    jstring jname = env->NewStringUTF(name);
    jobject local = env->CallObjectMethod(
        GetFirebasePerformanceClassInstance(),
        performance_jni::GetMethodId(performance_jni::kNewTrace), jname);
    if (!jni::CheckAndClearException(env) && local) {
      active_trace_ = env->NewGlobalRef(local);
    }
    if (local) env->DeleteLocalRef(local);
    if (jname) env->DeleteLocalRef(jname);
    return active_trace_ != nullptr;
  }

  void StartCreatedTraceLocked() {
    if (WarnIfNotCreated("Cannot start Trace.") || is_started_) return;
    JNIEnv* env = GetFirebaseApp()->GetJNIEnv();
    env->CallVoidMethod(active_trace_,
                        trace_jni::GetMethodId(trace_jni::kStartTrace));
    if (!jni::CheckAndClearException(env)) is_started_ = true;
  }

  void CancelTraceLocked() {
    GetFirebaseApp()->GetJNIEnv()->DeleteGlobalRef(active_trace_);
    active_trace_ = nullptr;
    is_started_ = false;
  }

  bool WarnIfNotCreated(const char* msg) const {
    if (!active_trace_) {
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
