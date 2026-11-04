// Copyright 2021 Google LLC

#ifndef FIREBASE_PERFORMANCE_SRC_INCLUDE_FIREBASE_PERFORMANCE_H_
#define FIREBASE_PERFORMANCE_SRC_INCLUDE_FIREBASE_PERFORMANCE_H_

#include "firebase/app.h"
#include "firebase/internal/common.h"
#include "firebase/performance/http_metric.h"
#include "firebase/performance/trace.h"

#if !defined(SWIG) && !defined(DOXYGEN)
FIREBASE_APP_REGISTER_CALLBACKS_REFERENCE(performance)
#endif  // !defined(SWIG) && !defined(DOXYGEN)

namespace firebase {
namespace performance {

InitResult Initialize(const App& app);
void Terminate();
bool GetPerformanceCollectionEnabled();
void SetPerformanceCollectionEnabled(bool enabled);

}  // namespace performance
}  // namespace firebase

#endif  // FIREBASE_PERFORMANCE_SRC_INCLUDE_FIREBASE_PERFORMANCE_H_
