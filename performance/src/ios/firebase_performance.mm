// Copyright 2021 Google LLC

#import <Foundation/Foundation.h>

#import "FIRPerformance.h"

#include <cassert>
#include "app/src/assert.h"
#include "performance/src/include/firebase/performance.h"
#include "performance/src/performance_common.h"

namespace firebase {
namespace performance {

static bool g_initialized = false;

// Initializes the API.
InitResult Initialize(const ::firebase::App& app) {
  if (g_initialized) {
    LogWarning("%s API already initialized", internal::kPerformanceModuleName);
    return InitResult::kInitResultSuccess;
  }
  g_initialized = true;
  internal::RegisterTerminateOnDefaultAppDestroy(&app);
  // TODO(tdeshpande): Figure out if there are any situations where initalizing fails on iOS.
  return InitResult::kInitResultSuccess;
}

namespace internal {

// Determines whether the performance module is initialized.
bool IsInitialized() { return g_initialized; }

}  // namespace internal

// Terminates the API.
void Terminate() {
  if (!g_initialized) {
    LogWarning("%s API already shut down", internal::kPerformanceModuleName);
    return;
  }
  internal::UnregisterTerminateOnDefaultAppDestroy();
  g_initialized = false;
}

// Determines if performance collection is enabled.
bool GetPerformanceCollectionEnabled() {
  FIREBASE_ASSERT_RETURN(false, internal::IsInitialized());
  return [[FIRPerformance sharedInstance] isDataCollectionEnabled];
}

// Sets performance collection enabled or disabled.
void SetPerformanceCollectionEnabled(bool enabled) {
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  [FIRPerformance sharedInstance].dataCollectionEnabled = enabled;
}

}  // namespace performance
}  // namespace firebase
