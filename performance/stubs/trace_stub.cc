// Copyright 2021 Google LLC

#include "performance/src/include/firebase/performance/trace.h"

#include <cstdint>
#include <map>
#include <string>

#include "app/src/assert.h"
#include "performance/src/performance_common.h"

namespace firebase {
namespace performance {
namespace internal {

class TraceInternal {
 public:
  bool is_created = false;
  bool is_started = false;
  std::map<std::string, int64_t> metrics;
  std::map<std::string, std::string> attributes;

  void Reset() {
    is_created = false;
    is_started = false;
    metrics.clear();
    attributes.clear();
  }
};

}  // namespace internal

Trace::Trace() {
  FIREBASE_ASSERT(internal::IsInitialized());
  internal_ = new internal::TraceInternal();
}

Trace::Trace(const char* name) {
  FIREBASE_ASSERT(internal::IsInitialized());
  internal_ = new internal::TraceInternal();
  Start(name);
}

Trace::~Trace() {
  delete internal_;
  internal_ = nullptr;
}

Trace::Trace(Trace&& other) noexcept {
  internal_ = other.internal_;
  other.internal_ = nullptr;
}

Trace& Trace::operator=(Trace&& other) noexcept {
  if (this != &other) {
    delete internal_;
    internal_ = other.internal_;
    other.internal_ = nullptr;
  }
  return *this;
}

bool Trace::is_started() const {
  return internal_ != nullptr && internal_->is_started;
}

void Trace::Cancel() {
  if (!internal_) return;
  internal_->Reset();
}

void Trace::Stop() {
  if (!internal_) return;
  internal_->Reset();
}

void Trace::Start(const char* name) {
  if (!internal_) {
    internal_ = new internal::TraceInternal();
  }
  internal_->Reset();
  if (name == nullptr) {
    return;
  }
  internal_->is_created = true;
  internal_->is_started = true;
}

int64_t Trace::GetLongMetric(const char* metric_name) const {
  if (!internal_ || !internal_->is_created || metric_name == nullptr) {
    return 0;
  }
  auto it = internal_->metrics.find(metric_name);
  return it != internal_->metrics.end() ? it->second : 0;
}

void Trace::IncrementMetric(const char* metric_name,
                            const int64_t increment_by) {
  if (!internal_ || !internal_->is_created || metric_name == nullptr) {
    return;
  }
  internal_->metrics[metric_name] += increment_by;
}

void Trace::SetMetric(const char* metric_name, const int64_t metric_value) {
  if (!internal_ || !internal_->is_created || metric_name == nullptr) {
    return;
  }
  internal_->metrics[metric_name] = metric_value;
}

void Trace::SetAttribute(const char* attribute_name,
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

std::string Trace::GetAttribute(const char* attribute_name) const {
  if (!internal_ || !internal_->is_created || attribute_name == nullptr) {
    return "";
  }
  auto it = internal_->attributes.find(attribute_name);
  return it != internal_->attributes.end() ? it->second : "";
}

void Trace::Create(const char* name) {
  if (!internal_) {
    internal_ = new internal::TraceInternal();
  }
  internal_->Reset();
  if (name == nullptr) {
    return;
  }
  internal_->is_created = true;
  internal_->is_started = false;
}

void Trace::StartCreatedTrace() {
  if (!internal_ || !internal_->is_created) return;
  internal_->is_started = true;
}

}  // namespace performance
}  // namespace firebase
