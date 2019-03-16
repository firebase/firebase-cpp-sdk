/*
 * Copyright 2016 Google LLC
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

#ifndef FIREBASE_ADMOB_CLIENT_CPP_SRC_COMMON_ADMOB_COMMON_H_
#define FIREBASE_ADMOB_CLIENT_CPP_SRC_COMMON_ADMOB_COMMON_H_

#include <stdarg.h>
#include <vector>

#include "app/src/cleanup_notifier.h"
#include "app/src/reference_counted_future_impl.h"

namespace firebase {
namespace admob {

// Error messages used for completing futures. These match the error codes in
// the AdMobError enumeration in the C++ API.
extern const char* kAdUninitializedErrorMessage;
extern const char* kAdLoadInProgressErrorMessage;
extern const char* kInternalSDKErrorMesage;

// Determine whether admob is initialized.
bool IsInitialized();

// Registers a cleanup task for this module if auto-initialization is disabled.
void RegisterTerminateOnDefaultAppDestroy();

// Unregisters the cleanup task for this module if auto-initialization is
// disabled.
void UnregisterTerminateOnDefaultAppDestroy();

// Get the cleanup notifier for the AdMob module, creating one if it doesn't
// exist.  This allows all objects that depend upon AdMob's lifecycle to be
// cleaned up if the module is terminated.
CleanupNotifier* GetOrCreateCleanupNotifier();

// Destroy the cleanup notifier.
void DestroyCleanupNotifier();

// Returns the request agent string for this library.
const char* GetRequestAgentString();

// Hold backing data for returned Futures.
struct FutureData {
  explicit FutureData(int num_functions_that_return_futures)
      : future_impl(num_functions_that_return_futures) {}

  // Handle calls from Futures that the API returns.
  ReferenceCountedFutureImpl future_impl;
};

// Create a future and update the corresponding last result.
FutureHandle CreateFuture(int fn_idx, FutureData* future_data);

// Mark a future as complete.
void CompleteFuture(int error, const char* error_msg, FutureHandle handle,
                    FutureData* future_data);

// For calls that aren't asynchronous, create and complete the future at the
// same time.
void CreateAndCompleteFuture(int fn_idx, int error, const char* error_msg,
                             FutureData* future_data);

struct FutureCallbackData {
  FutureData* future_data;
  FutureHandle future_handle;
};

FutureCallbackData* CreateFutureCallbackData(FutureData* future_data,
                                             int fn_idx);

}  // namespace admob
}  // namespace firebase

#endif  // FIREBASE_ADMOB_CLIENT_CPP_SRC_COMMON_ADMOB_COMMON_H_
