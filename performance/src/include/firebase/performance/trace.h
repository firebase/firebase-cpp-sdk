// Copyright 2021 Google LLC

#ifndef FIREBASE_PERFORMANCE_SRC_INCLUDE_FIREBASE_PERFORMANCE_TRACE_H_
#define FIREBASE_PERFORMANCE_SRC_INCLUDE_FIREBASE_PERFORMANCE_TRACE_H_

#include <cstdint>
#include <string>

#include "firebase/performance.h"

namespace firebase {
namespace performance {

namespace internal {
class TraceInternal;
}

class Trace {
 public:
  explicit Trace();
  explicit Trace(const char* name);
  ~Trace();

  Trace(const Trace& src) = delete;
  Trace& operator=(const Trace& src) = delete;

  Trace(Trace&& other) noexcept;
  Trace& operator=(Trace&& other) noexcept;

  bool is_started() const;
  void Cancel();
  void Stop();
  void Start(const char* name);

  void Create(const char* name);
  void StartCreatedTrace();

  int64_t GetLongMetric(const char* metric_name) const;
  void IncrementMetric(const char* metric_name, const int64_t increment_by);
  void SetMetric(const char* metric_name, const int64_t metric_value);
  void SetAttribute(const char* attribute_name, const char* attribute_value);
  std::string GetAttribute(const char* attribute_name) const;

 private:
  internal::TraceInternal* internal_ = nullptr;
};

}  // namespace performance
}  // namespace firebase

#endif  // FIREBASE_PERFORMANCE_SRC_INCLUDE_FIREBASE_PERFORMANCE_TRACE_H_
