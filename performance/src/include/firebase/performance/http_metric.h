// Copyright 2021 Google LLC

#ifndef FIREBASE_PERFORMANCE_SRC_INCLUDE_FIREBASE_PERFORMANCE_HTTP_METRIC_H_
#define FIREBASE_PERFORMANCE_SRC_INCLUDE_FIREBASE_PERFORMANCE_HTTP_METRIC_H_

#include <cstdint>
#include <string>

namespace firebase {
namespace performance {

namespace internal {
class HttpMetricInternal;
}

enum HttpMethod {
  kHttpMethodGet = 0,
  kHttpMethodPut,
  kHttpMethodPost,
  kHttpMethodDelete,
  kHttpMethodHead,
  kHttpMethodPatch,
  kHttpMethodOptions,
  kHttpMethodTrace,
  kHttpMethodConnect
};

class HttpMetric {
 public:
  HttpMetric();
  HttpMetric(const char* url, HttpMethod http_method);
  ~HttpMetric();

  HttpMetric(const HttpMetric& src) = delete;
  HttpMetric& operator=(const HttpMetric& src) = delete;

  HttpMetric(HttpMetric&& other) noexcept;
  HttpMetric& operator=(HttpMetric&& other) noexcept;

  bool is_started() const;
  void Cancel();
  void Stop();
  void Start(const char* url, HttpMethod http_method);

#if defined(INTERNAL_EXPERIMENTAL) || defined(SWIG)
  void Create(const char* url, HttpMethod http_method);
  void StartCreatedHttpMetric();
#endif  // defined(INTERNAL_EXPERIMENTAL) || defined(SWIG)

  void SetAttribute(const char* attribute_name, const char* attribute_value);
  std::string GetAttribute(const char* attribute_name) const;
  void set_http_response_code(int http_response_code);
  void set_request_payload_size(int64_t bytes);
  void set_response_content_type(const char* content_type);
  void set_response_payload_size(int64_t bytes);

 private:
  internal::HttpMetricInternal* internal_ = nullptr;
};

}  // namespace performance
}  // namespace firebase

#endif  // FIREBASE_PERFORMANCE_SRC_INCLUDE_FIREBASE_PERFORMANCE_HTTP_METRIC_H_
