// Copyright 2021 Google LLC

#include <iostream>

#include "performance/src/include/firebase/performance/trace.h"

namespace firebase {
namespace performance {
namespace internal {

// TODO(tdeshpande): Fill this in when this is converted to a fake.
class TraceInternal {};

}  // namespace internal
}  // namespace performance
}  // namespace firebase

firebase::performance::Trace::Trace() {
  internal_ = new internal::TraceInternal();
  std::cout << "Created a trace object. " << std::endl;
}

firebase::performance::Trace::Trace(const char* name) {
  std::cout << "Created trace with name: " << name << std::endl;
}

firebase::performance::Trace::~Trace() {
  delete internal_;
  internal_ = nullptr;
  std::cout << "Destroyed a trace." << std::endl;
}

firebase::performance::Trace::Trace(Trace&& other) {
  internal_ = other.internal_;
  other.internal_ = nullptr;
}

firebase::performance::Trace& firebase::performance::Trace::operator=(
    Trace&& other) {
  if (this != &other) {
    delete internal_;

    internal_ = other.internal_;
    other.internal_ = nullptr;
  }
  return *this;
}

bool firebase::performance::Trace::is_started() { return true; }

void firebase::performance::Trace::Cancel() {
  std::cout << "Cancel()" << std::endl;
}

void firebase::performance::Trace::Stop() {
  std::cout << "Stop()" << std::endl;
}

void firebase::performance::Trace::Start(const char* name) {
  std::cout << "Start trace with name: " << name << std::endl;
}

int64_t firebase::performance::Trace::GetLongMetric(
    const char* metric_name) const {
  return 200;
}

void firebase::performance::Trace::IncrementMetric(const char* metric_name,
                                                   const int64_t increment_by) {
}

void firebase::performance::Trace::SetMetric(const char* metric_name,
                                             const int64_t metric_value) {}

void firebase::performance::Trace::SetAttribute(const char* attribute_name,
                                                const char* attribute_value) {}

std::string firebase::performance::Trace::GetAttribute(
    const char* attribute_name) const {
  return "value";
}

// Used only in SWIG.

void firebase::performance::Trace::Create(const char* name) {
  std::cout << "Created an underlying trace object without starting it. "
            << std::endl;
}

void firebase::performance::Trace::StartCreatedTrace() {
  std::cout << "Started the previously created trace." << std::endl;
}
