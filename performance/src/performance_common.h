// Copyright 2021 Google LLC

// This file contains the declarations of common functions that deal with
// initialization of the Firebase Performance C++ API and how it deals with the
// lifecycle of the common FirebaseApp instance. Some of these functions are
// implemented in a platform independent manner, while some are implemented
// specific to each platform.
#ifndef FIREBASE_PERFORMANCE_SRC_PERFORMANCE_COMMON_H_
#define FIREBASE_PERFORMANCE_SRC_PERFORMANCE_COMMON_H_

#include <mutex>

#include "app/src/include/firebase/app.h"

namespace firebase {
namespace performance {
namespace internal {

extern const char* kPerformanceModuleName;

// Serializes Initialize() / Terminate() against AppState teardown guards.
std::mutex& GetLifecycleMutex();

// Returns whether the performance module is initialized.
// This is implemented per platform.
bool IsInitialized();

// Registers a cleanup task on the given App's AppState to terminate performance
// when the AppState is destroyed.
void RegisterTerminateOnAppDestroy(const App& app);

// Unregisters the cleanup task for this module.
void UnregisterTerminateOnAppDestroy();

}  // namespace internal
}  // namespace performance
}  // namespace firebase

#endif  // FIREBASE_PERFORMANCE_SRC_PERFORMANCE_COMMON_H_
