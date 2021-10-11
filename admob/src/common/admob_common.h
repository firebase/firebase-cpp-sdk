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

#ifndef FIREBASE_ADMOB_SRC_COMMON_ADMOB_COMMON_H_
#define FIREBASE_ADMOB_SRC_COMMON_ADMOB_COMMON_H_

#include <stdarg.h>

#include <vector>

#include "admob/src/include/firebase/admob/types.h"
#include "app/src/cleanup_notifier.h"
#include "app/src/reference_counted_future_impl.h"

namespace firebase {
namespace admob {

// Error messages used for completing futures. These match the error codes in
// the AdMobError enumeration in the C++ API.
extern const char* kAdAlreadyInitializedErrorMessage;
extern const char* kAdCouldNotParseAdRequestErrorMessage;
extern const char* kAdLoadInProgressErrorMessage;
extern const char* kAdUninitializedErrorMessage;

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
template <class T>
SafeFutureHandle<T> CreateFuture(int fn_idx, FutureData* future_data);

// Mark a future as complete.
void CompleteFuture(int error, const char* error_msg,
                    SafeFutureHandle<void> handle, FutureData* future_data);

template <class T>
void CompleteFuture(int error, const char* error_msg,
                    SafeFutureHandle<T> handle, FutureData* future_data,
                    const T& result);

// For calls that aren't asynchronous, create and complete the future at the
// same time.
Future<void> CreateAndCompleteFuture(int fn_idx, int error,
                                     const char* error_msg,
                                     FutureData* future_data);

// For calls that aren't asynchronous, create and complete a future with a
// result at the same time.
Future<LoadAdResult> CreateAndCompleteFutureWithResult(
    int fn_idx, int error, const char* error_msg, FutureData* future_data,
    const LoadAdResult& result);

template <class T>
struct FutureCallbackData {
  FutureData* future_data;
  SafeFutureHandle<T> future_handle;
};

// Constructs a FutureCallbbackData instance to handle operations that return
// void Futures.
FutureCallbackData<void>* CreateVoidFutureCallbackData(int fn_idx,
                                                       FutureData* future_data);

// Constructs a FutureCallbackData instance to handle results from LoadAd.
// requests.
FutureCallbackData<LoadAdResult>* CreateLoadAdResultFutureCallbackData(
    int fn_idx, FutureData* future_data);

// A class that allows access to private/protected Admob structures for Java
// callbacks.  This is achieved via friend relationships with those classes.
class AdmobInternal {
 public:
  static void CompleteLoadAdFuture(
      FutureCallbackData<LoadAdResult>* callback_data, int error_code,
      const std::string& error_message,
      const LoadAdResultInternal& load_ad_result_internal);
};

}  // namespace admob
}  // namespace firebase

#endif  // FIREBASE_ADMOB_SRC_COMMON_ADMOB_COMMON_H_
