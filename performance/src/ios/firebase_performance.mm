// Copyright 2021 Google LLC

#import <Foundation/Foundation.h>

#import "FIRPerformance.h"

#include <atomic>
#include <cassert>
#include <mutex>
#include "app/src/assert.h"
#include "performance/src/include/firebase/performance.h"
#include "performance/src/performance_common.h"

namespace firebase {
namespace performance {

static std::atomic<bool> g_initialized{false};

// Initializes the API.
InitResult Initialize(const ::firebase::App& app) {
  std::lock_guard<std::mutex> lifecycle_lock(internal::GetLifecycleMutex());
  if (!app.is_valid()) {
    return InitResult::kInitResultFailedMissingDependency;
  }
  if (g_initialized.load(std::memory_order_acquire)) {
    return InitResult::kInitResultSuccess;
  }
  g_initialized.store(true, std::memory_order_release);
  internal::RegisterTerminateOnAppDestroy(app);
  // TODO(tdeshpande): Figure out if there are any situations where initalizing fails on iOS.
  return InitResult::kInitResultSuccess;
}

namespace internal {

// Determines whether the analytics module is initialized.
bool IsInitialized() { return g_initialized.load(std::memory_order_acquire); }

}  // namespace internal

// Terminates the API.
void Terminate() {
  std::lock_guard<std::mutex> lifecycle_lock(internal::GetLifecycleMutex());
  if (!g_initialized.load(std::memory_order_acquire)) {
    return;
  }
  internal::UnregisterTerminateOnAppDestroy();
  g_initialized.store(false, std::memory_order_release);
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
