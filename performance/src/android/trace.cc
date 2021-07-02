// Copyright 2021 Google LLC

#import "performance/src/include/firebase/performance/trace.h"

#include "app/src/assert.h"
#include "performance/src/android/performance_android_internal.h"
#include "performance/src/include/firebase/performance.h"
#include "performance/src/performance_common.h"

namespace firebase {
namespace performance {

namespace internal {

// The Internal implementation of Trace as recommended by the pImpl design
// pattern. This class is thread safe as long as we can assume that raw ponter
// access is atomic on any of the platforms this will be used on.
class TraceInternal {
 public:
  explicit TraceInternal() {}

  ~TraceInternal() {
    if (active_trace_) {
      if (stop_on_destroy_) {
        StopTrace();
      } else {
        CancelTrace();
      }
    }
  }

  // Creates a Trace using the Android implementation. If this method
  // is called before stopping the previous trace, the previous trace is
  // cancelled.
  void CreateTrace(const char* name, bool stop_on_destroy) {
    FIREBASE_ASSERT_RETURN_VOID(IsInitialized());

    stop_on_destroy_ = stop_on_destroy;

    if (WarnIf(name == nullptr, "Cannot start trace. Name cannot be null."))
      return;

    JNIEnv* env = GetFirebaseApp()->GetJNIEnv();

    if (active_trace_ != nullptr) {
      CancelTrace();
    }

    jstring name_jstring = env->NewStringUTF(name);

    jobject local_active_trace = env->CallObjectMethod(
        GetFirebasePerformanceClassInstance(),
        performance_jni::GetMethodId(performance_jni::kNewTrace), name_jstring);
    util::CheckAndClearJniExceptions(env);

    active_trace_ = env->NewGlobalRef(local_active_trace);
    env->DeleteLocalRef(local_active_trace);
    env->DeleteLocalRef(name_jstring);
  }

  // Creates and Starts a Trace using the Android implementation. If this
  // method is called before stopping the previous trace, the previous trace
  // is cancelled.
  void StartCreatedTrace() {
    if (active_trace_) {
      JNIEnv* env = GetFirebaseApp()->GetJNIEnv();
      env->CallVoidMethod(active_trace_,
                          trace_jni::GetMethodId(trace_jni::kStartTrace));
      util::CheckAndClearJniExceptions(env);
    }
  }

  // Creates and Starts a Trace using the Android implementation. If this
  // method is called before stopping the previous trace, the previous trace
  // is cancelled.
  void CreateAndStartTrace(const char* name) {
    CreateTrace(name, true);
    StartCreatedTrace();
  }

  // Stops the underlying Java trace if it has been started. Does nothing
  // otherwise.
  void StopTrace() {
    FIREBASE_ASSERT_RETURN_VOID(IsInitialized());
    if (WarnIfNotCreated("Cannot stop Trace.")) return;

    JNIEnv* env = GetFirebaseApp()->GetJNIEnv();
    env->CallVoidMethod(active_trace_,
                        trace_jni::GetMethodId(trace_jni::kStopTrace));
    util::CheckAndClearJniExceptions(env);
    env->DeleteGlobalRef(active_trace_);
    active_trace_ = nullptr;
  }

  // Cancels the currently running trace if one exists, which prevents it from
  // being logged to the backend.
  void CancelTrace() {
    FIREBASE_ASSERT_RETURN_VOID(IsInitialized());
    if (WarnIfNotCreated("Cannot cancel Trace.")) return;

    JNIEnv* env = GetFirebaseApp()->GetJNIEnv();
    env->DeleteGlobalRef(active_trace_);
    active_trace_ = nullptr;
  }

  // Returns whether there is a trace that is currently created.
  bool IsTraceCreated() { return active_trace_ != nullptr; }

  // Sets a value for the given attribute for the given trace.
  void SetAttribute(const char* attribute_name, const char* attribute_value) {
    FIREBASE_ASSERT_RETURN_VOID(IsInitialized());
    if (WarnIf(attribute_name == nullptr,
               "Cannot SetAttribute for null attribute_name."))
      return;
    if (WarnIfNotCreated("Cannot SetAttribute.")) return;

    JNIEnv* env = GetFirebaseApp()->GetJNIEnv();

    jstring attribute_name_jstring = env->NewStringUTF(attribute_name);

    if (attribute_value == nullptr) {
      env->CallVoidMethod(active_trace_,
                          trace_jni::GetMethodId(trace_jni::kRemoveAttribute),
                          attribute_name_jstring);
    } else {
      jstring attribute_value_jstring = env->NewStringUTF(attribute_value);
      env->CallVoidMethod(active_trace_,
                          trace_jni::GetMethodId(trace_jni::kSetAttribute),
                          attribute_name_jstring, attribute_value_jstring);
      env->DeleteLocalRef(attribute_value_jstring);
    }

    util::CheckAndClearJniExceptions(env);
    env->DeleteLocalRef(attribute_name_jstring);
  }

  // Gets the value of the custom attribute identified by the given
  // name or nullptr if it hasn't been set.
  std::string GetAttribute(const char* attribute_name) const {
    FIREBASE_ASSERT_RETURN("", IsInitialized());
    if (WarnIf(attribute_name == nullptr,
               "Cannot GetAttribute for null attribute_name."))
      return "";
    if (WarnIfNotCreated("Cannot GetAttribute.")) return "";

    JNIEnv* env = GetFirebaseApp()->GetJNIEnv();
    jstring attribute_name_jstring =
        attribute_name ? env->NewStringUTF(attribute_name) : nullptr;
    jobject attribute_value_jstring = env->CallObjectMethod(
        active_trace_, trace_jni::GetMethodId(trace_jni::kGetAttribute),
        attribute_name_jstring);
    util::CheckAndClearJniExceptions(env);
    env->DeleteLocalRef(attribute_name_jstring);

    if (attribute_value_jstring == nullptr) {
      return "";
    } else {
      return util::JniStringToString(env, attribute_value_jstring);
    }
  }

  // Gets the value of the metric identified by the metric_name or 0 if it
  // hasn't yet been set.
  int64_t GetLongMetric(const char* metric_name) const {
    FIREBASE_ASSERT_RETURN(0, IsInitialized());
    if (WarnIf(metric_name == nullptr,
               "Cannot GetLongMetric for null metric_name."))
      return 0;
    if (WarnIfNotCreated("Cannot GetLongMetric.")) return 0;

    JNIEnv* env = GetFirebaseApp()->GetJNIEnv();
    jstring metric_name_jstring =
        metric_name ? env->NewStringUTF(metric_name) : nullptr;

    jlong metric_value = env->CallLongMethod(
        active_trace_, trace_jni::GetMethodId(trace_jni::kGetLongMetric),
        metric_name_jstring);
    env->DeleteLocalRef(metric_name_jstring);
    util::CheckAndClearJniExceptions(env);

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
  jobject active_trace_ = nullptr;

  // The unity implementation doesn't stop the underlying Java trace, whereas
  // the C++ implementation does. This flag is set when a Java trace is
  // created to track whether it should be stopped before deallocating the
  // object.
  bool stop_on_destroy_ = false;

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
  // In the C++ API we never allow a situation where an underlying HttpMetric
  // is created, but not started, which is why this check is sufficient.
  return internal_->IsTraceCreated();
}

void Trace::Cancel() { internal_->CancelTrace(); }

void Trace::Stop() { internal_->StopTrace(); }

void Trace::Start(const char* name) {
  internal_->StopTrace();
  internal_->CreateAndStartTrace(name);
}

void Trace::SetAttribute(const char* attribute_name,
                         const char* attribute_value) {
  internal_->SetAttribute(attribute_name, attribute_value);
}

std::string Trace::GetAttribute(const char* attribute_name) const {
  return internal_->GetAttribute(attribute_name);
}

int64_t Trace::GetLongMetric(const char* metric_name) const {
  return internal_->GetLongMetric(metric_name);
}

void Trace::IncrementMetric(const char* metric_name,
                            const int64_t increment_by) {
  internal_->IncrementMetric(metric_name, increment_by);
}

void Trace::SetMetric(const char* metric_name, const int64_t metric_value) {
  internal_->SetMetric(metric_name, metric_value);
}

void Trace::Create(const char* name) { internal_->CreateTrace(name, false); }

void Trace::StartCreatedTrace() { internal_->StartCreatedTrace(); }

}  // namespace performance
}  // namespace firebase
