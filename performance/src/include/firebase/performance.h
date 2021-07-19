// Copyright 2021 Google LLC

#ifndef FIREBASE_PERFORMANCE_SRC_INCLUDE_FIREBASE_PERFORMANCE_H_
#define FIREBASE_PERFORMANCE_SRC_INCLUDE_FIREBASE_PERFORMANCE_H_

#include "firebase/app.h"
#include "firebase/performance/http_metric.h"
#include "firebase/performance/trace.h"

/// @brief Namespace that encompasses all Firebase APIs.
namespace firebase {

/// @brief Firebase Performance API.
///
/// See <a href="/docs/perf-mon">the developer guides</a> for general
/// information on using Firebase Performance in your apps.
///
/// This library is experimental and is not currently officially supported.
namespace performance {

/// @brief Initialize the Performance API.
///
/// This must be called prior to calling any other methods in the
/// firebase::performance namespace.
///
/// @param[in] app Default @ref firebase::App instance.
/// @return kInitResultSuccess if initialization succeeded, or
/// kInitResultFailedMissingDependency on Android if Google Play services is
/// not available on the current device.
///
/// @see firebase::App::GetInstance().
InitResult Initialize(const App& app);

/// @brief Terminate the Performance API.
///
/// Cleans up resources associated with the API.
///
/// Do note this does not disable any of the automatic platform specific
/// instrumentation that firebase performance does. Please explicitly disable
/// performance monitoring through
/// firebase::performance::SetPerformanceCollectionEnabled(bool enabled) for
/// that to happen.
void Terminate();

/// @brief Determines if Performance collection is enabled.
/// @returns true if performance collection is enabled, false otherwise.
bool GetPerformanceCollectionEnabled();

/// @brief Sets whether performance collection is enabled for this app on this
/// device.
///
/// This setting is persisted across app sessions. By default it is enabled.
///
/// This can be called before firebase::performance::Initialize(const App&
/// app) on iOS, but that is not true on Android due to the way the SDK is
/// initialized. If you need to disable firebase performance before that, see
/// <a href="/docs/perf-mon/disable-sdk">the documentation</a>.
///
/// @param[in] enabled true to enable performance collection, false to disable.
void SetPerformanceCollectionEnabled(bool enabled);
}  // namespace performance
}  // namespace firebase

#endif  // FIREBASE_PERFORMANCE_SRC_INCLUDE_FIREBASE_PERFORMANCE_H_
