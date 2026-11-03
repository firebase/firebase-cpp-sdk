// Copyright 2021 Google LLC

#include "performance/src/include/firebase/performance/http_metric.h"

#import <Foundation/Foundation.h>

#import "FIRHTTPMetric.h"
#import "FIRPerformance.h"

#include <cstdint>
#include <string>
#include <utility>

#include "app/src/assert.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "performance/src/include/firebase/performance.h"
#include "performance/src/performance_common.h"

namespace firebase {
namespace performance {

namespace internal {

// The order in the array has to be exactly the same as the order in which they're declared in
// http_metric.h.
constexpr FIRHTTPMethod kHttpMethodToFIRHTTPmethod[] = {
    FIRHTTPMethodGET,     FIRHTTPMethodPUT,   FIRHTTPMethodPOST,
    FIRHTTPMethodDELETE,  FIRHTTPMethodHEAD,  FIRHTTPMethodPATCH,
    FIRHTTPMethodOPTIONS, FIRHTTPMethodTRACE, FIRHTTPMethodCONNECT};

// Maps the firebase::performance::HttpMethod enum to its ObjC counterpart.
bool TryGetFIRHttpMethod(HttpMethod method, FIRHTTPMethod* out_method) {
  if (out_method == nullptr || method < 0 ||
      method >= static_cast<int>(FIREBASE_ARRAYSIZE(kHttpMethodToFIRHTTPmethod))) {
    return false;
  }
  *out_method = kHttpMethodToFIRHTTPmethod[method];
  return true;
}

// The internal implementation of HttpMetric using the pImpl design pattern.
// Access to the underlying Objective-C reference and lifecycle state is
// synchronized with mutex_.
class HttpMetricInternal {
 public:
  HttpMetricInternal() = default;
  HttpMetricInternal(const HttpMetricInternal&) = delete;
  HttpMetricInternal& operator=(const HttpMetricInternal&) = delete;

  ~HttpMetricInternal() {
    MutexLock lock(mutex_);
    if (IsInitialized() && current_http_metric_) {
      if (stop_on_destroy_ && is_started_) {
        StopHttpMetric();
      } else {
        CancelHttpMetric();
      }
    }
    current_http_metric_ = nil;
    is_started_ = false;
  }

  bool CreateHttpMetric(const char* url, HttpMethod http_method, bool stop_on_destroy) {
    FIREBASE_ASSERT_RETURN(false, IsInitialized());

    if (WarnIf(url == nullptr, "URL cannot be nullptr. Unable to start HttpMetric.")) return false;
    FIRHTTPMethod fir_method = FIRHTTPMethodGET;
    if (WarnIf(!TryGetFIRHttpMethod(http_method, &fir_method),
               "Invalid HttpMethod. Unable to start HttpMetric.")) {
      return false;
    }
    NSString* url_nsstring = [NSString stringWithUTF8String:url];
    if (WarnIf(url_nsstring == nil, "URL is not valid UTF-8. Unable to start HttpMetric.")) {
      return false;
    }
    NSURL* nsurl = [NSURL URLWithString:url_nsstring];
    if (WarnIf(nsurl == nil, "Invalid URL. Unable to start HttpMetric.")) return false;

    MutexLock lock(mutex_);
    if (current_http_metric_ != nil) {
      if (stop_on_destroy && is_started_) {
        StopHttpMetric();
      } else {
        CancelHttpMetric();
      }
    }

    stop_on_destroy_ = stop_on_destroy;
    is_started_ = false;

    FIRHTTPMetric* http_metric = [[FIRHTTPMetric alloc] initWithURL:nsurl
                                                         HTTPMethod:fir_method];
    current_http_metric_ = http_metric;
    return current_http_metric_ != nil;
  }

  void StartCreatedHttpMetric() {
    FIREBASE_ASSERT_RETURN_VOID(IsInitialized());
    MutexLock lock(mutex_);
    if (WarnIfNotCreated("Cannot start HttpMetric.")) return;
    if (!is_started_) {
      [current_http_metric_ start];
      is_started_ = true;
    }
  }

  // Creates and starts an underlying ObjC HttpMetric.
  void CreateAndStartHttpMetric(const char* url, HttpMethod http_method) {
    MutexLock lock(mutex_);
    if (CreateHttpMetric(url, http_method, true)) {
      StartCreatedHttpMetric();
    }
  }

  // Gets whether the HttpMetric associated with this object is started.
  bool IsStarted() const {
    MutexLock lock(mutex_);
    return current_http_metric_ != nil && is_started_;
  }

  // Cancels the http metric, and makes sure it isn't logged to the backend.
  void CancelHttpMetric() {
    FIREBASE_ASSERT_RETURN_VOID(IsInitialized());
    MutexLock lock(mutex_);
    if (WarnIfNotCreated("Cannot cancel HttpMetric.")) return;

    current_http_metric_ = nil;
    is_started_ = false;
  }

  // Stops the network trace if it hasn't already been stopped, and logs it to the backend.
  void StopHttpMetric() {
    FIREBASE_ASSERT_RETURN_VOID(IsInitialized());
    MutexLock lock(mutex_);
    if (WarnIfNotCreated("Cannot stop HttpMetric.")) return;

    if (is_started_) {
      [current_http_metric_ stop];
    }
    current_http_metric_ = nil;
    is_started_ = false;
  }

  // Creates a custom attribute for the given network trace with the
  // given name and value.
  void SetAttribute(const char* attribute_name, const char* attribute_value) {
    FIREBASE_ASSERT_RETURN_VOID(IsInitialized());
    if (WarnIf(attribute_name == nullptr, "Cannot set value for null attribute.")) return;
    MutexLock lock(mutex_);
    if (WarnIfNotCreated("Cannot SetAttribute.")) return;
    NSString* attr_name_ns = [NSString stringWithUTF8String:attribute_name];
    if (WarnIf(attr_name_ns == nil, "attribute_name is not valid UTF-8.")) return;

    if (attribute_value == nullptr) {
      [current_http_metric_ removeAttribute:attr_name_ns];
    } else {
      NSString* attr_val_ns = [NSString stringWithUTF8String:attribute_value];
      if (WarnIf(attr_val_ns == nil, "attribute_value is not valid UTF-8.")) return;
      [current_http_metric_ setValue:attr_val_ns forAttribute:attr_name_ns];
    }
  }

  // Gets the value of the custom attribute identified by the given
  // name or an empty string if it hasn't been set.
  std::string GetAttribute(const char* attribute_name) const {
    FIREBASE_ASSERT_RETURN("", IsInitialized());
    if (WarnIf(attribute_name == nullptr, "attribute_name cannot be null.")) return "";
    MutexLock lock(mutex_);
    if (WarnIfNotCreated("Cannot GetAttribute.")) return "";
    NSString* attr_name_ns = [NSString stringWithUTF8String:attribute_name];
    if (WarnIf(attr_name_ns == nil, "attribute_name is not valid UTF-8.")) return "";

    NSString* attribute_value = [current_http_metric_ valueForAttribute:attr_name_ns];
    const char* utf8_value = attribute_value != nil ? [attribute_value UTF8String] : nullptr;
    return utf8_value != nullptr ? std::string(utf8_value) : "";
  }

  // Sets the HTTP Response Code (for eg. 404 or 200) of the network
  // trace.
  void set_http_response_code(int http_response_code) {
    FIREBASE_ASSERT_RETURN_VOID(IsInitialized());
    MutexLock lock(mutex_);
    if (WarnIfNotCreated("Cannot set_http_response_code.")) return;

    current_http_metric_.responseCode = http_response_code;
  }

  // Sets the Request Payload size in bytes for the network trace.
  void set_request_payload_size(int64_t bytes) {
    FIREBASE_ASSERT_RETURN_VOID(IsInitialized());
    MutexLock lock(mutex_);
    if (WarnIfNotCreated("Cannot set_request_payload_size.")) return;

    current_http_metric_.requestPayloadSize = bytes;
  }

  // Sets the Response Content Type of the network trace.
  void set_response_content_type(const char* content_type) {
    FIREBASE_ASSERT_RETURN_VOID(IsInitialized());
    if (WarnIf(content_type == nullptr, "Cannot set null ResponseContentType.")) return;
    MutexLock lock(mutex_);
    if (WarnIfNotCreated("Cannot set_response_content_type.")) return;
    NSString* content_type_ns = [NSString stringWithUTF8String:content_type];
    if (WarnIf(content_type_ns == nil, "content_type is not valid UTF-8.")) return;

    current_http_metric_.responseContentType = content_type_ns;
  }

  // Sets the Response Payload Size in bytes for the network trace.
  void set_response_payload_size(int64_t bytes) {
    FIREBASE_ASSERT_RETURN_VOID(IsInitialized());
    MutexLock lock(mutex_);
    if (WarnIfNotCreated("Cannot set_response_payload_size.")) return;
    current_http_metric_.responsePayloadSize = bytes;
  }

 private:
  mutable Mutex mutex_;
  FIRHTTPMetric* current_http_metric_ = nil;

  // The unity implementation doesn't stop the underlying ObjC trace, whereas
  // the C++ implementation does. This flag is set when the ObjC trace is created
  // to track whether it should be stopped before deallocating the object.
  bool stop_on_destroy_ = false;
  bool is_started_ = false;

  bool WarnIfNotCreated(const char* warning_message_details) const {
    if (!current_http_metric_) {
      LogWarning("%s HttpMetric is not active. Please create a "
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

void HttpMetric::SetAttribute(const char* attribute_name, const char* attribute_value) {
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

// Used only in Unity.

// We need to expose this functionality because the Unity implementation needs
// it. The Unity implementation (unlike the C++ implementation) can create an
// HttpMetric without starting it, similar to the ObjC and Java APIs, which is
// why this is needed.

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
