// Copyright 2021 Google LLC

#include "performance/src/include/firebase/performance/http_metric.h"

#include <cstdint>
#include <map>
#include <string>

#include "app/src/assert.h"
#include "app/src/log.h"
#include "performance/src/performance_common.h"

namespace firebase {
namespace performance {
namespace internal {

class HttpMetricInternal {
 public:
  bool is_created = false;
  bool is_started = false;
  int http_response_code = 0;
  int64_t request_payload_size = 0;
  int64_t response_payload_size = 0;
  std::string response_content_type;
  std::map<std::string, std::string> attributes;

  void Reset() {
    is_created = false;
    is_started = false;
    http_response_code = 0;
    request_payload_size = 0;
    response_payload_size = 0;
    response_content_type.clear();
    attributes.clear();
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
  Start(url, http_method);
}

HttpMetric::~HttpMetric() {
  delete internal_;
  internal_ = nullptr;
}

HttpMetric::HttpMetric(HttpMetric&& other) noexcept {
  internal_ = other.internal_;
  other.internal_ = nullptr;
}

HttpMetric& HttpMetric::operator=(HttpMetric&& other) noexcept {
  if (this != &other) {
    delete internal_;
    internal_ = other.internal_;
    other.internal_ = nullptr;
  }
  return *this;
}

bool HttpMetric::is_started() const {
  return internal_ != nullptr && internal_->is_started;
}

void HttpMetric::Cancel() {
  if (!internal_) return;
  internal_->Reset();
}

void HttpMetric::Stop() {
  if (!internal_) return;
  internal_->Reset();
}

void HttpMetric::Start(const char* url, HttpMethod http_method) {
  if (!internal_) {
    internal_ = new internal::HttpMetricInternal();
  }
  internal_->Reset();
  if (url == nullptr || http_method < kHttpMethodGet ||
      http_method > kHttpMethodConnect) {
    return;
  }
  internal_->is_created = true;
  internal_->is_started = true;
}

void HttpMetric::SetAttribute(const char* attribute_name,
                              const char* attribute_value) {
  if (!internal_ || !internal_->is_created || attribute_name == nullptr) {
    return;
  }
  if (attribute_value == nullptr) {
    internal_->attributes.erase(attribute_name);
  } else {
    internal_->attributes[attribute_name] = attribute_value;
  }
}

std::string HttpMetric::GetAttribute(const char* attribute_name) const {
  if (!internal_ || !internal_->is_created || attribute_name == nullptr) {
    return "";
  }
  auto it = internal_->attributes.find(attribute_name);
  return it != internal_->attributes.end() ? it->second : "";
}

void HttpMetric::set_http_response_code(int http_response_code) {
  if (!internal_ || !internal_->is_created) return;
  internal_->http_response_code = http_response_code;
}

void HttpMetric::set_request_payload_size(int64_t bytes) {
  if (!internal_ || !internal_->is_created) return;
  internal_->request_payload_size = bytes;
}

void HttpMetric::set_response_content_type(const char* content_type) {
  if (!internal_ || !internal_->is_created || content_type == nullptr) return;
  internal_->response_content_type = content_type;
}

void HttpMetric::set_response_payload_size(int64_t bytes) {
  if (!internal_ || !internal_->is_created) return;
  internal_->response_payload_size = bytes;
}

void HttpMetric::Create(const char* url, HttpMethod http_method) {
  if (!internal_) {
    internal_ = new internal::HttpMetricInternal();
  }
  internal_->Reset();
  if (url == nullptr || http_method < kHttpMethodGet ||
      http_method > kHttpMethodConnect) {
    return;
  }
  internal_->is_created = true;
  internal_->is_started = false;
}

void HttpMetric::StartCreatedHttpMetric() {
  if (!internal_ || !internal_->is_created) return;
  internal_->is_started = true;
}

}  // namespace performance
}  // namespace firebase
