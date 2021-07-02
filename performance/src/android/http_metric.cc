// Copyright 2021 Google LLC

#import "performance/src/include/firebase/performance/http_metric.h"

#include <jni.h>

#include <string>

#include "app/src/assert.h"
#include "app/src/include/firebase/internal/common.h"
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
static const char* kHttpMethodToString[] = {"GET",     "PUT",   "POST",
                                            "DELETE",  "HEAD",  "PATCH",
                                            "OPTIONS", "TRACE", "CONNECT"};

// Maps the firebase::performance::HttpMethod enum to its string counterpart.
const char* GetFIRHttpMethodString(HttpMethod method) {
  FIREBASE_ASSERT(method >= 0 &&
                  method < FIREBASE_ARRAYSIZE(kHttpMethodToString));
  return kHttpMethodToString[method];
}

// The Internal implementation of HttpMetric as recommended by the pImpl design
// pattern. This class is thread safe as long as we can assume that raw ponter
// access is atomic on any of the platforms this will be used on.
class HttpMetricInternal {
 public:
  explicit HttpMetricInternal() {}

  ~HttpMetricInternal() {
    if (active_http_metric_) {
      if (stop_on_destroy_) {
        StopHttpMetric();
      } else {
        CancelHttpMetric();
      }
    }
  }

  // Creates an underlying Java HttpMetric. If a previous one exists,
  // it is cancelled.
  void CreateHttpMetric(const char* url, HttpMethod http_method,
                        bool stop_on_destroy) {
    FIREBASE_ASSERT_RETURN_VOID(IsInitialized());
    stop_on_destroy_ = stop_on_destroy;

    if (WarnIf(url == nullptr,
               "URL cannot be nullptr. Unable to create HttpMetric."))
      return;

    JNIEnv* env = GetFirebaseApp()->GetJNIEnv();

    if (active_http_metric_ != nullptr) {
      CancelHttpMetric();
    }

    jstring url_jstring = url ? env->NewStringUTF(url) : nullptr;
    jstring http_method_jstring =
        env->NewStringUTF(GetFIRHttpMethodString(http_method));

    jobject local_active_http_metric = env->CallObjectMethod(
        GetFirebasePerformanceClassInstance(),
        performance_jni::GetMethodId(performance_jni::kNewHttpMetric),
        url_jstring, http_method_jstring);
    util::CheckAndClearJniExceptions(env);

    active_http_metric_ = env->NewGlobalRef(local_active_http_metric);
    env->DeleteLocalRef(local_active_http_metric);
    if (url_jstring) env->DeleteLocalRef(url_jstring);
    env->DeleteLocalRef(http_method_jstring);
  }

  // Starts an already created Java HttpMetric.
  void StartCreatedHttpMetric() {
    JNIEnv* env = GetFirebaseApp()->GetJNIEnv();
    if (active_http_metric_) {
      env->CallVoidMethod(
          active_http_metric_,
          http_metric_jni::GetMethodId(http_metric_jni::kStartHttpMetric));
      util::CheckAndClearJniExceptions(env);
    }
  }

  // Creates and starts an underlying Java HttpMetric. If a previous one
  // exists, it is cancelled.
  void CreateAndStartHttpMetric(const char* url, HttpMethod http_method) {
    CreateHttpMetric(url, http_method, true);
    StartCreatedHttpMetric();
  }

  // Gets whether the underlying HttpMetric associated with this object is
  // created.
  bool IsHttpMetricCreated() { return active_http_metric_ != nullptr; }

  // Cancels the http metric, and makes sure it isn't logged to the backend.
  void CancelHttpMetric() {
    FIREBASE_ASSERT_RETURN_VOID(IsInitialized());
    if (WarnIfNotCreated("Cannot cancel HttpMetric.")) return;

    JNIEnv* env = GetFirebaseApp()->GetJNIEnv();
    env->DeleteGlobalRef(active_http_metric_);
    active_http_metric_ = nullptr;
  }

  // Stops the network trace if it hasn't already been stopped, and logs it to
  // the backend.
  void StopHttpMetric() {
    FIREBASE_ASSERT_RETURN_VOID(IsInitialized());
    if (WarnIfNotCreated("Cannot stop HttpMetric.")) return;

    JNIEnv* env = GetFirebaseApp()->GetJNIEnv();
    env->CallVoidMethod(
        active_http_metric_,
        http_metric_jni::GetMethodId(http_metric_jni::kStopHttpMetric));
    util::CheckAndClearJniExceptions(env);
    env->DeleteGlobalRef(active_http_metric_);
    active_http_metric_ = nullptr;
  }

  // Creates a custom attribute for the given network trace with the
  // given name and value.
  void SetAttribute(const char* attribute_name, const char* attribute_value) {
    FIREBASE_ASSERT_RETURN_VOID(IsInitialized());
    if (WarnIf(attribute_name == nullptr,
               "Cannot set value for null attribute."))
      return;
    if (WarnIfNotCreated("Cannot SetAttribute.")) return;

    JNIEnv* env = GetFirebaseApp()->GetJNIEnv();

    jstring attribute_name_jstring = env->NewStringUTF(attribute_name);

    if (attribute_value == nullptr) {
      env->CallVoidMethod(
          active_http_metric_,
          http_metric_jni::GetMethodId(http_metric_jni::kRemoveAttribute),
          attribute_name_jstring);
    } else {
      jstring attribute_value_jstring = env->NewStringUTF(attribute_value);
      env->CallVoidMethod(
          active_http_metric_,
          http_metric_jni::GetMethodId(http_metric_jni::kSetAttribute),
          attribute_name_jstring, attribute_value_jstring);
      env->DeleteLocalRef(attribute_value_jstring);
    }

    util::CheckAndClearJniExceptions(env);
    env->DeleteLocalRef(attribute_name_jstring);
  }

  // Gets the value of the custom attribute identified by the given
  // name or an empty string if it hasn't been set.
  std::string GetAttribute(const char* attribute_name) const {
    FIREBASE_ASSERT_RETURN("", IsInitialized());
    if (WarnIf(attribute_name == nullptr, "attribute_name cannot be null."))
      return "";
    if (WarnIfNotCreated("Cannot GetAttribute.")) return "";

    JNIEnv* env = GetFirebaseApp()->GetJNIEnv();
    jstring attribute_name_jstring =
        attribute_name ? env->NewStringUTF(attribute_name) : nullptr;
    jobject attribute_value_jstring = env->CallObjectMethod(
        active_http_metric_,
        http_metric_jni::GetMethodId(http_metric_jni::kGetAttribute),
        attribute_name_jstring);
    util::CheckAndClearJniExceptions(env);
    env->DeleteLocalRef(attribute_name_jstring);

    if (attribute_value_jstring == nullptr) {
      return "";
    } else {
      return util::JniStringToString(env, attribute_value_jstring);
    }
  }

  // Sets the HTTP Response Code (for eg. 404 or 200) of the network
  // trace.
  void set_http_response_code(int http_response_code) {
    FIREBASE_ASSERT_RETURN_VOID(IsInitialized());
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
    if (WarnIfNotCreated("Cannot set_response_payload_size.")) return;

    JNIEnv* env = GetFirebaseApp()->GetJNIEnv();
    env->CallVoidMethod(
        active_http_metric_,
        http_metric_jni::GetMethodId(http_metric_jni::kSetResponsePayloadSize),
        bytes);
    util::CheckAndClearJniExceptions(env);
  }

 private:
  jobject active_http_metric_ = nullptr;

  // The unity implementation doesn't stop the underlying Java trace, whereas
  // the C++ implementation does. This flag is set when a Java trace is created
  // to track whether it should be stopped before deallocating the object.
  bool stop_on_destroy_ = false;

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
    if (condition) LogWarning(warning_message);
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

HttpMetric::HttpMetric(HttpMetric&& other) {
  internal_ = other.internal_;
  other.internal_ = nullptr;
}

HttpMetric& HttpMetric::operator=(HttpMetric&& other) {
  if (this != &other) {
    delete internal_;

    internal_ = other.internal_;
    other.internal_ = nullptr;
  }
  return *this;
}

bool HttpMetric::is_started() {
  // In the C++ API we never allow a situation where an underlying HttpMetric
  // is created, but not started, which is why this check is sufficient.
  // This isn't used in the Unity implementation.
  return internal_->IsHttpMetricCreated();
}

void HttpMetric::Cancel() { internal_->CancelHttpMetric(); }

void HttpMetric::Stop() { internal_->StopHttpMetric(); }

void HttpMetric::Start(const char* url, HttpMethod http_method) {
  internal_->StopHttpMetric();
  internal_->CreateAndStartHttpMetric(url, http_method);
}

void HttpMetric::SetAttribute(const char* attribute_name,
                              const char* attribute_value) {
  internal_->SetAttribute(attribute_name, attribute_value);
}

std::string HttpMetric::GetAttribute(const char* attribute_name) const {
  return internal_->GetAttribute(attribute_name);
}

void HttpMetric::set_http_response_code(int http_response_code) {
  internal_->set_http_response_code(http_response_code);
}

void HttpMetric::set_request_payload_size(int64_t bytes) {
  internal_->set_request_payload_size(bytes);
}

void HttpMetric::set_response_content_type(const char* content_type) {
  internal_->set_response_content_type(content_type);
}

void HttpMetric::set_response_payload_size(int64_t bytes) {
  internal_->set_response_payload_size(bytes);
}

void HttpMetric::Create(const char* url, HttpMethod http_method) {
  internal_->CreateHttpMetric(url, http_method, false);
}

void HttpMetric::StartCreatedHttpMetric() {
  internal_->StartCreatedHttpMetric();
}

}  // namespace performance
}  // namespace firebase
