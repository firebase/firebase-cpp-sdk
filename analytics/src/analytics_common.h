/*
 * Copyright 2018 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef FIREBASE_ANALYTICS_CLIENT_CPP_SRC_ANALYTICS_COMMON_H__
#define FIREBASE_ANALYTICS_CLIENT_CPP_SRC_ANALYTICS_COMMON_H__

#include "app/src/reference_counted_future_impl.h"

namespace firebase {
namespace analytics {
namespace internal {

enum AnalyticsFn { kAnalyticsFnGetAnalyticsInstanceId, kAnalyticsFnCount };

// Data structure which holds the Future API for this module.
class FutureData {
 public:
  FutureData() : api_(kAnalyticsFnCount) {}
  ~FutureData() {}

  ReferenceCountedFutureImpl* api() { return &api_; }

  // Create FutureData singleton.
  static FutureData* Create();
  // Destroy the FutureData singleton.
  static void Destroy();
  // Get the Future data singleton.
  static FutureData* Get();

 private:
  ReferenceCountedFutureImpl api_;

  static FutureData* s_future_data_;
};

extern const char* kAnalyticsModuleName;

// Registers a cleanup task for this module if auto-initialization is disabled.
void RegisterTerminateOnDefaultAppDestroy();

// Unregisters the cleanup task for this module if auto-initialization is
// disabled.
void UnregisterTerminateOnDefaultAppDestroy();

// Determine whether the analytics module is initialized.
// This is implemented per platform.
bool IsInitialized();

}  // namespace internal
}  // namespace analytics
}  // namespace firebase

#endif  // FIREBASE_ANALYTICS_CLIENT_CPP_SRC_ANALYTICS_COMMON_H__
