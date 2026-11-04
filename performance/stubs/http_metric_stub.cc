// Copyright 2021 Google LLC

#include "performance/src/include/firebase/performance/http_metric.h"

#include <cstdint>
#include <map>
#include <string>
#include <utility>

#include "app/src/assert.h"
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
};

}  // namespace internal

HttpMetric::HttpMetric() : internal_(new internal::HttpMetricInternal()) {
  FIREBASE_ASSERT(internal::IsInitialized());
}
HttpMetric::HttpMetric(const char* url, HttpMethod http_method) : HttpMetric() {
  Start(url, http_method);
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
bool HttpMetric::is_started() const { return internal_ && internal_->is_started; }
void HttpMetric::Cancel() {
  if (internal_) *internal_ = {};
}
void HttpMetric::Stop() {
  if (internal_) *internal_ = {};
}
void HttpMetric::Start(const char* url, HttpMethod http_method) {
  Create(url, http_method);
  StartCreatedHttpMetric();
}
void HttpMetric::SetAttribute(const char* name, const char* value) {
  if (!internal_ || !internal_->is_created || !name) return;
  if (!value)
    internal_->attributes.erase(name);
  else
    internal_->attributes[name] = value;
}
std::string HttpMetric::GetAttribute(const char* name) const {
  if (!internal_ || !internal_->is_created || !name) return "";
  auto it = internal_->attributes.find(name);
  return it != internal_->attributes.end() ? it->second : "";
}
void HttpMetric::set_http_response_code(int code) {
  if (internal_ && internal_->is_created) internal_->http_response_code = code;
}
void HttpMetric::set_request_payload_size(int64_t bytes) {
  if (internal_ && internal_->is_created)
    internal_->request_payload_size = bytes;
}
void HttpMetric::set_response_content_type(const char* content_type) {
  if (internal_ && internal_->is_created && content_type)
    internal_->response_content_type = content_type;
}
void HttpMetric::set_response_payload_size(int64_t bytes) {
  if (internal_ && internal_->is_created)
    internal_->response_payload_size = bytes;
}
void HttpMetric::Create(const char* url, HttpMethod http_method) {
  if (!internal_) internal_ = new internal::HttpMetricInternal();
  *internal_ = {};
  if (url && http_method >= kHttpMethodGet &&
      http_method <= kHttpMethodConnect) {
    internal_->is_created = true;
  }
}
void HttpMetric::StartCreatedHttpMetric() {
  if (internal_ && internal_->is_created) internal_->is_started = true;
}

}  // namespace performance
}  // namespace firebase
