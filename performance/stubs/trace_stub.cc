// Copyright 2021 Google LLC

#include "performance/src/include/firebase/performance/trace.h"

#include <cstdint>
#include <map>
#include <string>
#include <utility>

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
};

}  // namespace internal

Trace::Trace() : internal_(new internal::TraceInternal()) {
  FIREBASE_ASSERT(internal::IsInitialized());
}
Trace::Trace(const char* name) : Trace() { Start(name); }
Trace::~Trace() { delete internal_; }
Trace::Trace(Trace&& other) noexcept
    : internal_(std::exchange(other.internal_, nullptr)) {}
Trace& Trace::operator=(Trace&& other) noexcept {
  if (this != &other) {
    delete internal_;
    internal_ = std::exchange(other.internal_, nullptr);
  }
  return *this;
}
bool Trace::is_started() const { return internal_ && internal_->is_started; }
void Trace::Cancel() {
  if (internal_) *internal_ = {};
}
void Trace::Stop() {
  if (internal_) *internal_ = {};
}
void Trace::Start(const char* name) {
  Create(name);
  StartCreatedTrace();
}
int64_t Trace::GetLongMetric(const char* name) const {
  if (!internal_ || !internal_->is_created || !name) return 0;
  auto it = internal_->metrics.find(name);
  return it != internal_->metrics.end() ? it->second : 0;
}
void Trace::IncrementMetric(const char* name, const int64_t increment_by) {
  if (internal_ && internal_->is_created && name)
    internal_->metrics[name] += increment_by;
}
void Trace::SetMetric(const char* name, const int64_t metric_value) {
  if (internal_ && internal_->is_created && name)
    internal_->metrics[name] = metric_value;
}
void Trace::SetAttribute(const char* name, const char* value) {
  if (!internal_ || !internal_->is_created || !name) return;
  if (!value)
    internal_->attributes.erase(name);
  else
    internal_->attributes[name] = value;
}
std::string Trace::GetAttribute(const char* name) const {
  if (!internal_ || !internal_->is_created || !name) return "";
  auto it = internal_->attributes.find(name);
  return it != internal_->attributes.end() ? it->second : "";
}
void Trace::Create(const char* name) {
  if (!internal_) internal_ = new internal::TraceInternal();
  *internal_ = {};
  if (name) internal_->is_created = true;
}
void Trace::StartCreatedTrace() {
  if (internal_ && internal_->is_created) internal_->is_started = true;
}

}  // namespace performance
}  // namespace firebase
