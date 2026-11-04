// Copyright 2021 Google LLC

#include "performance/src/include/firebase/performance/http_metric.h"

#include <jni.h>
#include <cstdint>
#include <string>
#include <utility>

#include "app/src/app_android.h"
#include "app/src/assert.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/jni/exception.h"
#include "app/src/jni/string.h"
#include "app/src/log.h"
#include "app/src/util_android.h"
#include "performance/src/android/performance_android_internal.h"
#include "performance/src/include/firebase/performance.h"
#include "performance/src/performance_common.h"

namespace firebase {
namespace performance {
namespace internal {

constexpr const char* const kHttpMethodToString[] = {
    "GET",   "PUT",     "POST",  "DELETE", "HEAD",
    "PATCH", "OPTIONS", "TRACE", "CONNECT"};

const char* GetFIRHttpMethodString(HttpMethod method) {
  return (method >= 0 &&
          method < static_cast<int>(FIREBASE_ARRAYSIZE(kHttpMethodToString)))
             ? kHttpMethodToString[method]
             : nullptr;
}

class HttpMetricInternal {
 public:
  HttpMetricInternal() = default;
  HttpMetricInternal(const HttpMetricInternal&) = delete;
  HttpMetricInternal& operator=(const HttpMetricInternal&) = delete;

  ~HttpMetricInternal() {
    MutexLock lock(mutex_);
    if (active_http_metric_) {
      JNIEnv* env = nullptr;
      if (IsInitialized()) {
        env = GetFirebaseApp()->GetJNIEnv();
        if (stop_on_destroy_ && is_started_) {
          env->CallVoidMethod(
              active_http_metric_,
              http_metric_jni::GetMethodId(http_metric_jni::kStopHttpMetric));
          jni::CheckAndClearException(env);
        }
      } else {
        env = firebase::util::GetJNIEnvFromApp();
      }
      if (env) env->DeleteGlobalRef(active_http_metric_);
    }
    active_http_metric_ = nullptr;
    is_started_ = false;
  }

  bool CreateHttpMetric(const char* url, HttpMethod http_method,
                        bool stop_on_destroy) {
    FIREBASE_ASSERT_RETURN(false, IsInitialized());
    MutexLock lock(mutex_);
    return CreateHttpMetricLocked(url, http_method, stop_on_destroy);
  }

  void StartCreatedHttpMetric() {
    FIREBASE_ASSERT_RETURN_VOID(IsInitialized());
    MutexLock lock(mutex_);
    StartCreatedHttpMetricLocked();
  }

  void CreateAndStartHttpMetric(const char* url, HttpMethod http_method) {
    FIREBASE_ASSERT_RETURN_VOID(IsInitialized());
    MutexLock lock(mutex_);
    if (CreateHttpMetricLocked(url, http_method, true)) {
      StartCreatedHttpMetricLocked();
    }
  }

  bool IsStarted() const {
    MutexLock lock(mutex_);
    return active_http_metric_ != nullptr && is_started_;
  }

  void CancelHttpMetric() {
    FIREBASE_ASSERT_RETURN_VOID(IsInitialized());
    MutexLock lock(mutex_);
    if (!WarnIfNotCreated("Cannot cancel HttpMetric."))
      CancelHttpMetricLocked();
  }

  void StopHttpMetric() {
    FIREBASE_ASSERT_RETURN_VOID(IsInitialized());
    MutexLock lock(mutex_);
    if (!WarnIfNotCreated("Cannot stop HttpMetric.")) StopHttpMetricLocked();
  }

  void SetAttribute(const char* name, const char* value) {
    FIREBASE_ASSERT_RETURN_VOID(IsInitialized());
    if (WarnIf(!name, "Cannot set value for null attribute.")) return;
    MutexLock lock(mutex_);
    if (WarnIfNotCreated("Cannot SetAttribute.")) return;
    JNIEnv* env = GetFirebaseApp()->GetJNIEnv();
    jstring jname = env->NewStringUTF(name);
    if (!value) {
      env->CallVoidMethod(
          active_http_metric_,
          http_metric_jni::GetMethodId(http_metric_jni::kRemoveAttribute),
          jname);
    } else {
      jstring jval = env->NewStringUTF(value);
      env->CallVoidMethod(
          active_http_metric_,
          http_metric_jni::GetMethodId(http_metric_jni::kSetAttribute), jname,
          jval);
      env->DeleteLocalRef(jval);
    }
    jni::CheckAndClearException(env);
    env->DeleteLocalRef(jname);
  }

  std::string GetAttribute(const char* name) const {
    FIREBASE_ASSERT_RETURN("", IsInitialized());
    if (WarnIf(!name, "attribute_name cannot be null.")) return "";
    MutexLock lock(mutex_);
    if (WarnIfNotCreated("Cannot GetAttribute.")) return "";
    JNIEnv* env = GetFirebaseApp()->GetJNIEnv();
    jstring jname = env->NewStringUTF(name);
    auto jval = static_cast<jstring>(env->CallObjectMethod(
        active_http_metric_,
        http_metric_jni::GetMethodId(http_metric_jni::kGetAttribute), jname));
    bool exc = jni::CheckAndClearException(env);
    env->DeleteLocalRef(jname);
    std::string res = (!exc && jval) ? jni::JStringToString(env, jval) : "";
    if (jval) env->DeleteLocalRef(jval);
    return res;
  }

  void set_http_response_code(int code) {
    FIREBASE_ASSERT_RETURN_VOID(IsInitialized());
    MutexLock lock(mutex_);
    if (WarnIfNotCreated("Cannot set_http_response_code.")) return;
    JNIEnv* env = GetFirebaseApp()->GetJNIEnv();
    env->CallVoidMethod(
        active_http_metric_,
        http_metric_jni::GetMethodId(http_metric_jni::kSetHttpResponseCode),
        code);
    jni::CheckAndClearException(env);
  }

  void set_request_payload_size(int64_t bytes) {
    CallLongSetter(http_metric_jni::kSetRequestPayloadSize, bytes,
                   "Cannot set_request_payload_size.");
  }

  void set_response_payload_size(int64_t bytes) {
    CallLongSetter(http_metric_jni::kSetResponsePayloadSize, bytes,
                   "Cannot set_response_payload_size.");
  }

  void set_response_content_type(const char* content_type) {
    FIREBASE_ASSERT_RETURN_VOID(IsInitialized());
    if (WarnIf(!content_type, "Cannot set null ResponseContentType.")) return;
    MutexLock lock(mutex_);
    if (WarnIfNotCreated("Cannot set_response_content_type.")) return;
    JNIEnv* env = GetFirebaseApp()->GetJNIEnv();
    jstring jct = env->NewStringUTF(content_type);
    env->CallVoidMethod(
        active_http_metric_,
        http_metric_jni::GetMethodId(http_metric_jni::kSetResponseContentType),
        jct);
    jni::CheckAndClearException(env);
    env->DeleteLocalRef(jct);
  }

 private:
  mutable Mutex mutex_;
  jobject active_http_metric_ = nullptr;
  bool stop_on_destroy_ = false;
  bool is_started_ = false;

  void CallLongSetter(http_metric_jni::Method m, int64_t bytes,
                      const char* warn_msg) {
    FIREBASE_ASSERT_RETURN_VOID(IsInitialized());
    MutexLock lock(mutex_);
    if (WarnIfNotCreated(warn_msg)) return;
    JNIEnv* env = GetFirebaseApp()->GetJNIEnv();
    env->CallVoidMethod(active_http_metric_, http_metric_jni::GetMethodId(m),
                        bytes);
    jni::CheckAndClearException(env);
  }

  bool CreateHttpMetricLocked(const char* url, HttpMethod http_method,
                              bool stop_on_destroy) {
    if (WarnIf(!url, "URL cannot be nullptr. Unable to create HttpMetric."))
      return false;
    const char* method_str = GetFIRHttpMethodString(http_method);
    if (WarnIf(!method_str, "Invalid HttpMethod. Unable to create HttpMetric."))
      return false;
    if (active_http_metric_) {
      if (stop_on_destroy_ && is_started_)
        StopHttpMetricLocked();
      else
        CancelHttpMetricLocked();
    }
    stop_on_destroy_ = stop_on_destroy;
    is_started_ = false;
    JNIEnv* env = GetFirebaseApp()->GetJNIEnv();
    jstring jurl = env->NewStringUTF(url);
    jstring jmethod = env->NewStringUTF(method_str);
    jobject local = env->CallObjectMethod(
        GetFirebasePerformanceClassInstance(),
        performance_jni::GetMethodId(performance_jni::kNewHttpMetric), jurl,
        jmethod);
    if (!jni::CheckAndClearException(env) && local) {
      active_http_metric_ = env->NewGlobalRef(local);
    }
    if (local) env->DeleteLocalRef(local);
    if (jurl) env->DeleteLocalRef(jurl);
    if (jmethod) env->DeleteLocalRef(jmethod);
    return active_http_metric_ != nullptr;
  }

  void StartCreatedHttpMetricLocked() {
    if (WarnIfNotCreated("Cannot start HttpMetric.") || is_started_) return;
    JNIEnv* env = GetFirebaseApp()->GetJNIEnv();
    env->CallVoidMethod(
        active_http_metric_,
        http_metric_jni::GetMethodId(http_metric_jni::kStartHttpMetric));
    if (!jni::CheckAndClearException(env)) is_started_ = true;
  }

  void CancelHttpMetricLocked() {
    GetFirebaseApp()->GetJNIEnv()->DeleteGlobalRef(active_http_metric_);
    active_http_metric_ = nullptr;
    is_started_ = false;
  }

  void StopHttpMetricLocked() {
    JNIEnv* env = GetFirebaseApp()->GetJNIEnv();
    if (is_started_) {
      env->CallVoidMethod(
          active_http_metric_,
          http_metric_jni::GetMethodId(http_metric_jni::kStopHttpMetric));
      jni::CheckAndClearException(env);
    }
    CancelHttpMetricLocked();
  }

  bool WarnIfNotCreated(const char* msg) const {
    if (!active_http_metric_) {
      LogWarning("%s HttpMetric is not active. Please create a new HttpMetric.",
                 msg);
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

HttpMetric::HttpMetric() : internal_(new internal::HttpMetricInternal()) {
  FIREBASE_ASSERT(internal::IsInitialized());
}
HttpMetric::HttpMetric(const char* url, HttpMethod http_method)
    : HttpMetric() {
  internal_->CreateAndStartHttpMetric(url, http_method);
}
HttpMetric::~HttpMetric() { delete internal_; }
HttpMetric::HttpMetric(HttpMetric&& other) noexcept
    : internal_(std::exchange(other.internal_, nullptr)) {}
HttpMetric& HttpMetric::operator=(HttpMetric&& other) noexcept {
  if (this != &other) {
    delete internal_;
    internal_ = std::exchange(other.internal_, nullptr);
  }
  return *this;
}
bool HttpMetric::is_started() const {
  return internal_ && internal_->IsStarted();
}
void HttpMetric::Cancel() {
  if (internal_) internal_->CancelHttpMetric();
}
void HttpMetric::Stop() {
  if (internal_) internal_->StopHttpMetric();
}
void HttpMetric::Start(const char* url, HttpMethod http_method) {
  if (!internal_) internal_ = new internal::HttpMetricInternal();
  internal_->CreateAndStartHttpMetric(url, http_method);
}
void HttpMetric::SetAttribute(const char* name, const char* value) {
  if (internal_) internal_->SetAttribute(name, value);
}
std::string HttpMetric::GetAttribute(const char* name) const {
  return internal_ ? internal_->GetAttribute(name) : "";
}
void HttpMetric::set_http_response_code(int code) {
  if (internal_) internal_->set_http_response_code(code);
}
void HttpMetric::set_request_payload_size(int64_t bytes) {
  if (internal_) internal_->set_request_payload_size(bytes);
}
void HttpMetric::set_response_content_type(const char* content_type) {
  if (internal_) internal_->set_response_content_type(content_type);
}
void HttpMetric::set_response_payload_size(int64_t bytes) {
  if (internal_) internal_->set_response_payload_size(bytes);
}
void HttpMetric::Create(const char* url, HttpMethod http_method) {
  if (!internal_) internal_ = new internal::HttpMetricInternal();
  internal_->CreateHttpMetric(url, http_method, false);
}
void HttpMetric::StartCreatedHttpMetric() {
  if (internal_) internal_->StartCreatedHttpMetric();
}

}  // namespace performance
}  // namespace firebase
