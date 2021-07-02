// Copyright 2021 Google LLC

#include <iostream>

#include "performance/src/include/firebase/performance/http_metric.h"

namespace firebase {
namespace performance {
namespace internal {

// TODO(tdeshpande): Fill this in when this is converted to a fake.
class HttpMetricInternal {};

}  // namespace internal
}  // namespace performance
}  // namespace firebase

firebase::performance::HttpMetric::HttpMetric() {
  internal_ = new internal::HttpMetricInternal();
  std::cout << "Constructing an http metric object." << std::endl;
}

firebase::performance::HttpMetric::HttpMetric(const char* url,
                                              HttpMethod http_method) {
  std::cout << "Constructing http metric for url: " << url << std::endl;
}

firebase::performance::HttpMetric::~HttpMetric() {
  delete internal_;
  internal_ = nullptr;
  std::cout << "Destroyed an HttpMetric object" << std::endl;
}

firebase::performance::HttpMetric::HttpMetric(HttpMetric&& other) {
  internal_ = other.internal_;
  other.internal_ = nullptr;
}

firebase::performance::HttpMetric& firebase::performance::HttpMetric::operator=(
    HttpMetric&& other) {
  if (this != &other) {
    delete internal_;

    internal_ = other.internal_;
    other.internal_ = nullptr;
  }
  return *this;
}

bool firebase::performance::HttpMetric::is_started() { return true; }

void firebase::performance::HttpMetric::Cancel() {
  std::cout << "Cancel()" << std::endl;
}

void firebase::performance::HttpMetric::Stop() {
  std::cout << "Stop()" << std::endl;
}

void firebase::performance::HttpMetric::Start(const char* url,
                                              HttpMethod http_method) {
  std::cout << "Start http metric for url: " << url << std::endl;
}

void firebase::performance::HttpMetric::SetAttribute(
    const char* attribute_name, const char* attribute_value) {
  std::cout << "Put attribute: " << attribute_value
            << " for name: " << attribute_name << std::endl;
}

std::string firebase::performance::HttpMetric::GetAttribute(
    const char* attribute_name) const {
  return "someValue";
}

void firebase::performance::HttpMetric::set_http_response_code(
    int http_response_code) {
  std::cout << "Set http response code to: " << http_response_code << std::endl;
}

void firebase::performance::HttpMetric::set_request_payload_size(
    int64_t bytes) {
  std::cout << "Request payload size set to: " << bytes << std::endl;
}

void firebase::performance::HttpMetric::set_response_content_type(
    const char* content_type) {}

void firebase::performance::HttpMetric::set_response_payload_size(
    int64_t bytes) {}

// Used only in SWIG.

void firebase::performance::HttpMetric::Create(const char* url,
                                               HttpMethod http_method) {
  std::cout << "Created an underlying http metric object without starting it."
            << std::endl;
}

void firebase::performance::HttpMetric::StartCreatedHttpMetric() {
  std::cout << "Started the previously created http metric." << std::endl;
}
