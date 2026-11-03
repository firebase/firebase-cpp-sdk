// Copyright 2021 Google LLC

#include "performance/src/include/firebase/performance/trace.h"

#include <cstdint>
#include <string>
#include <utility>

#include "app/src/assert.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "performance/src/android/performance_android_internal.h"
#include "performance/src/include/firebase/performance.h"
#include "performance/src/performance_common.h"

namespace firebase {
namespace performance {

namespace internal {

// The internal implementation of Trace using the pImpl design pattern.
// Access to the underlying JNI global reference and lifecycle state is
// synchronized with mutex_.
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
          env->CallVoidMethod(
              active_trace_,
              trace_jni::GetMethodId(trace_jni::kStopTrace));
          util::CheckAndClearJniExceptions(env);
        }
      } else {
        env = firebase::util::GetJNIEnvFromApp();
      }
      if (env) {
        env->DeleteGlobalRef(active_trace_);
      }
    }
    active_trace_ = nullptr;
    is_started_ = false;
  }

  // Creates a Trace using the Android implementation. If this method
  // is called before stopping the previous trace, the previous trace is
  // cancelled.
  bool CreateTrace(const char* name, bool stop_on_destroy) {
    FIREBASE_ASSERT_RETURN(false, IsInitialized());

    if (WarnIf(name == nullptr, "Cannot start trace. Name cannot be null.")) {
      return false;
    }

    MutexLock lock(mutex_);
    if (active_trace_ != nullptr) {
      CancelTrace();
    }

    stop_on_destroy_ = stop_on_destroy;
    is_started_ = false;

    JNIEnv* env = GetFirebaseApp()->GetJNIEnv();

    jstring name_jstring = env->NewStringUTF(name);

    jobject local_active_trace = env->CallObjectMethod(
        GetFirebasePerformanceClassInstance(),
        performance_jni::GetMethodId(performance_jni::kNewTrace), name_jstring);
    bool had_exception = util::CheckAndClearJniExceptions(env);

    if (!had_exception && local_active_trace != nullptr) {
      active_trace_ = env->NewGlobalRef(local_active_trace);
    }
    if (local_active_trace != nullptr) {
      env->DeleteLocalRef(local_active_trace);
    }
    if (name_jstring != nullptr) {
      env->DeleteLocalRef(name_jstring);
    }
    return active_trace_ != nullptr;
  }

  // Starts a created Trace using the Android implementation.
  void StartCreatedTrace() {
    FIREBASE_ASSERT_RETURN_VOID(IsInitialized());
    MutexLock lock(mutex_);
    if (WarnIfNotCreated("Cannot start Trace.")) return;
    if (!is_started_) {
      JNIEnv* env = GetFirebaseApp()->GetJNIEnv();
      env->CallVoidMethod(active_trace_,
                          trace_jni::GetMethodId(trace_jni::kStartTrace));
      if (!util::CheckAndClearJniExceptions(env)) {
        is_started_ = true;
      }
    }
  }

  // Creates and Starts a Trace using the Android implementation. If this
  // method is called before stopping the previous trace, the previous trace
  // is cancelled.
  void CreateAndStartTrace(const char* name) {
    MutexLock lock(mutex_);
    if (CreateTrace(name, true)) {
      StartCreatedTrace();
    }
  }

  // Stops the underlying Java trace if it has been started. Does nothing
  // otherwise.
  void StopTrace() {
    FIREBASE_ASSERT_RETURN_VOID(IsInitialized());
    MutexLock lock(mutex_);
    if (WarnIfNotCreated("Cannot stop Trace.")) return;

    JNIEnv* env = GetFirebaseApp()->GetJNIEnv();
    if (is_started_) {
      env->CallVoidMethod(active_trace_,
                          trace_jni::GetMethodId(trace_jni::kStopTrace));
      util::CheckAndClearJniExceptions(env);
    }
    env->DeleteGlobalRef(active_trace_);
    active_trace_ = nullptr;
    is_started_ = false;
  }

  // Cancels the currently running trace if one exists, which prevents it from
  // being logged to the backend.
  void CancelTrace() {
    FIREBASE_ASSERT_RETURN_VOID(IsInitialized());
    MutexLock lock(mutex_);
    if (WarnIfNotCreated("Cannot cancel Trace.")) return;

    JNIEnv* env = GetFirebaseApp()->GetJNIEnv();
    env->DeleteGlobalRef(active_trace_);
    active_trace_ = nullptr;
    is_started_ = false;
  }

  // Returns whether the trace has been started.
  bool IsStarted() const {
    MutexLock lock(mutex_);
    return active_trace_ != nullptr && is_started_;
  }

  // Sets a value for the given attribute for the given trace.
  void SetAttribute(const char* attribute_name, const char* attribute_value) {
    FIREBASE_ASSERT_RETURN_VOID(IsInitialized());
    if (WarnIf(attribute_name == nullptr,
               "Cannot SetAttribute for null attribute_name."))
      return;
    MutexLock lock(mutex_);
    if (WarnIfNotCreated("Cannot SetAttribute.")) return;

    JNIEnv* env = GetFirebaseApp()->GetJNIEnv();

    jstring attribute_name_jstring = env->NewStringUTF(attribute_name);

    if (attribute_value == nullptr) {
      env->CallVoidMethod(active_trace_,
                          trace_jni::GetMethodId(trace_jni::kRemoveAttribute),
                          attribute_name_jstring);
      util::CheckAndClearJniExceptions(env);
    } else {
      jstring attribute_value_jstring = env->NewStringUTF(attribute_value);
      env->CallVoidMethod(active_trace_,
                          trace_jni::GetMethodId(trace_jni::kSetAttribute),
                          attribute_name_jstring, attribute_value_jstring);
      util::CheckAndClearJniExceptions(env);
      env->DeleteLocalRef(attribute_value_jstring);
    }

    env->DeleteLocalRef(attribute_name_jstring);
  }

  // Gets the value of the custom attribute identified by the given
  // name or an empty string if it hasn't been set.
  std::string GetAttribute(const char* attribute_name) const {
    FIREBASE_ASSERT_RETURN("", IsInitialized());
    if (WarnIf(attribute_name == nullptr,
               "Cannot GetAttribute for null attribute_name."))
      return "";
    MutexLock lock(mutex_);
    if (WarnIfNotCreated("Cannot GetAttribute.")) return "";

    JNIEnv* env = GetFirebaseApp()->GetJNIEnv();
    jstring attribute_name_jstring = env->NewStringUTF(attribute_name);
    jobject attribute_value_jstring = env->CallObjectMethod(
        active_trace_, trace_jni::GetMethodId(trace_jni::kGetAttribute),
        attribute_name_jstring);
    bool exception_pending = util::CheckAndClearJniExceptions(env);
    env->DeleteLocalRef(attribute_name_jstring);

    if (exception_pending || attribute_value_jstring == nullptr) {
      if (attribute_value_jstring != nullptr) {
        env->DeleteLocalRef(attribute_value_jstring);
      }
      return "";
    }
    return util::JniStringToString(env, attribute_value_jstring);
  }

  // Gets the value of the metric identified by the metric_name or 0 if it
  // hasn't yet been set.
  int64_t GetLongMetric(const char* metric_name) const {
    FIREBASE_ASSERT_RETURN(0, IsInitialized());
    if (WarnIf(metric_name == nullptr,
               "Cannot GetLongMetric for null metric_name."))
      return 0;
    MutexLock lock(mutex_);
    if (WarnIfNotCreated("Cannot GetLongMetric.")) return 0;

    JNIEnv* env = GetFirebaseApp()->GetJNIEnv();
    jstring metric_name_jstring = env->NewStringUTF(metric_name);

    jlong metric_value = env->CallLongMethod(
        active_trace_, trace_jni::GetMethodId(trace_jni::kGetLongMetric),
        metric_name_jstring);
    bool exception_pending = util::CheckAndClearJniExceptions(env);
    env->DeleteLocalRef(metric_name_jstring);
    if (exception_pending) {
      return 0;
    }

    // jlong == int64_t, and so this is ok.
    return metric_value;
  }

  // Increments the existing value of the given metric by increment_by or sets
  // it to increment_by if the metric hasn't been set.
  void IncrementMetric(const char* metric_name, const int64_t increment_by) {
    FIREBASE_ASSERT_RETURN_VOID(IsInitialized());
    if (WarnIf(metric_name == nullptr,
               "Cannot IncrementMetric for null metric_name."))
      return;
    MutexLock lock(mutex_);
    if (WarnIfNotCreated("Cannot IncrementMetric.")) return;

    JNIEnv* env = GetFirebaseApp()->GetJNIEnv();

    jstring metric_name_jstring = env->NewStringUTF(metric_name);

    env->CallVoidMethod(active_trace_,
                        trace_jni::GetMethodId(trace_jni::kIncrementMetric),
                        metric_name_jstring, increment_by);

    util::CheckAndClearJniExceptions(env);
    env->DeleteLocalRef(metric_name_jstring);
  }

  // Sets the value of the given metric to metric_value.
  void SetMetric(const char* metric_name, const int64_t metric_value) {
    FIREBASE_ASSERT_RETURN_VOID(IsInitialized());
    if (WarnIf(metric_name == nullptr,
               "Cannot SetMetric for null metric_name."))
      return;
    MutexLock lock(mutex_);
    if (WarnIfNotCreated("Cannot SetMetric.")) return;

    JNIEnv* env = GetFirebaseApp()->GetJNIEnv();

    jstring metric_name_jstring = env->NewStringUTF(metric_name);

    env->CallVoidMethod(active_trace_,
                        trace_jni::GetMethodId(trace_jni::kPutMetric),
                        metric_name_jstring, metric_value);

    util::CheckAndClearJniExceptions(env);
    env->DeleteLocalRef(metric_name_jstring);
  }

 private:
  mutable Mutex mutex_;
  jobject active_trace_ = nullptr;

  // The unity implementation doesn't stop the underlying Java trace, whereas
  // the C++ implementation does. This flag is set when a Java trace is
  // created to track whether it should be stopped before deallocating the
  // object.
  bool stop_on_destroy_ = false;
  bool is_started_ = false;

  bool WarnIfNotCreated(const char* warning_message_details) const {
    if (!active_trace_) {
      LogWarning(
          "%s Trace is not active. Please create a new "
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

void Trace::SetAttribute(const char* attribute_name,
                         const char* attribute_value) {
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

void Trace::IncrementMetric(const char* metric_name,
                            const int64_t increment_by) {
  if (!internal_) return;
  internal_->IncrementMetric(metric_name, increment_by);
}

void Trace::SetMetric(const char* metric_name, const int64_t metric_value) {
  if (!internal_) return;
  internal_->SetMetric(metric_name, metric_value);
}

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
