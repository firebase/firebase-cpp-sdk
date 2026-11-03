// Copyright 2021 Google LLC

#include "performance/src/include/firebase/performance/http_metric.h"

#include <jni.h>

#include <cstdint>
#include <string>
#include <utility>

#include "app/src/assert.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/log.h"
#include "app/src/util_android.h"
#include "performance/src/android/performance_android_internal.h"
#include "performance/src/include/firebase/performance.h"
#include "performance/src/performance_common.h"

namespace firebase {
namespace performance {

namespace internal {

// The order in the array has to be exactly the same as the order in which
// they're declared in http_metric.h.
constexpr const char* const kHttpMethodToString[] = {
    "GET",   "PUT",     "POST",  "DELETE", "HEAD",
    "PATCH", "OPTIONS", "TRACE", "CONNECT"};

// Maps the firebase::performance::HttpMethod enum to its string counterpart.
const char* GetFIRHttpMethodString(HttpMethod method) {
  if (method < 0 ||
      method >= static_cast<int>(FIREBASE_ARRAYSIZE(kHttpMethodToString))) {
    return nullptr;
  }
  return kHttpMethodToString[method];
}

// The internal implementation of HttpMetric using the pImpl design pattern.
// Access to the underlying JNI global reference and lifecycle state is
// synchronized with mutex_.
class HttpMetricInternal {
 public:
  HttpMetricInternal() = default;
  HttpMetricInternal(const HttpMetricInternal&) = delete;
  HttpMetricInternal& operator=(const HttpMetricInternal&) = delete;

  ~HttpMetricInternal() {
    MutexLock lock(mutex_);
    if (IsInitialized() && active_http_metric_) {
      if (stop_on_destroy_ && is_started_) {
        StopHttpMetric();
      } else {
        CancelHttpMetric();
      }
    }
    active_http_metric_ = nullptr;
    is_started_ = false;
  }

  // Creates an underlying Java HttpMetric. If a previous one exists, it is
  // stopped (when stop_on_destroy is true and it was started) or cancelled.
  bool CreateHttpMetric(const char* url, HttpMethod http_method,
                        bool stop_on_destroy) {
    FIREBASE_ASSERT_RETURN(false, IsInitialized());

    if (WarnIf(url == nullptr,
               "URL cannot be nullptr. Unable to create HttpMetric.")) {
      return false;
    }
    const char* method_str = GetFIRHttpMethodString(http_method);
    if (WarnIf(method_str == nullptr,
               "Invalid HttpMethod. Unable to create HttpMetric.")) {
      return false;
    }

    MutexLock lock(mutex_);
    if (active_http_metric_ != nullptr) {
      if (stop_on_destroy && is_started_) {
        StopHttpMetric();
      } else {
        CancelHttpMetric();
      }
    }

    stop_on_destroy_ = stop_on_destroy;
    is_started_ = false;

    JNIEnv* env = GetFirebaseApp()->GetJNIEnv();

    jstring url_jstring = env->NewStringUTF(url);
    jstring http_method_jstring = env->NewStringUTF(method_str);

    jobject local_active_http_metric = env->CallObjectMethod(
        GetFirebasePerformanceClassInstance(),
        performance_jni::GetMethodId(performance_jni::kNewHttpMetric),
        url_jstring, http_method_jstring);
    bool had_exception = util::CheckAndClearJniExceptions(env);

    if (!had_exception && local_active_http_metric != nullptr) {
      active_http_metric_ = env->NewGlobalRef(local_active_http_metric);
    }
    if (local_active_http_metric != nullptr) {
      env->DeleteLocalRef(local_active_http_metric);
    }
    if (url_jstring) env->DeleteLocalRef(url_jstring);
    if (http_method_jstring) env->DeleteLocalRef(http_method_jstring);
    return active_http_metric_ != nullptr;
  }

  // Starts an already created Java HttpMetric.
  void StartCreatedHttpMetric() {
    FIREBASE_ASSERT_RETURN_VOID(IsInitialized());
    MutexLock lock(mutex_);
    if (WarnIfNotCreated("Cannot start HttpMetric.")) return;
    if (!is_started_) {
      JNIEnv* env = GetFirebaseApp()->GetJNIEnv();
      env->CallVoidMethod(
          active_http_metric_,
          http_metric_jni::GetMethodId(http_metric_jni::kStartHttpMetric));
      if (!util::CheckAndClearJniExceptions(env)) {
        is_started_ = true;
      }
    }
  }

  // Creates and starts an underlying Java HttpMetric.
  void CreateAndStartHttpMetric(const char* url, HttpMethod http_method) {
    MutexLock lock(mutex_);
    if (CreateHttpMetric(url, http_method, true)) {
      StartCreatedHttpMetric();
    }
  }

  // Gets whether the underlying HttpMetric has been started.
  bool IsStarted() const {
    MutexLock lock(mutex_);
    return active_http_metric_ != nullptr && is_started_;
  }

  // Cancels the http metric, and makes sure it isn't logged to the backend.
  void CancelHttpMetric() {
    FIREBASE_ASSERT_RETURN_VOID(IsInitialized());
    MutexLock lock(mutex_);
    if (WarnIfNotCreated("Cannot cancel HttpMetric.")) return;

    JNIEnv* env = GetFirebaseApp()->GetJNIEnv();
    env->DeleteGlobalRef(active_http_metric_);
    active_http_metric_ = nullptr;
    is_started_ = false;
  }

  // Stops the network trace if it hasn't already been stopped, and logs it to
  // the backend.
  void StopHttpMetric() {
    FIREBASE_ASSERT_RETURN_VOID(IsInitialized());
    MutexLock lock(mutex_);
    if (WarnIfNotCreated("Cannot stop HttpMetric.")) return;

    JNIEnv* env = GetFirebaseApp()->GetJNIEnv();
    if (is_started_) {
      env->CallVoidMethod(
          active_http_metric_,
          http_metric_jni::GetMethodId(http_metric_jni::kStopHttpMetric));
      util::CheckAndClearJniExceptions(env);
    }
    env->DeleteGlobalRef(active_http_metric_);
    active_http_metric_ = nullptr;
    is_started_ = false;
  }

  // Creates a custom attribute for the given network trace with the
  // given name and value.
  void SetAttribute(const char* attribute_name, const char* attribute_value) {
    FIREBASE_ASSERT_RETURN_VOID(IsInitialized());
    if (WarnIf(attribute_name == nullptr,
               "Cannot set value for null attribute."))
      return;
    MutexLock lock(mutex_);
    if (WarnIfNotCreated("Cannot SetAttribute.")) return;

    JNIEnv* env = GetFirebaseApp()->GetJNIEnv();

    jstring attribute_name_jstring = env->NewStringUTF(attribute_name);

    if (attribute_value == nullptr) {
      env->CallVoidMethod(
          active_http_metric_,
          http_metric_jni::GetMethodId(http_metric_jni::kRemoveAttribute),
          attribute_name_jstring);
      util::CheckAndClearJniExceptions(env);
    } else {
      jstring attribute_value_jstring = env->NewStringUTF(attribute_value);
      env->CallVoidMethod(
          active_http_metric_,
          http_metric_jni::GetMethodId(http_metric_jni::kSetAttribute),
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
    if (WarnIf(attribute_name == nullptr, "attribute_name cannot be null."))
      return "";
    MutexLock lock(mutex_);
    if (WarnIfNotCreated("Cannot GetAttribute.")) return "";

    JNIEnv* env = GetFirebaseApp()->GetJNIEnv();
    jstring attribute_name_jstring = env->NewStringUTF(attribute_name);
    jobject attribute_value_jstring = env->CallObjectMethod(
        active_http_metric_,
        http_metric_jni::GetMethodId(http_metric_jni::kGetAttribute),
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

  // Sets the HTTP Response Code (for eg. 404 or 200) of the network
  // trace.
  void set_http_response_code(int http_response_code) {
    FIREBASE_ASSERT_RETURN_VOID(IsInitialized());
    MutexLock lock(mutex_);
    if (WarnIfNotCreated("Cannot set_http_response_code.")) return;

    JNIEnv* env = GetFirebaseApp()->GetJNIEnv();
    env->CallVoidMethod(
        active_http_metric_,
        http_metric_jni::GetMethodId(http_metric_jni::kSetHttpResponseCode),
        http_response_code);
    util::CheckAndClearJniExceptions(env);
  }

  // Sets the Request Payload size in bytes for the network trace.
  void set_request_payload_size(int64_t bytes) {
    FIREBASE_ASSERT_RETURN_VOID(IsInitialized());
    MutexLock lock(mutex_);
    if (WarnIfNotCreated("Cannot set_request_payload_size.")) return;

    JNIEnv* env = GetFirebaseApp()->GetJNIEnv();
    env->CallVoidMethod(
        active_http_metric_,
        http_metric_jni::GetMethodId(http_metric_jni::kSetRequestPayloadSize),
        bytes);
    util::CheckAndClearJniExceptions(env);
  }

  // Sets the Response Content Type of the network trace.
  void set_response_content_type(const char* content_type) {
    FIREBASE_ASSERT_RETURN_VOID(IsInitialized());
    if (WarnIf(content_type == nullptr, "Cannot set null ResponseContentType."))
      return;
    MutexLock lock(mutex_);
    if (WarnIfNotCreated("Cannot set_response_content_type.")) return;

    JNIEnv* env = GetFirebaseApp()->GetJNIEnv();
    jstring content_type_jstring = env->NewStringUTF(content_type);
    env->CallVoidMethod(
        active_http_metric_,
        http_metric_jni::GetMethodId(http_metric_jni::kSetResponseContentType),
        content_type_jstring);
    util::CheckAndClearJniExceptions(env);
    env->DeleteLocalRef(content_type_jstring);
  }

  // Sets the Response Payload Size in bytes for the network trace.
  void set_response_payload_size(int64_t bytes) {
    FIREBASE_ASSERT_RETURN_VOID(IsInitialized());
    MutexLock lock(mutex_);
    if (WarnIfNotCreated("Cannot set_response_payload_size.")) return;

    JNIEnv* env = GetFirebaseApp()->GetJNIEnv();
    env->CallVoidMethod(
        active_http_metric_,
        http_metric_jni::GetMethodId(http_metric_jni::kSetResponsePayloadSize),
        bytes);
    util::CheckAndClearJniExceptions(env);
  }

 private:
  mutable Mutex mutex_;
  jobject active_http_metric_ = nullptr;

  // The unity implementation doesn't stop the underlying Java trace, whereas
  // the C++ implementation does. This flag is set when a Java trace is created
  // to track whether it should be stopped before deallocating the object.
  bool stop_on_destroy_ = false;
  bool is_started_ = false;

  bool WarnIfNotCreated(const char* warning_message_details) const {
    if (!active_http_metric_) {
      LogWarning(
          "%s HttpMetric is not active. Please create a "
          "new HttpMetric.",
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

HttpMetric::HttpMetric() {
  FIREBASE_ASSERT(internal::IsInitialized());
  internal_ = new internal::HttpMetricInternal();
}

HttpMetric::HttpMetric(const char* url, HttpMethod http_method) {
  FIREBASE_ASSERT(internal::IsInitialized());
  internal_ = new internal::HttpMetricInternal();
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
  return internal_ != nullptr && internal_->IsStarted();
}

void HttpMetric::Cancel() {
  if (!internal_) return;
  internal_->CancelHttpMetric();
}

void HttpMetric::Stop() {
  if (!internal_) return;
  internal_->StopHttpMetric();
}

void HttpMetric::Start(const char* url, HttpMethod http_method) {
  if (!internal_) {
    internal_ = new internal::HttpMetricInternal();
  }
  internal_->CreateAndStartHttpMetric(url, http_method);
}

void HttpMetric::SetAttribute(const char* attribute_name,
                              const char* attribute_value) {
  if (!internal_) return;
  internal_->SetAttribute(attribute_name, attribute_value);
}

std::string HttpMetric::GetAttribute(const char* attribute_name) const {
  if (!internal_) return "";
  return internal_->GetAttribute(attribute_name);
}

void HttpMetric::set_http_response_code(int http_response_code) {
  if (!internal_) return;
  internal_->set_http_response_code(http_response_code);
}

void HttpMetric::set_request_payload_size(int64_t bytes) {
  if (!internal_) return;
  internal_->set_request_payload_size(bytes);
}

void HttpMetric::set_response_content_type(const char* content_type) {
  if (!internal_) return;
  internal_->set_response_content_type(content_type);
}

void HttpMetric::set_response_payload_size(int64_t bytes) {
  if (!internal_) return;
  internal_->set_response_payload_size(bytes);
}

void HttpMetric::Create(const char* url, HttpMethod http_method) {
  if (!internal_) {
    internal_ = new internal::HttpMetricInternal();
  }
  internal_->CreateHttpMetric(url, http_method, false);
}

void HttpMetric::StartCreatedHttpMetric() {
  if (!internal_) return;
  internal_->StartCreatedHttpMetric();
}

}  // namespace performance
}  // namespace firebase
