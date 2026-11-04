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

constexpr FIRHTTPMethod kHttpMethodToFIRHTTPmethod[] = {
    FIRHTTPMethodGET,     FIRHTTPMethodPUT,   FIRHTTPMethodPOST,
    FIRHTTPMethodDELETE,  FIRHTTPMethodHEAD,  FIRHTTPMethodPATCH,
    FIRHTTPMethodOPTIONS, FIRHTTPMethodTRACE, FIRHTTPMethodCONNECT};

bool TryGetFIRHttpMethod(HttpMethod method, FIRHTTPMethod* out_method) {
  if (!out_method || method < 0 ||
      method >=
          static_cast<int>(FIREBASE_ARRAYSIZE(kHttpMethodToFIRHTTPmethod))) {
    return false;
  }
  *out_method = kHttpMethodToFIRHTTPmethod[method];
  return true;
}

class HttpMetricInternal {
 public:
  HttpMetricInternal() = default;
  HttpMetricInternal(const HttpMetricInternal&) = delete;
  HttpMetricInternal& operator=(const HttpMetricInternal&) = delete;

  ~HttpMetricInternal() {
    MutexLock lock(mutex_);
    if (IsInitialized() && current_http_metric_) {
      if (stop_on_destroy_ && is_started_)
        StopHttpMetric();
      else
        CancelHttpMetric();
    }
    current_http_metric_ = nil;
    is_started_ = false;
  }

  bool CreateHttpMetric(const char* url, HttpMethod http_method,
                        bool stop_on_destroy) {
    FIREBASE_ASSERT_RETURN(false, IsInitialized());
    if (WarnIf(!url, "URL cannot be nullptr. Unable to start HttpMetric."))
      return false;
    FIRHTTPMethod fir_method = FIRHTTPMethodGET;
    if (WarnIf(!TryGetFIRHttpMethod(http_method, &fir_method),
               "Invalid HttpMethod. Unable to start HttpMetric."))
      return false;
    NSString* url_ns = [NSString stringWithUTF8String:url];
    if (WarnIf(!url_ns, "URL is not valid UTF-8. Unable to start HttpMetric."))
      return false;
    NSURL* nsurl = [NSURL URLWithString:url_ns];
    if (WarnIf(!nsurl, "Invalid URL. Unable to start HttpMetric."))
      return false;

    MutexLock lock(mutex_);
    if (current_http_metric_ != nil) {
      if (stop_on_destroy && is_started_)
        StopHttpMetric();
      else
        CancelHttpMetric();
    }
    stop_on_destroy_ = stop_on_destroy;
    is_started_ = false;
    current_http_metric_ = [[FIRHTTPMetric alloc] initWithURL:nsurl
                                                   HTTPMethod:fir_method];
    return current_http_metric_ != nil;
  }

  void StartCreatedHttpMetric() {
    FIREBASE_ASSERT_RETURN_VOID(IsInitialized());
    MutexLock lock(mutex_);
    if (!WarnIfNotCreated("Cannot start HttpMetric.") && !is_started_) {
      [current_http_metric_ start];
      is_started_ = true;
    }
  }

  void CreateAndStartHttpMetric(const char* url, HttpMethod http_method) {
    MutexLock lock(mutex_);
    if (CreateHttpMetric(url, http_method, true)) StartCreatedHttpMetric();
  }

  bool IsStarted() const {
    MutexLock lock(mutex_);
    return current_http_metric_ != nil && is_started_;
  }

  void CancelHttpMetric() {
    FIREBASE_ASSERT_RETURN_VOID(IsInitialized());
    MutexLock lock(mutex_);
    if (!WarnIfNotCreated("Cannot cancel HttpMetric.")) {
      current_http_metric_ = nil;
      is_started_ = false;
    }
  }

  void StopHttpMetric() {
    FIREBASE_ASSERT_RETURN_VOID(IsInitialized());
    MutexLock lock(mutex_);
    if (WarnIfNotCreated("Cannot stop HttpMetric.")) return;
    if (is_started_) [current_http_metric_ stop];
    current_http_metric_ = nil;
    is_started_ = false;
  }

  void SetAttribute(const char* name, const char* value) {
    FIREBASE_ASSERT_RETURN_VOID(IsInitialized());
    if (WarnIf(!name, "Cannot set value for null attribute.")) return;
    MutexLock lock(mutex_);
    if (WarnIfNotCreated("Cannot SetAttribute.")) return;
    NSString* k = [NSString stringWithUTF8String:name];
    if (WarnIf(!k, "attribute_name is not valid UTF-8.")) return;
    if (!value) {
      [current_http_metric_ removeAttribute:k];
    } else if (NSString* v = [NSString stringWithUTF8String:value]) {
      [current_http_metric_ setValue:v forAttribute:k];
    } else {
      LogWarning("attribute_value is not valid UTF-8.");
    }
  }

  std::string GetAttribute(const char* name) const {
    FIREBASE_ASSERT_RETURN("", IsInitialized());
    if (WarnIf(!name, "attribute_name cannot be null.")) return "";
    MutexLock lock(mutex_);
    if (WarnIfNotCreated("Cannot GetAttribute.")) return "";
    NSString* k = [NSString stringWithUTF8String:name];
    if (WarnIf(!k, "attribute_name is not valid UTF-8.")) return "";
    NSString* v = [current_http_metric_ valueForAttribute:k];
    const char* s = v ? [v UTF8String] : nullptr;
    return s ? std::string(s) : "";
  }

  void set_http_response_code(int code) {
    FIREBASE_ASSERT_RETURN_VOID(IsInitialized());
    MutexLock lock(mutex_);
    if (!WarnIfNotCreated("Cannot set_http_response_code."))
      current_http_metric_.responseCode = code;
  }

  void set_request_payload_size(int64_t bytes) {
    FIREBASE_ASSERT_RETURN_VOID(IsInitialized());
    MutexLock lock(mutex_);
    if (!WarnIfNotCreated("Cannot set_request_payload_size."))
      current_http_metric_.requestPayloadSize = bytes;
  }

  void set_response_content_type(const char* content_type) {
    FIREBASE_ASSERT_RETURN_VOID(IsInitialized());
    if (WarnIf(!content_type, "Cannot set null ResponseContentType.")) return;
    MutexLock lock(mutex_);
    if (WarnIfNotCreated("Cannot set_response_content_type.")) return;
    if (NSString* ct = [NSString stringWithUTF8String:content_type]) {
      current_http_metric_.responseContentType = ct;
    } else {
      LogWarning("content_type is not valid UTF-8.");
    }
  }

  void set_response_payload_size(int64_t bytes) {
    FIREBASE_ASSERT_RETURN_VOID(IsInitialized());
    MutexLock lock(mutex_);
    if (!WarnIfNotCreated("Cannot set_response_payload_size."))
      current_http_metric_.responsePayloadSize = bytes;
  }

 private:
  mutable Mutex mutex_;
  FIRHTTPMetric* current_http_metric_ = nil;
  bool stop_on_destroy_ = false;
  bool is_started_ = false;

  bool WarnIfNotCreated(const char* msg) const {
    if (!current_http_metric_) {
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
