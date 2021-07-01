// Copyright 2021 Google LLC

#import <Foundation/Foundation.h>

#import "FIRHTTPMetric.h"
#import "FIRPerformance.h"

#include "app/src/assert.h"
#include "app/src/include/firebase/internal/common.h"
#include "performance/src/include/firebase/performance.h"
#import "performance/src/include/firebase/performance/http_metric.h"
#include "performance/src/performance_common.h"

namespace firebase {
namespace performance {

namespace internal {

// The order in the array has to be exactly the same as the order in which they're declared in
// http_metric.h.
static const FIRHTTPMethod kHttpMethodToFIRHTTPmethod[] = {
    FIRHTTPMethodGET,     FIRHTTPMethodPUT,   FIRHTTPMethodPOST,
    FIRHTTPMethodDELETE,  FIRHTTPMethodHEAD,  FIRHTTPMethodPATCH,
    FIRHTTPMethodOPTIONS, FIRHTTPMethodTRACE, FIRHTTPMethodCONNECT};

// Maps the firebase::performance::HttpMethod enum to its ObjC counterpart.
FIRHTTPMethod GetFIRHttpMethod(HttpMethod method) {
  FIREBASE_ASSERT(method >= 0 && method < FIREBASE_ARRAYSIZE(kHttpMethodToFIRHTTPmethod));
  return kHttpMethodToFIRHTTPmethod[method];
}

// The Internal implementation of HttpMetric as recommended by the pImpl design pattern.
// This class is thread safe as long as we can assume that raw ponter access is atomic on any of the
// platforms this will be used on.
class HttpMetricInternal {
 public:
  explicit HttpMetricInternal() { current_http_metric_ = nil; }

  ~HttpMetricInternal() {
    if (current_http_metric_) {
      if (stop_on_destroy_) {
        StopHttpMetric();
      } else {
        CancelHttpMetric();
      }
    }
  }

  void CreateHttpMetric(const char* url, HttpMethod http_method, bool stop_on_destroy) {
    FIREBASE_ASSERT_RETURN_VOID(IsInitialized());

    stop_on_destroy_ = stop_on_destroy;

    if (WarnIf(url == nullptr, "URL cannot be nullptr. Unable to start HttpMetric.")) return;

    FIRHTTPMetric* http_metric = [[FIRHTTPMetric alloc] initWithURL:[NSURL URLWithString:@(url)]
                                                         HTTPMethod:GetFIRHttpMethod(http_method)];
    if (http_metric != nil) {
      current_http_metric_ = http_metric;
    }
  }

  void StartCreatedHttpMetric() { [current_http_metric_ start]; }

  // Creates and starts an underlying ObjC HttpMetric. If a previous one exists, it is cancelled.
  void CreateAndStartHttpMetric(const char* url, HttpMethod http_method) {
    CreateHttpMetric(url, http_method, true);
    StartCreatedHttpMetric();
  }

  // Gets whether the HttpMetric associated with this object is started.
  bool IsHttpMetricCreated() { return current_http_metric_ != nil; }

  // Cancels the http metric, and makes sure it isn't logged to the backend.
  void CancelHttpMetric() {
    FIREBASE_ASSERT_RETURN_VOID(IsInitialized());
    if (WarnIfNotCreated("Cannot cancel HttpMetric.")) return;

    current_http_metric_ = nil;
  }

  // Stops the network trace if it hasn't already been stopped, and logs it to the backend.
  void StopHttpMetric() {
    FIREBASE_ASSERT_RETURN_VOID(IsInitialized());
    if (WarnIfNotCreated("Cannot stop HttpMetric.")) return;

    [current_http_metric_ stop];
    current_http_metric_ = nil;
  }

  // Creates a custom attribute for the given network trace with the
  // given name and value.
  void SetAttribute(const char* attribute_name, const char* attribute_value) {
    FIREBASE_ASSERT_RETURN_VOID(IsInitialized());
    if (WarnIf(attribute_name == nullptr, "Cannot set value for null attribute.")) return;
    if (WarnIfNotCreated("Cannot SetAttribute.")) return;

    if (attribute_value == nullptr) {
      [current_http_metric_ removeAttribute:@(attribute_name)];
    } else {
      [current_http_metric_ setValue:@(attribute_value) forAttribute:@(attribute_name)];
    }
  }

  // Gets the value of the custom attribute identified by the given
  // name or an empty string if it hasn't been set.
  std::string GetAttribute(const char* attribute_name) const {
    FIREBASE_ASSERT_RETURN("", IsInitialized());
    if (WarnIf(attribute_name == nullptr, "attribute_name cannot be null.")) return "";
    if (WarnIfNotCreated("Cannot GetAttribute.")) return "";

    NSString* attribute_value = [current_http_metric_ valueForAttribute:@(attribute_name)];
    if (attribute_value != nil) {
      return [attribute_value UTF8String];
    } else {
      return "";
    }
  }

  // Sets the HTTP Response Code (for eg. 404 or 200) of the network
  // trace.
  void set_http_response_code(int http_response_code) {
    FIREBASE_ASSERT_RETURN_VOID(IsInitialized());
    if (WarnIfNotCreated("Cannot set_http_response_code.")) return;

    current_http_metric_.responseCode = http_response_code;
  }

  // Sets the Request Payload size in bytes for the network trace.
  void set_request_payload_size(int64_t bytes) {
    FIREBASE_ASSERT_RETURN_VOID(IsInitialized());
    if (WarnIfNotCreated("Cannot set_request_payload_size.")) return;

    current_http_metric_.requestPayloadSize = bytes;
  }

  // Sets the Response Content Type of the network trace.
  void set_response_content_type(const char* content_type) {
    FIREBASE_ASSERT_RETURN_VOID(IsInitialized());
    if (WarnIf(content_type == nullptr, "Cannot set null ResponseContentType.")) return;
    if (WarnIfNotCreated("Cannot set_response_content_type.")) return;

    current_http_metric_.responseContentType = @(content_type);
  }

  // Sets the Response Payload Size in bytes for the network trace.
  void set_response_payload_size(int64_t bytes) {
    FIREBASE_ASSERT_RETURN_VOID(IsInitialized());
    if (WarnIfNotCreated("Cannot set_response_payload_size.")) return;
    current_http_metric_.responsePayloadSize = bytes;
  }

 private:
  FIRHTTPMetric* current_http_metric_ = nil;

  // The unity implementation doesn't stop the underlying ObjC trace, whereas
  // the C++ implementation does. This flag is set when the ObjC trace is created
  // to track whether it should be stopped before deallocating the object.
  bool stop_on_destroy_ = false;

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
  // In the C++ API we never allow a situation where an underlying HttpMetric is created, but not
  // started, which is why this check is sufficient.
  return internal_->IsHttpMetricCreated();
}

void HttpMetric::Cancel() { internal_->CancelHttpMetric(); }

void HttpMetric::Stop() { internal_->StopHttpMetric(); }

void HttpMetric::Start(const char* url, HttpMethod http_method) {
  internal_->StopHttpMetric();
  internal_->CreateAndStartHttpMetric(url, http_method);
}

void HttpMetric::SetAttribute(const char* attribute_name, const char* attribute_value) {
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

// Used only in Unity.

// We need to expose this functionality because the Unity implementation needs
// it. The Unity implementation (unlike the C++ implementation) can create an
// HttpMetric without starting it, similar to the ObjC and Java APIs, which is
// why this is needed.

void HttpMetric::Create(const char* url, HttpMethod http_method) {
  internal_->CreateHttpMetric(url, http_method, false);
}

void HttpMetric::StartCreatedHttpMetric() { internal_->StartCreatedHttpMetric(); }

}  // namespace performance
}  // namespace firebase
