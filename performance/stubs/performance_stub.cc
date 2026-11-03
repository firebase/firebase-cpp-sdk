// Copyright 2021 Google LLC

#include "app/src/assert.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/version.h"
#include "app/src/log.h"
#include "performance/src/include/firebase/performance.h"
#include "performance/src/performance_common.h"

namespace firebase {
namespace performance {

static bool g_initialized = false;
static bool g_collection_enabled = true;

namespace internal {

DEFINE_FIREBASE_VERSION_STRING(FirebasePerformance);

bool IsInitialized() { return g_initialized; }

}  // namespace internal

InitResult Initialize(const App& app) {
  if (g_initialized) {
    LogWarning("%s API already initialized", internal::kPerformanceModuleName);
    return kInitResultSuccess;
  }
  g_initialized = true;
  g_collection_enabled = true;
  internal::RegisterTerminateOnDefaultAppDestroy(&app);
  return kInitResultSuccess;
}

void Terminate() {
  if (!g_initialized) {
    LogWarning("%s API already shut down", internal::kPerformanceModuleName);
    return;
  }
  internal::UnregisterTerminateOnDefaultAppDestroy();
  g_initialized = false;
}

bool GetPerformanceCollectionEnabled() {
  FIREBASE_ASSERT_RETURN(false, internal::IsInitialized());
  return g_collection_enabled;
}

void SetPerformanceCollectionEnabled(bool enabled) {
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  g_collection_enabled = enabled;
}

}  // namespace performance
}  // namespace firebase
