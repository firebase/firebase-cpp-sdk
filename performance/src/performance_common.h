// Copyright 2021 Google LLC

// This file contains the declarations of common functions that deal with
// initialization of the Firebase Performance C++ API and how it deals with the
// lifecycle of the common FirebaseApp instance. Some of these functions are
// implemented in a platform independent manner, while some are implemented
// specific to each platform.
#ifndef FIREBASE_PERFORMANCE_SRC_PERFORMANCE_COMMON_H_
#define FIREBASE_PERFORMANCE_SRC_PERFORMANCE_COMMON_H_

namespace firebase {
namespace performance {
namespace internal {

extern const char* kPerformanceModuleName;

// Returns whether the performance module is initialized.
// This is implemented per platform.
bool IsInitialized();

// Registers a cleanup task for this module if auto-initialization is disabled.
void RegisterTerminateOnDefaultAppDestroy();

// Unregisters the cleanup task for this module if auto-initialization is
// disabled.
void UnregisterTerminateOnDefaultAppDestroy();

}  // namespace internal
}  // namespace performance
}  // namespace firebase

#endif  // FIREBASE_PERFORMANCE_SRC_PERFORMANCE_COMMON_H_
